/*
 * eval.c - material + piece-square evaluation with a tapered king (C89).
 *
 * The piece-square tables are the well-known "simplified evaluation function"
 * tables (Tomasz Michniewski), written from White's point of view with the
 * first row being rank 8. The king is interpolated between its middlegame and
 * endgame tables by a material phase (0 = endgame, 24 = full material).
 *
 * This is a hand-tuned symbolic evaluation: the honest, era-appropriate
 * starting point. A learned/quantised evaluation is the natural next step and
 * is discussed in NOTES.md.
 *
 * Author: DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include "eval.h"

static const int pval[8] = { 0, 100, 320, 330, 500, 900, 0, 0 };

static const int pst_pawn[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,
      5,  5, 10, 25, 25, 10,  5,  5,
      0,  0,  0, 20, 20,  0,  0,  0,
      5, -5,-10,  0,  0,-10, -5,  5,
      5, 10, 10,-20,-20, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0
};

static const int pst_knight[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

static const int pst_bishop[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

static const int pst_rook[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
      5, 10, 10, 10, 10, 10, 10,  5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      0,  0,  0,  5,  5,  0,  0,  0
};

static const int pst_queen[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

static const int pst_king_mg[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
};

static const int pst_king_eg[64] = {
    -50,-40,-30,-20,-20,-30,-40,-50,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -50,-30,-30,-30,-30,-30,-30,-50
};

/* PST index: tables read from White's side with rank 8 at the top. */
static int pst_index(int sq, int color)
{
    int r = sq >> 4;
    int f = sq & 7;
    if (color == WHITE)
        return (7 - r) * 8 + f;
    return r * 8 + f;
}

static const int *pst_ptr(int type)
{
    switch (type) {
    case T_PAWN:   return pst_pawn;
    case T_KNIGHT: return pst_knight;
    case T_BISHOP: return pst_bishop;
    case T_ROOK:   return pst_rook;
    case T_QUEEN:  return pst_queen;
    default:       return pst_pawn;
    }
}

/*
 * Pawn shield in front of the king: rewards friendly pawns on the king's file
 * and the two files beside it, and penalises open files next to the king. It is
 * scaled by the material phase in eval(), so it matters in the middlegame but
 * not in the endgame (where the king belongs in the open).
 */
static int king_shield(const Position *p, int color)
{
    int k = p->king_sq[color];
    int r = k >> 4;
    int f = k & 7;
    int dir = (color == WHITE) ? 1 : -1;
    int own = (color == WHITE) ? WP : BP;
    int df, sc = 0;

    for (df = -1; df <= 1; df++) {
        int ff = f + df;
        if (ff < 0 || ff > 7)
            continue;
        if (r + dir >= 0 && r + dir <= 7 &&
            p->board[(r + dir) * 16 + ff] == own)
            sc += 12;
        else if (r + 2 * dir >= 0 && r + 2 * dir <= 7 &&
                 p->board[(r + 2 * dir) * 16 + ff] == own)
            sc += 6;
        else
            sc -= 12;
    }
    return sc;
}

/*
 * Pawn structure: penalise doubled and isolated pawns, reward passed pawns by
 * how close they are to promoting. Returns a White-relative score.
 */
static int pawn_structure(const Position *p)
{
    static const int passed[8] = { 0, 5, 10, 20, 35, 60, 100, 0 };
    int score = 0;
    int f, r, rr, ff, sq;
    int wcnt[8], bcnt[8];

    for (f = 0; f < 8; f++) { wcnt[f] = 0; bcnt[f] = 0; }
    for (sq = 0; sq < 128; sq++) {
        if (!on_board(sq))
            continue;
        if (p->board[sq] == WP)
            wcnt[sq & 7]++;
        else if (p->board[sq] == BP)
            bcnt[sq & 7]++;
    }

    for (f = 0; f < 8; f++) {
        int wl = (f > 0) ? wcnt[f - 1] : 0;
        int wr = (f < 7) ? wcnt[f + 1] : 0;
        int bl = (f > 0) ? bcnt[f - 1] : 0;
        int br = (f < 7) ? bcnt[f + 1] : 0;

        if (wcnt[f] > 1)
            score -= 12 * (wcnt[f] - 1);              /* doubled */
        if (bcnt[f] > 1)
            score += 12 * (bcnt[f] - 1);
        if (wcnt[f] > 0 && wl == 0 && wr == 0)
            score -= 15 * wcnt[f];                    /* isolated */
        if (bcnt[f] > 0 && bl == 0 && br == 0)
            score += 15 * bcnt[f];
    }

    for (sq = 0; sq < 128; sq++) {
        int pc, blocked;
        if (!on_board(sq))
            continue;
        pc = p->board[sq];
        r = sq >> 4;
        f = sq & 7;
        if (pc == WP) {
            blocked = 0;
            for (ff = f - 1; ff <= f + 1 && !blocked; ff++) {
                if (ff < 0 || ff > 7)
                    continue;
                for (rr = r + 1; rr < 8; rr++)
                    if (p->board[rr * 16 + ff] == BP) { blocked = 1; break; }
            }
            if (!blocked)
                score += passed[r];
        } else if (pc == BP) {
            blocked = 0;
            for (ff = f - 1; ff <= f + 1 && !blocked; ff++) {
                if (ff < 0 || ff > 7)
                    continue;
                for (rr = r - 1; rr >= 0; rr--)
                    if (p->board[rr * 16 + ff] == WP) { blocked = 1; break; }
            }
            if (!blocked)
                score -= passed[7 - r];
        }
    }
    return score;
}

int eval(const Position *p)
{
    int sq;
    int score = 0;          /* White-relative centipawns */
    int phase = 0;          /* 0 (endgame) .. 24 (full material) */
    int wb = 0, bb = 0;

    for (sq = 0; sq < 128; sq++) {
        int pc, t, col, idx;

        if (!on_board(sq))
            continue;
        pc = p->board[sq];
        if (pc == EMPTY)
            continue;
        t = PC_TYPE(pc);
        if (t == T_KING)
            continue;

        col = PC_COLOR(pc);
        idx = pst_index(sq, col);

        if (col == WHITE)
            score += pval[t] + pst_ptr(t)[idx];
        else
            score -= pval[t] + pst_ptr(t)[idx];

        switch (t) {
        case T_KNIGHT: phase += 1; break;
        case T_BISHOP: phase += 1; if (col == WHITE) wb++; else bb++; break;
        case T_ROOK:   phase += 2; break;
        case T_QUEEN:  phase += 4; break;
        default:       break;
        }
    }
    if (phase > 24)
        phase = 24;

    {
        int wi = pst_index(p->king_sq[WHITE], WHITE);
        int bi = pst_index(p->king_sq[BLACK], BLACK);
        score += (pst_king_mg[wi] * (24 - phase) + pst_king_eg[wi] * phase) / 24;
        score -= (pst_king_mg[bi] * (24 - phase) + pst_king_eg[bi] * phase) / 24;
    }

    if (wb >= 2)
        score += 30;
    if (bb >= 2)
        score -= 30;

    /* king safety: a pawn shield, scaled to the middlegame */
    score += (king_shield(p, WHITE) - king_shield(p, BLACK)) * phase / 24;

    score += pawn_structure(p);

    score += (p->side == WHITE) ? 10 : -10;     /* tempo */

    return (p->side == WHITE) ? score : -score;
}
