/*
 * board.c - position representation for Lite Blue (C89).
 *
 * Implements FEN parsing, Zobrist hashing (incremental, with a from-scratch
 * routine used to verify it), attack detection, and make/unmake.
 *
 * Author: DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "board.h"

static U64 z_piece[16][128];
static U64 z_side;
static U64 z_castle[16];
static U64 z_ep[8];

/* Castling rights removed by a move from or to each square. */
static int castling_mask[128];

static U64 rng_state;

/*
 * splitmix64. A fixed seed keeps Zobrist keys reproducible across builds and
 * both compilers; the seed is deliberately documented (see NOTES.md).
 */
static U64 next_rand(void)
{
    U64 z;
    rng_state += 0x9E3779B97F4A7C15ULL;
    z = rng_state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void board_init(void)
{
    int i, s;
    rng_state = 0x0123456789ABCDEFULL;
    for (i = 0; i < 16; i++)
        for (s = 0; s < 128; s++)
            z_piece[i][s] = next_rand();
    z_side = next_rand();
    for (i = 0; i < 16; i++)
        z_castle[i] = next_rand();
    for (i = 0; i < 8; i++)
        z_ep[i] = next_rand();

    for (i = 0; i < 128; i++)
        castling_mask[i] = 15;
    castling_mask[SQ_E1] = 15 & ~(CASTLE_WK | CASTLE_WQ);
    castling_mask[SQ_A1] = 15 & ~CASTLE_WQ;
    castling_mask[SQ_H1] = 15 & ~CASTLE_WK;
    castling_mask[SQ_E8] = 15 & ~(CASTLE_BK | CASTLE_BQ);
    castling_mask[SQ_A8] = 15 & ~CASTLE_BQ;
    castling_mask[SQ_H8] = 15 & ~CASTLE_BK;
}

int on_board(int sq)
{
    return (sq & 0x88) == 0;
}

const char *sq_name(int sq)
{
    static char buf[4];
    if (!on_board(sq)) {
        strcpy(buf, "--");
    } else {
        buf[0] = (char)('a' + (sq & 7));
        buf[1] = (char)('1' + (sq >> 4));
        buf[2] = '\0';
    }
    return buf;
}

static int piece_from_char(int c)
{
    switch (c) {
    case 'P': return WP;
    case 'N': return WN;
    case 'B': return WB;
    case 'R': return WR;
    case 'Q': return WQ;
    case 'K': return WK;
    case 'p': return BP;
    case 'n': return BN;
    case 'b': return BB;
    case 'r': return BR;
    case 'q': return BQ;
    case 'k': return BK;
    default:  return EMPTY;
    }
}

static const char piece_char[16] = {
    '.', 'P', 'N', 'B', 'R', 'Q', 'K', '?',
    '?', 'p', 'n', 'b', 'r', 'q', 'k', '?'
};

int pos_fen(Position *p, const char *fen)
{
    const char *s = fen;
    int r, f, sq, i;

    if (fen == NULL)
        return -1;

    for (i = 0; i < 128; i++)
        p->board[i] = OFFBOARD;
    for (r = 0; r < 8; r++)
        for (f = 0; f < 8; f++)
            p->board[r * 16 + f] = EMPTY;

    p->side = WHITE;
    p->castling = 0;
    p->ep = -1;
    p->halfmove = 0;
    p->fullmove = 0;
    p->king_sq[WHITE] = -1;
    p->king_sq[BLACK] = -1;

    /* 1. piece placement, from rank 8 down to rank 1 */
    r = 7;
    f = 0;
    while (*s != '\0' && *s != ' ') {
        if (*s == '/') {
            r--;
            f = 0;
        } else if (*s >= '1' && *s <= '8') {
            f += *s - '0';
        } else {
            int pc = piece_from_char((unsigned char)*s);
            if (pc == EMPTY)
                return -1;
            sq = r * 16 + f;
            p->board[sq] = pc;
            if (PC_TYPE(pc) == T_KING)
                p->king_sq[PC_COLOR(pc)] = sq;
            f++;
        }
        s++;
    }

    /* 2. side to move */
    while (*s == ' ')
        s++;
    if (*s == 'b')
        p->side = BLACK;
    if (*s != '\0')
        s++;

    /* 3. castling availability */
    while (*s == ' ')
        s++;
    while (*s != '\0' && *s != ' ') {
        switch (*s) {
        case 'K': p->castling |= CASTLE_WK; break;
        case 'Q': p->castling |= CASTLE_WQ; break;
        case 'k': p->castling |= CASTLE_BK; break;
        case 'q': p->castling |= CASTLE_BQ; break;
        default: break;
        }
        s++;
    }

    /* 4. en-passant target */
    while (*s == ' ')
        s++;
    if (*s != '\0' && *s != '-' && *s != ' ') {
        int ff = s[0] - 'a';
        int rr = s[1] - '1';
        if (ff < 0 || ff > 7 || rr < 0 || rr > 7)
            return -1;
        p->ep = rr * 16 + ff;
        s += 2;
    } else if (*s == '-') {
        s++;
    }

    /* 5. halfmove clock */
    while (*s == ' ')
        s++;
    while (*s >= '0' && *s <= '9') {
        p->halfmove = p->halfmove * 10 + (*s - '0');
        s++;
    }

    /* 6. fullmove number */
    while (*s == ' ')
        s++;
    while (*s >= '0' && *s <= '9') {
        p->fullmove = p->fullmove * 10 + (*s - '0');
        s++;
    }
    if (p->fullmove == 0)
        p->fullmove = 1;

    if (p->king_sq[WHITE] < 0 || p->king_sq[BLACK] < 0)
        return -1;

    p->key = zobrist_compute(p);
    return 0;
}

void pos_start(Position *p)
{
    pos_fen(p, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

U64 zobrist_compute(const Position *p)
{
    U64 k = 0;
    int sq;
    for (sq = 0; sq < 128; sq++) {
        if (!on_board(sq))
            continue;
        if (p->board[sq] != EMPTY)
            k ^= z_piece[p->board[sq]][sq];
    }
    if (p->side == BLACK)
        k ^= z_side;
    k ^= z_castle[p->castling & 15];
    if (p->ep >= 0)
        k ^= z_ep[p->ep & 7];
    return k;
}

void pos_print(const Position *p)
{
    int r, f;
    printf("  +-----------------+\n");
    for (r = 7; r >= 0; r--) {
        printf("%d | ", r + 1);
        for (f = 0; f < 8; f++)
            printf("%c ", piece_char[p->board[r * 16 + f]]);
        printf("|\n");
    }
    printf("  +-----------------+\n");
    printf("    a b c d e f g h\n");
    printf("side=%s castle=%d ep=%s half=%d full=%d key=%016llx\n",
           p->side == WHITE ? "w" : "b", p->castling,
           p->ep >= 0 ? sq_name(p->ep) : "-",
           p->halfmove, p->fullmove, p->key);
}

int sq_attacked(const Position *p, int sq, int by)
{
    static const int KN[8] = { -33, -31, -18, -14, 14, 18, 31, 33 };
    static const int KG[8] = { -17, -16, -15, -1, 1, 15, 16, 17 };
    static const int BD[4] = { -17, -15, 15, 17 };
    static const int RD[4] = { -16, -1, 1, 16 };
    int i, t;

    /* pawns */
    if (by == WHITE) {
        if (on_board(sq - 17) && p->board[sq - 17] == WP) return 1;
        if (on_board(sq - 15) && p->board[sq - 15] == WP) return 1;
    } else {
        if (on_board(sq + 17) && p->board[sq + 17] == BP) return 1;
        if (on_board(sq + 15) && p->board[sq + 15] == BP) return 1;
    }

    /* knights */
    {
        int pc = (by == WHITE) ? WN : BN;
        for (i = 0; i < 8; i++) {
            t = sq + KN[i];
            if (on_board(t) && p->board[t] == pc)
                return 1;
        }
    }

    /* king */
    {
        int pc = (by == WHITE) ? WK : BK;
        for (i = 0; i < 8; i++) {
            t = sq + KG[i];
            if (on_board(t) && p->board[t] == pc)
                return 1;
        }
    }

    /* bishop and queen on diagonals */
    {
        int b = (by == WHITE) ? WB : BB;
        int q = (by == WHITE) ? WQ : BQ;
        for (i = 0; i < 4; i++) {
            t = sq + BD[i];
            while (on_board(t)) {
                int pc = p->board[t];
                if (pc != EMPTY) {
                    if (pc == b || pc == q)
                        return 1;
                    break;
                }
                t += BD[i];
            }
        }
    }

    /* rook and queen on ranks and files */
    {
        int r = (by == WHITE) ? WR : BR;
        int q = (by == WHITE) ? WQ : BQ;
        for (i = 0; i < 4; i++) {
            t = sq + RD[i];
            while (on_board(t)) {
                int pc = p->board[t];
                if (pc != EMPTY) {
                    if (pc == r || pc == q)
                        return 1;
                    break;
                }
                t += RD[i];
            }
        }
    }

    return 0;
}

void make_move(Position *p, Move m, Undo *u)
{
    int from = M_FROM(m);
    int to = M_TO(m);
    int flags = M_FLAGS(m);
    int promo = M_PROMO(m);
    int pc = p->board[from];
    int cap = p->board[to];

    u->castling = p->castling;
    u->ep = p->ep;
    u->halfmove = p->halfmove;
    u->key = p->key;
    u->captured = cap;

    if (p->ep >= 0)
        p->key ^= z_ep[p->ep & 7];
    p->key ^= z_castle[p->castling & 15];

    /* lift the moving piece */
    p->key ^= z_piece[pc][from];
    p->board[from] = EMPTY;

    /* handle capture (including en passant) */
    if (flags & FLAG_EP) {
        int cap_sq = (p->side == WHITE) ? to - 16 : to + 16;
        int cpc = p->board[cap_sq];
        u->captured = cpc;
        p->key ^= z_piece[cpc][cap_sq];
        p->board[cap_sq] = EMPTY;
    } else if (cap != EMPTY) {
        p->key ^= z_piece[cap][to];
    }

    /* place the piece (or its promotion) */
    if (promo != 0) {
        int npc = (p->side == WHITE) ? promo : promo + 8;
        p->board[to] = npc;
        p->key ^= z_piece[npc][to];
    } else {
        p->board[to] = pc;
        p->key ^= z_piece[pc][to];
        if (PC_TYPE(pc) == T_KING)
            p->king_sq[p->side] = to;
    }

    /* move the rook for castling */
    if (flags & FLAG_CASTLE) {
        int rf, rt, rook;
        if (to > from) { rf = from + 3; rt = from + 1; }
        else           { rf = from - 4; rt = from - 1; }
        rook = p->board[rf];
        p->board[rf] = EMPTY;
        p->board[rt] = rook;
        p->key ^= z_piece[rook][rf] ^ z_piece[rook][rt];
    }

    /* castling rights */
    p->castling &= castling_mask[from] & castling_mask[to];

    /* en-passant square */
    if (flags & FLAG_DOUBLE)
        p->ep = (p->side == WHITE) ? from + 16 : from - 16;
    else
        p->ep = -1;
    if (p->ep >= 0)
        p->key ^= z_ep[p->ep & 7];

    /* halfmove clock */
    if (PC_TYPE(pc) == T_PAWN || cap != EMPTY || (flags & FLAG_EP))
        p->halfmove = 0;
    else
        p->halfmove++;

    if (p->side == BLACK)
        p->fullmove++;
    p->side ^= 1;
    p->key ^= z_side;
    p->key ^= z_castle[p->castling & 15];
}

void make_null(Position *p, Undo *u)
{
    u->castling = p->castling;
    u->ep = p->ep;
    u->halfmove = p->halfmove;
    u->key = p->key;
    u->captured = EMPTY;

    if (p->ep >= 0)
        p->key ^= z_ep[p->ep & 7];
    p->ep = -1;
    p->halfmove++;

    if (p->side == BLACK)
        p->fullmove++;
    p->side ^= 1;
    p->key ^= z_side;
}

void unmake_null(Position *p, const Undo *u)
{
    p->side ^= 1;
    if (p->side == BLACK)
        p->fullmove--;
    p->castling = u->castling;
    p->ep = u->ep;
    p->halfmove = u->halfmove;
    p->key = u->key;
}

void move_str(Move m, char *buf)
{
    int from = M_FROM(m);
    int to = M_TO(m);
    int promo = M_PROMO(m);

    buf[0] = (char)('a' + (from & 7));
    buf[1] = (char)('1' + (from >> 4));
    buf[2] = (char)('a' + (to & 7));
    buf[3] = (char)('1' + (to >> 4));
    buf[4] = '\0';
    if (promo != 0) {
        buf[4] = "?nbrq"[promo];
        buf[5] = '\0';
    }
}

void unmake_move(Position *p, Move m, const Undo *u)
{
    int from = M_FROM(m);
    int to = M_TO(m);
    int flags = M_FLAGS(m);
    int promo = M_PROMO(m);
    int pc;

    p->side ^= 1;
    if (p->side == BLACK)
        p->fullmove--;

    /* put the rook back before the king (king still on 'to') */
    if (flags & FLAG_CASTLE) {
        int rf, rt, rook;
        if (to > from) { rf = from + 3; rt = from + 1; }
        else           { rf = from - 4; rt = from - 1; }
        rook = p->board[rt];
        p->board[rt] = EMPTY;
        p->board[rf] = rook;
    }

    if (promo != 0) {
        p->board[from] = (p->side == WHITE) ? WP : BP;
        p->board[to] = EMPTY;
    } else {
        pc = p->board[to];
        p->board[from] = pc;
        p->board[to] = EMPTY;
        if (PC_TYPE(pc) == T_KING)
            p->king_sq[p->side] = from;
    }

    if (flags & FLAG_EP) {
        int cap_sq = (p->side == WHITE) ? to - 16 : to + 16;
        p->board[cap_sq] = u->captured;
    } else if (u->captured != EMPTY) {
        p->board[to] = u->captured;
    }

    p->castling = u->castling;
    p->ep = u->ep;
    p->halfmove = u->halfmove;
    p->key = u->key;
}
