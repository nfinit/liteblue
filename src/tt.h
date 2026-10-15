/*
 * tt.h - Zobrist-keyed transposition table for Lite Blue.
 */
#ifndef LITEBLUE_TT_H
#define LITEBLUE_TT_H

#include "board.h"

#define TT_EXACT 1
#define TT_LOWER 2
#define TT_UPPER 3

/* Allocate a table of at most `entries` slots (rounded down to a power of two). */
void tt_init(long entries);

void tt_clear(void);
void tt_new_age(void);                 /* advance the generation (per search) */
long tt_size(void);                    /* slots actually allocated */
long tt_hashfull(void);                /* occupied slots with the current age */

/*
 * Probe. On a hit returns 1 and fills move, score (from the mover's point of
 * view, mate-adjusted), flag and the stored depth. ply is used to undo the
 * mate-distance adjustment.
 */
int  tt_probe(U64 key, Move *move, int *score, int *flag, int *depth, int ply);

void tt_store(U64 key, Move move, int score, int flag, int depth, int ply);

#endif /* LITEBLUE_TT_H */
