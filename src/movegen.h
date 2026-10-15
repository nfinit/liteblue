/*
 * movegen.h - pseudo-legal and legal move generation (0x88).
 */
#ifndef LITEBLUE_MOVEGEN_H
#define LITEBLUE_MOVEGEN_H

#include "board.h"

/*
 * A legal chess position has at most 218 moves, so 256 slots never overflow;
 * move generation silently stops past this, which cannot happen in practice.
 */
#define MAX_MOVES 256

typedef struct {
    Move m[MAX_MOVES];
    int n;
} MoveList;

/* Pseudo-legal moves for the side to move (king may be left in check). */
int gen_pseudo(const Position *p, MoveList *ml);

/* Pseudo-legal captures and promotions only (for quiescence search). */
int gen_captures(const Position *p, MoveList *ml);

/* Legal moves: pseudo-legal moves filtered so the mover's king is safe. */
int gen_legal(Position *p, MoveList *ml);

/* Parse a coordinate move ("e2e4", "e7e8q") against the legal moves of p.
 * Returns 0 if it is not a legal move. */
Move parse_move(Position *p, const char *s);

#endif /* LITEBLUE_MOVEGEN_H */
