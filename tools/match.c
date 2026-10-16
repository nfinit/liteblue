/*
 * match.c - play ENGINE_A against ENGINE_B over UCI, to measure engine changes.
 *
 * Each engine is a persistent UCI process (one per side for the whole match),
 * so moves cost only the search, not a fresh process. The runner owns the board
 * (it links board.c/movegen.c), applies the engines' moves, and adjudicates
 * mate, stalemate, the fifty-move rule and threefold repetition. Each opening is
 * played twice with colours swapped.
 *
 * Usage: match ENGINE_A ENGINE_B [depthA] [depthB] [max_plies] [book]
 *   book: a file of opening positions, one FEN per line (e.g. a UHO subset).
 *   Without it a small built-in list of opening move sequences is used.
 *
 * DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "board.h"
#include "movegen.h"

#define MAXLINE 512
#define MAXKEYS 1024
#define MAXOPEN 1024
#define OPENLEN 160

static const char *builtin[] = {
    "e2e4 e7e5", "d2d4 d5", "e2e4 c7c5", "d2d4 g8f6",
    "g1f3 d7d5", "e2e4 e7e6", "c2c4 e7e5", "e2e4 g8f6",
    "e2e4 c7c6", "d2d4 d7d5 c2c4 e7e6", "d2d4 d7d5 c2c4 c7c6",
    "e2e4 e7e5 g1f3 b8c6", "e2e4 c7c5 g1f3 d7d6", "e2e4 e7e5 g1f3 g8f6",
    "d2d4 g8f6 c2c4 e7e6", "e2e4 e7e5 b1c3",
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6", "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5",
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6", "d2d4 g8f6 c2c4 g7g6",
    "d2d4 g8f6 c2c4 e7e6 g1f3 b7b6", "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4",
    "e2e4 c7c5 g1f3 b8c6 d2d4 c5d4", "e2e4 e7e6 d2d4 d7d5 b1c3 g8f6",
    "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4", "e2e4 d7d5 e4d5 d8d5",
    "e2e4 g8f6 e4e5 f6d5", "d2d4 f7f5", "e2e4 e7e5 f2f4",
    "d2d4 d7d5 c2c4 d5c4", "e2e4 e7e5 g1f3 b8c6 d2d4 e5d4",
    "c2c4 e7e5 b1c3 g8f6"
};

typedef struct {
    FILE *in;
    FILE *out;
    pid_t pid;
} Engine;

static char openbuf[MAXOPEN][OPENLEN];
static const char *openings[MAXOPEN];
static int nopen = 0;
static int g_ply = 0;
static int g_nodes = 0;      /* if > 0, play at fixed nodes per move */

static void load_builtin(void)
{
    int i, n = (int) (sizeof builtin / sizeof builtin[0]);
    for (i = 0; i < n && i < MAXOPEN; i++) {
        strncpy(openbuf[i], builtin[i], OPENLEN - 1);
        openbuf[i][OPENLEN - 1] = '\0';
        openings[i] = openbuf[i];
    }
    nopen = n;
}

static int load_book(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[MAXLINE];
    int n = 0;

    if (f == NULL)
        return -1;
    while (fgets(line, sizeof line, f) != NULL && n < MAXOPEN) {
        char *t;
        char fen[OPENLEN];
        int tok = 0;
        fen[0] = '\0';
        for (t = strtok(line, " \t\r\n"); t != NULL && tok < 6;
             t = strtok(NULL, " \t\r\n")) {
            if (tok)
                strcat(fen, " ");
            strcat(fen, t);
            tok++;
        }
        if (tok >= 4) {
            strncpy(openbuf[n], fen, OPENLEN - 1);
            openbuf[n][OPENLEN - 1] = '\0';
            openings[n] = openbuf[n];
            n++;
        }
    }
    fclose(f);
    nopen = n;
    return n;
}

/* --- persistent engine sessions ---------------------------------------- */

