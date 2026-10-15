/*
 * tui.c - a minimal interactive terminal front end for Lite Blue (C89).
 *
 * Type a move in coordinate notation ("e2e4", "e7e8q") and the engine replies.
 * Commands: d (display), undo, new, time <ms>, go, quit.
 *
 * Author: DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "movegen.h"
#include "search.h"

static Position pos;
static U64 hist[1024];
static int histn;
static Move move_list[1024];
static int nmoves;
static int engine_ms = 1500;

static void push_hist(void)
{
    if (histn < 1024)
        hist[histn] = pos.key;
    histn++;
}

static void replay(void)
{
    int i;
    pos_start(&pos);
    histn = 0;
    push_hist();
    for (i = 0; i < nmoves; i++) {
        Undo u;
        make_move(&pos, move_list[i], &u);
        push_hist();
    }
}

static void apply(Move m)
{
    Undo u;
    if (nmoves < 1024)
        move_list[nmoves++] = m;
    make_move(&pos, m, &u);
    push_hist();
}

static void engine_move(void)
{
    SearchLimit lim;
    SearchResult res;
    Move m;
    char buf[8];

    lim.depth = 0;
    lim.movetime_ms = engine_ms;
    lim.wtime_ms = lim.btime_ms = lim.winc_ms = lim.binc_ms = -1;
    lim.infinite = 0;
    lim.nodes = 0;
    lim.silent = 1;

    search_set_history(hist, histn);
    m = search_go(&pos, &lim, &res);
    if (m == 0) {
        printf("(no legal moves)\n");
        return;
    }
    move_str(m, buf);
    printf("engine: %-6s depth %d  score %d  nodes %llu  %d ms\n",
           buf, res.depth, res.score, res.nodes, res.time_ms);
    apply(m);
}

int main(void)
{
    char line[256];

    board_init();
    search_init();
    pos_start(&pos);
    histn = 0;
    push_hist();

    printf("Lite Blue - your move (e2e4), or: d, undo, new, go, time <ms>, quit\n");
    pos_print(&pos);

    while (fgets(line, (int) sizeof(line), stdin) != NULL) {
        char *cmd = strtok(line, " \t\n");
        if (cmd == NULL)
            continue;

        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
            break;
        } else if (strcmp(cmd, "d") == 0) {
            pos_print(&pos);
        } else if (strcmp(cmd, "new") == 0) {
            nmoves = 0;
            replay();
            pos_print(&pos);
        } else if (strcmp(cmd, "undo") == 0) {
            if (nmoves >= 2)
                nmoves -= 2;
            else if (nmoves == 1)
                nmoves -= 1;
            replay();
            pos_print(&pos);
        } else if (strcmp(cmd, "time") == 0) {
            char *t = strtok(NULL, " \t\n");
            if (t)
                engine_ms = atoi(t);
            printf("engine time: %d ms\n", engine_ms);
        } else if (strcmp(cmd, "go") == 0) {
            engine_move();
            pos_print(&pos);
        } else {
            Move m = parse_move(&pos, cmd);
            if (m == 0) {
                printf("illegal or unknown move\n");
                continue;
            }
            apply(m);
            pos_print(&pos);
            engine_move();
            pos_print(&pos);
        }
    }
    return 0;
}
