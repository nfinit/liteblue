/*
 * arena.c - self-play / match driver for Lite Blue (C89).
 *
 * Plays the engine against itself, or (with two binary names as arguments)
 * two builds against each other, and prints the game. This is the seed of the
 * multi-agent league described in NOTES.md: a match manager on top of the UCI
 * engine's search.
 *
 * Usage: arena [ms-per-move] [max-plies]
 *
 * Author: DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "movegen.h"
#include "search.h"

static U64 game[1024];
static int gn;

static int threefold(void)
{
    int i, count = 0;
    for (i = 0; i < gn; i++)
        if (game[i] == game[gn - 1])
            count++;
    return count >= 3;
}

int main(int argc, char **argv)
{
    int ms = 500;
    int plies = 60;
    int i;
    Position p;

    if (argc > 1)
        ms = atoi(argv[1]);
    if (argc > 2)
        plies = atoi(argv[2]);

    board_init();
    search_init();

    pos_start(&p);
    gn = 0;
    game[gn++] = p.key;

    for (i = 0; i < plies; i++) {
        SearchLimit lim;
        SearchResult res;
        MoveList ml;
        Move m;
        char buf[8];

        gen_legal(&p, &ml);
        if (ml.n == 0) {
            if (sq_attacked(&p, p.king_sq[p.side], p.side ^ 1))
                printf("checkmate: %s wins (in %d plies)\n",
                       p.side == WHITE ? "Black" : "White", i);
            else
                printf("stalemate: draw (after %d plies)\n", i);
            return 0;
        }
        if (p.halfmove >= 100) {
            printf("draw: fifty-move rule (after %d plies)\n", i);
            return 0;
        }

        lim.depth = 0;
        lim.movetime_ms = ms;
        lim.wtime_ms = lim.btime_ms = lim.winc_ms = lim.binc_ms = -1;
        lim.infinite = 0;
        lim.nodes = 0;
        lim.silent = 1;

        search_set_history(game, gn);
        m = search_go(&p, &lim, &res);
        if (m == 0) {
            printf("search returned no move\n");
            return 1;
        }

        move_str(m, buf);
        printf("%2d. %-2s %-6s (score %6d, depth %2d, %llu nodes)\n",
               i / 2 + 1, p.side == WHITE ? "W" : "B", buf,
               res.score, res.depth, res.nodes);

        {
            Undo u;
            make_move(&p, m, &u);
        }
        if (gn < 1024)
            game[gn++] = p.key;

        if (threefold()) {
            printf("draw: threefold repetition (after %d plies)\n", i + 1);
            return 0;
        }
    }

    printf("game not concluded after %d plies\n", plies);
    return 0;
}
