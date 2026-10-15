/*
 * types.h - Lite Blue core types, piece codes and move encoding (C89).
 *
 * Lite Blue is a chess engine for obsidian (IBM RS/6000 43P, one PowerPC 604e,
 * AIX 4.3.3), a deliberately single-node answer to Deep Blue. See NOTES.md.
 *
 * Author: DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#ifndef LITEBLUE_TYPES_H
#define LITEBLUE_TYPES_H

typedef unsigned long long U64;

#define WHITE 0
#define BLACK 1

/*
 * Piece codes. The colour is bit 3, so white pieces are 1..6 and black are
 * 9..14; PC_TYPE() takes the low three bits. OFFBOARD marks invalid 0x88
 * squares.
 */
enum {
    EMPTY = 0,
    WP = 1, WN = 2, WB = 3, WR = 4, WQ = 5, WK = 6,
    BP = 9, BN = 10, BB = 11, BR = 12, BQ = 13, BK = 14,
    OFFBOARD = 15
};

#define PC_COLOR(p) (((p) >> 3) & 1)
#define PC_TYPE(p)  ((p) & 7)

/* Piece types as used by promotion (2..5 = N,B,R,Q). */
#define T_PAWN   1
#define T_KNIGHT 2
#define T_BISHOP 3
#define T_ROOK   4
#define T_QUEEN  5
#define T_KING   6

/* Castling-right bits. */
#define CASTLE_WK 1
#define CASTLE_WQ 2
#define CASTLE_BK 4
#define CASTLE_BQ 8

/*
 * A move is packed into an unsigned int:
 *   bits  0-6   from square (0x88)
 *   bits  7-13  to square   (0x88)
 *   bits 14-16  promotion piece type (0 none, 2..5 = N,B,R,Q)
 *   bits 17-21  flags
 */
typedef unsigned int Move;

#define FLAG_CAPTURE 1
#define FLAG_EP      2
#define FLAG_CASTLE  4
#define FLAG_DOUBLE  8
#define FLAG_PROMO   16

#define MK_MOVE(f, t, pr, fl) \
    ((Move)((f) | ((t) << 7) | ((pr) << 14) | ((fl) << 17)))
#define M_FROM(m)  ((int)((m) & 0x7f))
#define M_TO(m)    ((int)(((m) >> 7) & 0x7f))
#define M_PROMO(m) ((int)(((m) >> 14) & 7))
#define M_FLAGS(m) ((int)(((m) >> 17) & 0x1f))

/* Scores. Search returns mate as MATE_SCORE - distance-to-mate. */
#define MATE_SCORE 30000
#define INF_SCORE  32000

#endif /* LITEBLUE_TYPES_H */
