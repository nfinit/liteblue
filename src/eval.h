/*
 * eval.h - static evaluation for Lite Blue.
 */
#ifndef LITEBLUE_EVAL_H
#define LITEBLUE_EVAL_H

#include "board.h"

/*
 * Evaluate a position in centipawns from the point of view of the side to
 * move (so negamax can use it directly). Positive is good for the mover.
 */
int eval(const Position *p);

#endif /* LITEBLUE_EVAL_H */
