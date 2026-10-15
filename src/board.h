/*
 * board.h - position representation, FEN, Zobrist hashing, make/unmake.
 *
 * Board is a 0x88 mailbox: a 128-entry array where a square is valid iff
 * (sq & 0x88) == 0. Squares are rank * 16 + file with a1 = 0, h8 = 119.
 */
#ifndef LITEBLUE_BOARD_H
#define LITEBLUE_BOARD_H

#include "types.h"

/* Named squares used for castling and home squares. */
#define SQ_A1  0
#define SQ_B1  1
#define SQ_C1  2
#define SQ_D1  3
#define SQ_E1  4
#define SQ_F1  5
#define SQ_G1  6
#define SQ_H1  7
#define SQ_A8  112
#define SQ_B8  113
#define SQ_C8  114
#define SQ_D8  115
#define SQ_E8  116
#define SQ_F8  117
#define SQ_G8  118
#define SQ_H8  119

typedef struct {
    int board[128];      /* piece code, EMPTY or OFFBOARD */
    int side;            /* WHITE or BLACK */
    int castling;        /* CASTLE_* bits */
    int ep;              /* en-passant target square, or -1 */
    int halfmove;        /* halfmove clock for the fifty-move rule */
    int fullmove;        /* fullmove number */
    int king_sq[2];      /* [WHITE], [BLACK] */
    U64 key;             /* Zobrist key */
} Position;

typedef struct {
    int captured;        /* piece removed (EMPTY if none) */
    int castling;
    int ep;
    int halfmove;
    U64 key;
} Undo;

void board_init(void);                     /* build Zobrist tables first */
void pos_start(Position *p);
int  pos_fen(Position *p, const char *fen); /* 0 on success, -1 on error */
void pos_print(const Position *p);
U64  zobrist_compute(const Position *p);    /* from scratch (self-test) */

int  sq_attacked(const Position *p, int sq, int by_side);
void make_move(Position *p, Move m, Undo *u);
void unmake_move(Position *p, Move m, const Undo *u);

/* Null move (pass the turn), used by null-move pruning. */
void make_null(Position *p, Undo *u);
void unmake_null(Position *p, const Undo *u);

/* Format a move as coordinate notation ("e2e4", "e7e8q"). buf >= 6 chars. */
void move_str(Move m, char *buf);

int  on_board(int sq);
const char *sq_name(int sq);                /* returns a static buffer */

#endif /* LITEBLUE_BOARD_H */
