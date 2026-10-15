/*
 * perft.c - move-generation correctness harness for Lite Blue (C89).
 *
 * Perft counts leaf nodes of the legal move tree and is the standard way to
 * validate move generation, castling, en passant and promotion. Known node
 * counts are from the Chess Programming Wiki perft results.
 *
 * Usage:
 *   perft                     run the standard suite at default depths
 *   perft -k                  same, verifying the incremental Zobrist key
 *   perft suite N             run the suite with depth overridden to N
 *   perft startpos N          perft from the initial position to depth N
 *   perft fen "<FEN>" N       perft from a FEN to depth N
 *
 * Author: DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "board.h"
#include "movegen.h"

typedef struct {
    const char *name;
    const char *fen;
    int defdepth;
    unsigned long long expect[7];   /* indexed 1..6, 0 = not used */
} Case;

static int verify_keys = 0;

static Case cases[] = {
    { "startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5,
      { 0ULL, 20ULL, 400ULL, 8902ULL, 197281ULL, 4865609ULL, 119060324ULL } },
    { "kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4,
      { 0ULL, 48ULL, 2039ULL, 97862ULL, 4085603ULL, 193690690ULL, 0ULL } },
    { "pos3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5,
      { 0ULL, 14ULL, 191ULL, 2812ULL, 43238ULL, 674624ULL, 11030083ULL } },
    { "pos4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4,
      { 0ULL, 6ULL, 264ULL, 9467ULL, 422333ULL, 15833292ULL, 0ULL } },
    { "pos5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4,
      { 0ULL, 44ULL, 1486ULL, 62379ULL, 2103487ULL, 89941194ULL, 0ULL } },
    { "pos6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4,
      { 0ULL, 46ULL, 2079ULL, 89890ULL, 3894594ULL, 164075551ULL, 0ULL } },
};

static unsigned long long perft(Position *p, int depth)
{
    MoveList ml;
    Undo u;
    unsigned long long nodes = 0;
    int i;

    if (depth == 0)
        return 1ULL;

    gen_legal(p, &ml);
    if (depth == 1)
        return (unsigned long long)ml.n;

    for (i = 0; i < ml.n; i++) {
        make_move(p, ml.m[i], &u);
        if (verify_keys && zobrist_compute(p) != p->key) {
            printf("KEY MISMATCH: %s-%s stored %016llx computed %016llx\n",
                   sq_name(M_FROM(ml.m[i])), sq_name(M_TO(ml.m[i])),
                   p->key, zobrist_compute(p));
            exit(2);
        }
        nodes += perft(p, depth - 1);
        unmake_move(p, ml.m[i], &u);
    }
    return nodes;
}

static void run_case(const Case *c, int depth)
{
    Position p;
    unsigned long long nodes;
    clock_t t0, t1;
    double secs;
    int ok = 1;

    if (pos_fen(&p, c->fen) != 0) {
        printf("%-9s FEN parse error\n", c->name);
        return;
    }

    t0 = clock();
    nodes = perft(&p, depth);
    t1 = clock();
    secs = (double)(t1 - t0) / (double)CLOCKS_PER_SEC;

    if (depth <= 6 && c->expect[depth] != 0ULL) {
        if (nodes != c->expect[depth])
            ok = 0;
        printf("%-9s d%d  %12llu  expected %12llu  %s  (%.2fs)\n",
               c->name, depth, nodes, c->expect[depth],
               ok ? "OK" : "FAIL", secs);
        if (!ok)
            exit(1);
    } else {
        printf("%-9s d%d  %12llu  (no reference)  (%.2fs)\n",
               c->name, depth, nodes, secs);
    }
}

int main(int argc, char **argv)
{
    int i;
    int depth_override = -1;
    const char *fen = NULL;
    int ncases = (int)(sizeof(cases) / sizeof(cases[0]));
    int argi = 1;

    board_init();

    /* strip flags */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-k") == 0)
            verify_keys = 1;
    }

    /* collect positional arguments */
    {
        const char *pos[4];
        int npos = 0;
        for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-k") == 0)
                continue;
            if (npos < 4)
                pos[npos++] = argv[i];
        }
        if (npos == 0) {
            /* default suite */
            for (i = 0; i < ncases; i++)
                run_case(&cases[i], cases[i].defdepth);
        } else if (strcmp(pos[0], "suite") == 0) {
            if (npos >= 2)
                depth_override = atoi(pos[1]);
            for (i = 0; i < ncases; i++) {
                int d = cases[i].defdepth;
                if (depth_override > 0 && depth_override < d)
                    d = depth_override;
                if (depth_override > d)
                    d = depth_override;
                run_case(&cases[i], d);
            }
        } else if (strcmp(pos[0], "startpos") == 0) {
            int d = (npos >= 2) ? atoi(pos[1]) : 5;
            run_case(&cases[0], d);
        } else if (strcmp(pos[0], "fen") == 0 && npos >= 3) {
            Case c;
            c.name = "fen";
            c.fen = pos[1];
            c.defdepth = 0;
            memset(c.expect, 0, sizeof(c.expect));
            fen = pos[1];
            depth_override = atoi(pos[2]);
            /* report leaf counts at every depth up to the requested one */
            {
                Position p;
                int d;
                if (pos_fen(&p, fen) != 0) {
                    printf("FEN parse error\n");
                    return 2;
                }
                for (d = 1; d <= depth_override; d++) {
                    clock_t t0 = clock();
                    unsigned long long nodes;
                    Position q;
                    pos_fen(&q, fen);
                    nodes = perft(&q, d);
                    printf("depth %d  %12llu  (%.2fs)\n", d, nodes,
                           (double)(clock() - t0) / (double)CLOCKS_PER_SEC);
                }
            }
            (void)argi;
            return 0;
        } else {
            printf("usage: perft [-k] [suite N | startpos N | fen \"FEN\" N]\n");
            return 2;
        }
    }

    return 0;
}
