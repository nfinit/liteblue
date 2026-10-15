/*
 * movegen.c - 0x88 move generation for Lite Blue (C89).
 *
 * gen_moves() is the common engine; gen_pseudo() asks for all moves and
 * gen_captures() asks only for captures and promotions (used by quiescence).
 *
 * Author: DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include "movegen.h"

static const int KN[8] = { -33, -31, -18, -14, 14, 18, 31, 33 };
static const int KG[8] = { -17, -16, -15, -1, 1, 15, 16, 17 };
static const int BD[4] = { -17, -15, 15, 17 };
static const int RD[4] = { -16, -1, 1, 16 };

static void add(MoveList *ml, int from, int to, int promo, int flags)
{
    if (ml->n < MAX_MOVES)
        ml->m[ml->n++] = MK_MOVE(from, to, promo, flags);
}

static void add_promotions(MoveList *ml, int from, int to, int cap)
{
    int base = cap ? (FLAG_PROMO | FLAG_CAPTURE) : FLAG_PROMO;
    add(ml, from, to, T_QUEEN, base);
    add(ml, from, to, T_ROOK, base);
    add(ml, from, to, T_BISHOP, base);
    add(ml, from, to, T_KNIGHT, base);
}

static int gen_moves(const Position *p, MoveList *ml, int captures_only)
{
    int us = p->side;
    int sq, i, t;

    ml->n = 0;

    for (sq = 0; sq < 128; sq++) {
        int pc, type;
        if (!on_board(sq))
            continue;
        pc = p->board[sq];
        if (pc == EMPTY || PC_COLOR(pc) != us)
            continue;
        type = PC_TYPE(pc);

        switch (type) {
        case T_PAWN: {
            int fwd = (us == WHITE) ? 16 : -16;
            int start = (us == WHITE) ? 1 : 6;
            int promo_rank = (us == WHITE) ? 6 : 1;
            int rk = sq >> 4;
            int caps[2];

            t = sq + fwd;
            if (on_board(t) && p->board[t] == EMPTY) {
                if (rk == promo_rank) {
                    add_promotions(ml, sq, t, 0);
                } else if (!captures_only) {
                    add(ml, sq, t, 0, 0);
                    if (rk == start) {
                        int t2 = sq + 2 * fwd;
                        if (p->board[t2] == EMPTY)
                            add(ml, sq, t2, 0, FLAG_DOUBLE);
                    }
                }
            }

            caps[0] = fwd - 1;
            caps[1] = fwd + 1;
            for (i = 0; i < 2; i++) {
                t = sq + caps[i];
                if (!on_board(t))
                    continue;
                if (p->board[t] != EMPTY && PC_COLOR(p->board[t]) != us) {
                    if (rk == promo_rank)
                        add_promotions(ml, sq, t, 1);
                    else
                        add(ml, sq, t, 0, FLAG_CAPTURE);
                } else if (p->ep >= 0 && t == p->ep) {
                    add(ml, sq, t, 0, FLAG_EP | FLAG_CAPTURE);
                }
            }
            break;
        }

        case T_KNIGHT:
            for (i = 0; i < 8; i++) {
                t = sq + KN[i];
                if (!on_board(t))
                    continue;
                if (p->board[t] == EMPTY) {
                    if (!captures_only)
                        add(ml, sq, t, 0, 0);
                } else if (PC_COLOR(p->board[t]) != us) {
                    add(ml, sq, t, 0, FLAG_CAPTURE);
                }
            }
            break;

        case T_BISHOP:
        case T_ROOK:
        case T_QUEEN: {
            const int *off;
            int noff;
            if (type == T_BISHOP) { off = BD; noff = 4; }
            else if (type == T_ROOK) { off = RD; noff = 4; }
            else { off = KG; noff = 8; }

            for (i = 0; i < noff; i++) {
                t = sq + off[i];
                while (on_board(t)) {
                    if (p->board[t] == EMPTY) {
                        if (!captures_only)
                            add(ml, sq, t, 0, 0);
                    } else {
                        if (PC_COLOR(p->board[t]) != us)
                            add(ml, sq, t, 0, FLAG_CAPTURE);
                        break;
                    }
                    t += off[i];
                }
            }
            break;
        }

        case T_KING:
            for (i = 0; i < 8; i++) {
                t = sq + KG[i];
                if (!on_board(t))
                    continue;
                if (p->board[t] == EMPTY) {
                    if (!captures_only)
                        add(ml, sq, t, 0, 0);
                } else if (PC_COLOR(p->board[t]) != us) {
                    add(ml, sq, t, 0, FLAG_CAPTURE);
                }
            }

            if (!captures_only && us == WHITE && sq == SQ_E1) {
                if ((p->castling & CASTLE_WK) &&
                    p->board[SQ_F1] == EMPTY && p->board[SQ_G1] == EMPTY &&
                    !sq_attacked(p, SQ_E1, BLACK) &&
                    !sq_attacked(p, SQ_F1, BLACK) &&
                    !sq_attacked(p, SQ_G1, BLACK))
                    add(ml, SQ_E1, SQ_G1, 0, FLAG_CASTLE);
                if ((p->castling & CASTLE_WQ) &&
                    p->board[SQ_D1] == EMPTY && p->board[SQ_C1] == EMPTY &&
                    p->board[SQ_B1] == EMPTY &&
                    !sq_attacked(p, SQ_E1, BLACK) &&
                    !sq_attacked(p, SQ_D1, BLACK) &&
                    !sq_attacked(p, SQ_C1, BLACK))
                    add(ml, SQ_E1, SQ_C1, 0, FLAG_CASTLE);
            } else if (!captures_only && us == BLACK && sq == SQ_E8) {
                if ((p->castling & CASTLE_BK) &&
                    p->board[SQ_F8] == EMPTY && p->board[SQ_G8] == EMPTY &&
                    !sq_attacked(p, SQ_E8, WHITE) &&
                    !sq_attacked(p, SQ_F8, WHITE) &&
                    !sq_attacked(p, SQ_G8, WHITE))
                    add(ml, SQ_E8, SQ_G8, 0, FLAG_CASTLE);
                if ((p->castling & CASTLE_BQ) &&
                    p->board[SQ_D8] == EMPTY && p->board[SQ_C8] == EMPTY &&
                    p->board[SQ_B8] == EMPTY &&
                    !sq_attacked(p, SQ_E8, WHITE) &&
                    !sq_attacked(p, SQ_D8, WHITE) &&
                    !sq_attacked(p, SQ_C8, WHITE))
                    add(ml, SQ_E8, SQ_C8, 0, FLAG_CASTLE);
            }
            break;

        default:
            break;
        }
    }

    return ml->n;
}

int gen_pseudo(const Position *p, MoveList *ml)
{
    return gen_moves(p, ml, 0);
}

int gen_captures(const Position *p, MoveList *ml)
{
    return gen_moves(p, ml, 1);
}

int gen_legal(Position *p, MoveList *ml)
{
    MoveList tmp;
    Undo u;
    int us = p->side;
    int i, n;

    n = gen_pseudo(p, &tmp);
    ml->n = 0;
    for (i = 0; i < n; i++) {
        make_move(p, tmp.m[i], &u);
        if (!sq_attacked(p, p->king_sq[us], p->side))
            ml->m[ml->n++] = tmp.m[i];
        unmake_move(p, tmp.m[i], &u);
    }
    return ml->n;
}

Move parse_move(Position *p, const char *s)
{
    MoveList ml;
    int i, from, to, promo = 0;

    if (s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8')
        return 0;
    if (s[2] < 'a' || s[2] > 'h' || s[3] < '1' || s[3] > '8')
        return 0;

    from = (s[1] - '1') * 16 + (s[0] - 'a');
    to = (s[3] - '1') * 16 + (s[2] - 'a');
    switch (s[4]) {
    case 'n': promo = T_KNIGHT; break;
    case 'b': promo = T_BISHOP; break;
    case 'r': promo = T_ROOK; break;
    case 'q': promo = T_QUEEN; break;
    default: promo = 0; break;
    }

    gen_legal(p, &ml);
    for (i = 0; i < ml.n; i++) {
        if (M_FROM(ml.m[i]) == from && M_TO(ml.m[i]) == to &&
            (promo == 0 || M_PROMO(ml.m[i]) == promo))
            return ml.m[i];
    }
    return 0;
}
