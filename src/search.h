/*
 * search.h - alpha-beta search with iterative deepening for Lite Blue.
 */
#ifndef LITEBLUE_SEARCH_H
#define LITEBLUE_SEARCH_H

#include "board.h"

typedef struct {
    int depth;          /* maximum depth; 0 = none */
    int movetime_ms;    /* fixed movetime; 0 = none */
    int wtime_ms;       /* -1 = unset */
    int btime_ms;
    int winc_ms;
    int binc_ms;
    int infinite;       /* search to the depth cap, ignore the clock */
    int nodes;          /* node limit; 0 = none */
    int silent;         /* suppress UCI info output */
} SearchLimit;

typedef struct {
    U64 nodes;
    int depth;          /* deepest completed iteration */
    int score;          /* side-to-move view, centipawns or mate scale */
    int time_ms;
    Move best;
} SearchResult;

void search_init(void);
void search_set_hash(long entries);
void search_clear(void);

/*
 * Supply the Zobrist keys of all positions played so far (oldest first,
 * including the current position) so the search can detect repetitions.
 */
void search_set_history(const U64 *keys, int n);

Move search_go(Position *p, const SearchLimit *lim, SearchResult *res);

#endif /* LITEBLUE_SEARCH_H */
