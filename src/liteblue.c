/*
 * liteblue.c - the UCI front end for Lite Blue (C89).
 *
 * This is the agent-facing interface: any UCI harness, tournament manager or
 * script can drive the engine over stdin/stdout. See NOTES.md for the
 * multi-agent plan this exists to support.
 *
 * Commands: uci, isready, ucinewgame, setoption, position, go, stop, quit,
 * plus two conveniences: "d" (display the board) and "perft N".
 *
 * Author: DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "movegen.h"
#include "search.h"
#include "version.h"

static Position pos;
static U64 hist[1024];
static int histn;

static void push_hist(void)
{
    if (histn < 1024)
        hist[histn] = pos.key;
    histn++;
}

static unsigned long long perft(Position *p, int depth)
{
    MoveList ml;
    Undo u;
    int i;
    unsigned long long n = 0;

    if (depth == 0)
        return 1ULL;
    gen_legal(p, &ml);
    if (depth == 1)
        return (unsigned long long) ml.n;
    for (i = 0; i < ml.n; i++) {
        make_move(p, ml.m[i], &u);
        n += perft(p, depth - 1);
        unmake_move(p, ml.m[i], &u);
    }
    return n;
}

static void cmd_position(char *rest)
{
    char *tok = strtok(rest, " \t\n");

    histn = 0;

    if (tok == NULL) {
        pos_start(&pos);
    } else if (strcmp(tok, "startpos") == 0) {
        pos_start(&pos);
        tok = strtok(NULL, " \t\n");
    } else if (strcmp(tok, "fen") == 0) {
        char fen[256];
        char *t;
        int nf = 0;
        fen[0] = '\0';
        while ((t = strtok(NULL, " \t\n")) != NULL && strcmp(t, "moves") != 0) {
            if (nf > 0)
                strcat(fen, " ");
            strcat(fen, t);
            nf++;
        }
        if (pos_fen(&pos, fen) != 0)
            pos_start(&pos);
        tok = t;
    } else {
        pos_start(&pos);
    }

    push_hist();

    if (tok != NULL && strcmp(tok, "moves") == 0) {
        char *mv;
        while ((mv = strtok(NULL, " \t\n")) != NULL) {
            Move m = parse_move(&pos, mv);
            Undo u;
            if (m == 0)
                break;
            make_move(&pos, m, &u);
            push_hist();
        }
    }
}

static void cmd_go(char *rest)
{
    SearchLimit lim;
    SearchResult res;
    char *tok;
    Move best;
    char buf[8];

    search_limit_default(&lim);

    tok = strtok(rest, " \t\n");
    while (tok != NULL) {
        if (strcmp(tok, "wtime") == 0) {
            tok = strtok(NULL, " \t\n"); if (tok) lim.wtime_ms = atoi(tok);
        } else if (strcmp(tok, "btime") == 0) {
            tok = strtok(NULL, " \t\n"); if (tok) lim.btime_ms = atoi(tok);
        } else if (strcmp(tok, "winc") == 0) {
            tok = strtok(NULL, " \t\n"); if (tok) lim.winc_ms = atoi(tok);
        } else if (strcmp(tok, "binc") == 0) {
            tok = strtok(NULL, " \t\n"); if (tok) lim.binc_ms = atoi(tok);
        } else if (strcmp(tok, "movetime") == 0) {
            tok = strtok(NULL, " \t\n"); if (tok) lim.movetime_ms = atoi(tok);
        } else if (strcmp(tok, "depth") == 0) {
            tok = strtok(NULL, " \t\n"); if (tok) lim.depth = atoi(tok);
        } else if (strcmp(tok, "nodes") == 0) {
            tok = strtok(NULL, " \t\n"); if (tok) lim.nodes = atoi(tok);
        } else if (strcmp(tok, "infinite") == 0) {
            lim.infinite = 1;
        }
        tok = strtok(NULL, " \t\n");
    }

    search_set_history(hist, histn);
    best = search_go(&pos, &lim, &res);

    if (best == 0)
        printf("bestmove 0000\n");
    else {
        move_str(best, buf);
        printf("bestmove %s\n", buf);
    }
    fflush(stdout);
}

static void cmd_setoption(char *rest)
{
    char *p;
    long mb;

    if (strstr(rest, "name") == NULL)
        return;
    p = strstr(rest, "value");
    if (p == NULL)
        return;
    mb = atol(p + 5);
    if (mb < 1)
        mb = 1;
    if (mb > 64)
        mb = 64;
    search_set_hash(mb * 65536L);        /* 65536 entries = 1 MiB */
}

static void cmd_bench(char *rest)
{
    int depth = atoi(rest);
    SearchLimit lim;
    SearchResult res;
    Position q;
    U64 h[1];
    long nps;

    if (depth <= 0)
        depth = 6;

    search_limit_default(&lim);
    lim.depth = depth;
    lim.silent = 1;

    pos_start(&q);
    h[0] = q.key;
    search_set_history(h, 1);
    search_go(&q, &lim, &res);

    nps = (res.time_ms > 0)
        ? (long)((double) res.nodes * 1000.0 / (double) res.time_ms)
        : (long) res.nodes;
    printf("bench depth %d nodes %llu time %d ms nps %ld\n",
           res.depth, res.nodes, res.time_ms, nps);
    fflush(stdout);
}

int main(void)
{
    char line[4096];

    board_init();
    search_init();
    pos_start(&pos);
    histn = 0;
    push_hist();

    while (fgets(line, (int) sizeof(line), stdin) != NULL) {
        if (strncmp(line, "uci", 3) == 0 &&
            (line[3] == '\n' || line[3] == ' ' || line[3] == '\0')) {
            printf("id name Lite Blue %s\n", LITEBLUE_VERSION);
            printf("id author DeepSeek (deepseek-v4.1-flash)\n");
            printf("option name Hash type spin default 1 min 1 max 64\n");
            printf("uciok\n");
        } else if (strncmp(line, "isready", 7) == 0) {
            printf("readyok\n");
        } else if (strncmp(line, "ucinewgame", 10) == 0) {
            search_clear();
            pos_start(&pos);
            histn = 0;
            push_hist();
        } else if (strncmp(line, "position", 8) == 0) {
            cmd_position(line + 8);
        } else if (strncmp(line, "go", 2) == 0 &&
                   (line[2] == '\n' || line[2] == ' ' || line[2] == '\0')) {
            cmd_go(line + 2);
        } else if (strncmp(line, "setoption", 9) == 0) {
            cmd_setoption(line + 9);
        } else if (strncmp(line, "stop", 4) == 0) {
            /* synchronous search: a stop between searches is a no-op */
        } else if (strncmp(line, "quit", 4) == 0) {
            break;
        } else if (line[0] == 'd' && (line[1] == '\n' || line[1] == ' ')) {
            pos_print(&pos);
        } else if (strncmp(line, "perft", 5) == 0) {
            int d = atoi(line + 5);
            printf("nodes %llu\n", perft(&pos, d));
        } else if (strncmp(line, "bench", 5) == 0) {
            cmd_bench(line + 5);
        }
        fflush(stdout);
    }
    return 0;
}