static int eng_open(Engine *e, const char *path)
{
    int pin[2], pout[2];

    if (pipe(pin) != 0 || pipe(pout) != 0)
        return -1;
    e->pid = fork();
    if (e->pid < 0)
        return -1;
    if (e->pid == 0) {
        dup2(pin[0], 0);
        dup2(pout[1], 1);
        close(pin[0]); close(pin[1]);
        close(pout[0]); close(pout[1]);
        execl(path, path, (char *) NULL);
        _exit(127);
    }
    close(pin[0]);
    close(pout[1]);
    e->in = fdopen(pin[1], "w");
    e->out = fdopen(pout[0], "r");
    if (e->in == NULL || e->out == NULL)
        return -1;
    /* Do not let a later engine's fork+exec inherit these pipe ends, or
     * closing them would never give this engine EOF. */
    fcntl(fileno(e->in), F_SETFD, FD_CLOEXEC);
    fcntl(fileno(e->out), F_SETFD, FD_CLOEXEC);
    return 0;
}

static void eng_send(Engine *e, const char *s)
{
    if (getenv("MATCH_DEBUG"))
        fprintf(stderr, ">> %s\n", s);
    fputs(s, e->in);
    fputc('\n', e->in);
    fflush(e->in);
}

/* Read lines until one begins with tag; find the first token after the tag. */
static int eng_wait(Engine *e, const char *tag, char *out, int outsz)
{
    char line[MAXLINE];
    size_t tl = strlen(tag);

    while (fgets(line, sizeof line, e->out) != NULL) {
        if (getenv("MATCH_DEBUG"))
            fprintf(stderr, "<< %s", line);
        if (strncmp(line, tag, tl) == 0) {
            if (out != NULL) {
                char *q = line + tl;
                int i = 0;
                while (*q != '\0' && *q != ' ' && *q != '\n' && i < outsz - 1)
                    out[i++] = *q++;
                out[i] = '\0';
            }
            return 1;
        }
    }
    return 0;
}

static int eng_init(Engine *e)
{
    eng_send(e, "uci");
    if (!eng_wait(e, "uciok", NULL, 0))
        return -1;
    eng_send(e, "isready");
    return eng_wait(e, "readyok", NULL, 0) ? 0 : -1;
}

static void eng_newgame(Engine *e)
{
    eng_send(e, "ucinewgame");
    eng_send(e, "isready");
    eng_wait(e, "readyok", NULL, 0);
}

static int eng_move(Engine *e, const char *poscmd, const char *moves, int depth,
                    char *out, int outsz)
{
    char buf[8192];

    sprintf(buf, "position %s%s%s", poscmd, moves[0] ? " " : "", moves);
    eng_send(e, buf);
    if (g_nodes > 0)
        sprintf(buf, "go nodes %d", g_nodes);
    else
        sprintf(buf, "go depth %d", depth);
    eng_send(e, buf);
    return eng_wait(e, "bestmove ", out, outsz);
}

static void eng_close(Engine *e)
{
    if (e->in != NULL) {
        fclose(e->in);
        e->in = NULL;
    }
    if (e->out != NULL) {
        fclose(e->out);
        e->out = NULL;
    }
    waitpid(e->pid, NULL, 0);
}

/* --- the game ---------------------------------------------------------- */

static int threefold(const U64 *keys, int n)
{
    int i, count = 0;
    for (i = 0; i < n; i++)
        if (keys[i] == keys[n - 1])
            count++;
    return count >= 3;
}

