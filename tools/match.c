/*
 * match.c - play ENGINE_A against ENGINE_B over UCI, to measure engine changes.
 *
 * The engine is stateless per "position ... go ...", so every move is a fresh
 * one-shot UCI process: no coprocesses, works anywhere popen() does. Both
 * engines play every opening once with each colour. The runner owns the board
 * (it links board.c/movegen.c), applies the engines' moves, and adjudicates
 * mate, stalemate, the fifty-move rule and threefold repetition.
 *
 * Usage: match ENGINE_A ENGINE_B [depth] [max_plies]
 *
 * DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "board.h"
#include "movegen.h"

#define MAXLINE 512
#define MAXKEYS 1024

static const char *openings[] = {
    "e2e4 e7e5", "d2d4 d5", "e2e4 c7c5", "d2d4 g8f6",
    "g1f3 d7d5", "e2e4 e7e6", "c2c4 e7e5", "e2e4 g8f6",
    "e2e4 c7c6", "d2d4 d7d5 c2c4 e7e6", "d2d4 d7d5 c2c4 c7c6",
    "e2e4 e7e5 g1f3 b8c6", "e2e4 c7c5 g1f3 d7d6", "e2e4 e7e5 g1f3 g8f6",
    "d2d4 g8f6 c2c4 e7e6", "e2e4 e7e5 b1c3"
};
#define NOPEN ((int) (sizeof openings / sizeof openings[0]))

/* Ask one engine for a move from the start position plus a move list. */
static int ask(const char *engine, const char *moves, int depth,
               char *out, int outsz)
{
    char cmd[8192];
    char line[MAXLINE];
    FILE *fp;
    int got = 0;

    sprintf(cmd,
            "printf 'position startpos moves %s\\ngo depth %d\\nquit\\n' | %s",
            moves, depth, engine);
    fp = popen(cmd, "r");
    if (fp == NULL)
        return 0;
    while (fgets(line, sizeof line, fp) != NULL) {
        if (strncmp(line, "bestmove ", 9) == 0) {
            char *q = line + 9;
            int i = 0;
            while (*q != '\0' && *q != ' ' && *q != '\n' && i < outsz - 1)
                out[i++] = *q++;
            out[i] = '\0';
            got = 1;
            break;
        }
    }
    pclose(fp);
    return got;
}

static int threefold(const U64 *keys, int n)
{
    int i, count = 0;
    for (i = 0; i < n; i++)
        if (keys[i] == keys[n - 1])
            count++;
    return count >= 3;
}

/*
 * Returns +1 if A wins, -1 if A loses, 0 for a draw. Both engines see the same
 * opening; a_is_white says which colour A plays this game.
 */
static int play_game(const char *a, const char *b, int a_is_white,
                     const char *opening, int depthA, int depthB, int maxply)
{
    Position p;
    Undo u;
    MoveList ml;
    Move m;
    char moves[4096];
    char tmp[256];
    char buf[16];
    char *tok;
    U64 keys[MAXKEYS];
    int keyn = 0, ply = 0;

    pos_start(&p);
    keys[keyn++] = p.key;
    moves[0] = '\0';
    strncpy(tmp, opening, sizeof tmp - 1);
    tmp[sizeof tmp - 1] = '\0';
    for (tok = strtok(tmp, " "); tok != NULL; tok = strtok(NULL, " ")) {
        m = parse_move(&p, tok);
        if (m == 0)
            return 0;
        make_move(&p, m, &u);
        if (keyn < MAXKEYS)
            keys[keyn++] = p.key;
        if (moves[0] != '\0')
            strcat(moves, " ");
        strcat(moves, tok);
    }

    while (ply < maxply) {
        int mover_is_a, r;
        const char *eng;

        gen_legal(&p, &ml);
        if (ml.n == 0) {
            mover_is_a = ((p.side == WHITE) == (a_is_white != 0));
            if (!sq_attacked(&p, p.king_sq[p.side], p.side ^ 1))
                return 0;                       /* stalemate */
            return mover_is_a ? -1 : 1;          /* checkmate */
        }
        if (p.halfmove >= 100)
            return 0;
        if (threefold(keys, keyn))
            return 0;

        mover_is_a = ((p.side == WHITE) == (a_is_white != 0));
        eng = mover_is_a ? a : b;
        if (!ask(eng, moves, mover_is_a ? depthA : depthB, buf, sizeof buf))
            return 0;
        if (strcmp(buf, "0000") == 0)
            return 0;

        m = parse_move(&p, buf);
        if (m == 0)
            return mover_is_a ? -1 : 1;          /* illegal move: forfeit */
        make_move(&p, m, &u);
        if (keyn < MAXKEYS)
            keys[keyn++] = p.key;

        if (moves[0] != '\0')
            strcat(moves, " ");
        strcat(moves, buf);
        ply++;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *a, *b;
    int depthA = 8, depthB = 8, maxply = 200;
    int i, s, games = 0, winsA = 0, winsB = 0, draws = 0;
    double score;

    if (argc < 3) {
        fprintf(stderr, "usage: match ENGINE_A ENGINE_B [depthA] [depthB] [max_plies]\n");
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

    board_init();
    printf("A = %s  (depth %d)\nB = %s  (depth %d)\nmax_plies = %d\n\n",
           a, depthA, b, depthB, maxply);

    for (i = 0; i < NOPEN; i++) {
        for (s = 0; s < 2; s++) {
            int r = play_game(a, b, s == 0, openings[i], depthA, depthB, maxply);
            games++;
            if (r > 0) {
                winsA++;
                printf("%-26s A wins%s\n", openings[i], s ? "  (A=black)" : "");
            } else if (r < 0) {
                winsB++;
                printf("%-26s B wins%s\n", openings[i], s ? "  (A=black)" : "");
            } else {
                draws++;
                printf("%-26s draw%s\n", openings[i], s ? "  (A=black)" : "");
            }
            fflush(stdout);
        }
    }

    score = ((double) winsA + 0.5 * (double) draws) / (double) games;
    printf("\nA %d  B %d  D %d   (%d games)\n", winsA, winsB, draws, games);
    printf("A score %.3f", score);
    if (score > 0.0 && score < 1.0)
        printf("   Elo(A-B) %+.0f", -400.0 * log10(1.0 / score - 1.0));
    printf("\n");
    return 0;
}