/* +1 A wins, -1 A loses, 0 draw, -2 the opening could not be set up. */
static int play_game(Engine *ea, Engine *eb, const char *aname, const char *bname,
                     int a_is_white, const char *opening,
                     int depthA, int depthB, int maxply)
{
    Position p;
    Undo u;
    MoveList ml;
    Move m;
    char poscmd[OPENLEN + 16];
    char moves[4096];
    char tmp[OPENLEN];
    char buf[16];
    char *tok;
    U64 keys[MAXKEYS];
    int keyn = 0, ply = 0;

    (void) aname;
    (void) bname;

    if (strchr(opening, '/') != NULL) {
        if (pos_fen(&p, opening) != 0)
            return -2;
        sprintf(poscmd, "fen %s moves", opening);
    } else {
        pos_start(&p);
        sprintf(poscmd, "startpos moves %s", opening);
        strncpy(tmp, opening, OPENLEN - 1);
        tmp[OPENLEN - 1] = '\0';
        for (tok = strtok(tmp, " "); tok != NULL; tok = strtok(NULL, " ")) {
            m = parse_move(&p, tok);
            if (m == 0)
                return -2;
            make_move(&p, m, &u);
        }
    }
    keys[keyn++] = p.key;
    moves[0] = '\0';
    g_ply = 0;

    eng_newgame(ea);
    eng_newgame(eb);

    while (ply < maxply) {
        int mover_is_a;
        Engine *eng;

        gen_legal(&p, &ml);
        if (ml.n == 0) {
            mover_is_a = ((p.side == WHITE) == (a_is_white != 0));
            if (!sq_attacked(&p, p.king_sq[p.side], p.side ^ 1))
                return 0;
            return mover_is_a ? -1 : 1;
        }
        if (p.halfmove >= 100)
            return 0;
        if (threefold(keys, keyn))
            return 0;

        mover_is_a = ((p.side == WHITE) == (a_is_white != 0));
        eng = mover_is_a ? ea : eb;
        if (!eng_move(eng, poscmd, moves, mover_is_a ? depthA : depthB,
                      buf, sizeof buf))
            return 0;
        if (strcmp(buf, "0000") == 0)
            return 0;

        m = parse_move(&p, buf);
        if (m == 0)
            return mover_is_a ? -1 : 1;
        make_move(&p, m, &u);
        if (keyn < MAXKEYS)
            keys[keyn++] = p.key;

        if (moves[0] != '\0')
            strcat(moves, " ");
        strcat(moves, buf);
        ply++;
        g_ply = ply;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *a, *b;
    int depthA = 8, depthB = 8, maxply = 200;
    int i, s, games = 0, winsA = 0, winsB = 0, draws = 0, skipped = 0;
    double score;
    Engine ea, eb;

    if (argc < 3) {
        fprintf(stderr,
                "usage: match ENGINE_A ENGINE_B [depthA] [depthB] [max_plies] [book]\n");
        return 2;
    }
    a = argv[1];
    b = argv[2];
    if (argc > 3)
        depthA = atoi(argv[3]);
    if (argc > 4)
        depthB = atoi(argv[4]);
    if (argc > 5)
        maxply = atoi(argv[5]);
    if (argc > 7)
        g_nodes = atoi(argv[7]);

    board_init();
    if (argc > 6) {
        if (load_book(argv[6]) <= 0) {
            fprintf(stderr, "no openings read from %s\n", argv[6]);
            return 2;
        }
    } else {
        load_builtin();
    }

    if (eng_open(&ea, a) != 0 || eng_init(&ea) != 0) {
        fprintf(stderr, "cannot start engine A: %s\n", a);
        return 2;
    }
    if (eng_open(&eb, b) != 0 || eng_init(&eb) != 0) {
        fprintf(stderr, "cannot start engine B: %s\n", b);
        return 2;
    }

    printf("A = %s  (depth %d)\nB = %s  (depth %d)\nopenings = %d  max_plies = %d%s%d\n\n",
           a, depthA, b, depthB, nopen, maxply,
           g_nodes > 0 ? "  nodes/move = " : "  depth mode", g_nodes);

    for (i = 0; i < nopen; i++) {
        for (s = 0; s < 2; s++) {
            int r = play_game(&ea, &eb, a, b, s == 0, openings[i],
                              depthA, depthB, maxply);
            if (r == -2) {
                skipped++;
                continue;
            }
            games++;
            if (r > 0)
                winsA++;
            else if (r < 0)
                winsB++;
            else
                draws++;
            printf("%4d/%d %s %s (%d plies)\n", i + 1, nopen, s ? "b" : "w",
                   r > 0 ? "A" : r < 0 ? "B" : "=", g_ply);
            fflush(stdout);
        }
    }

    eng_close(&ea);
    eng_close(&eb);

    score = ((double) winsA + 0.5 * (double) draws) / (double) games;
    printf("\nA %d  B %d  D %d   (%d games%s)\n", winsA, winsB, draws, games,
           skipped ? ", some skipped" : "");
    printf("A score %.3f", score);
    if (score > 0.0 && score < 1.0)
        printf("   Elo(A-B) %+.0f", -400.0 * log10(1.0 / score - 1.0));
    printf("\n");
    return 0;
}
