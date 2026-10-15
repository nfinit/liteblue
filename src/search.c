/*
 * search.c - negamax alpha-beta search for Lite Blue (C89).
 *
 * Features: iterative deepening, a Zobrist transposition table (tt.c),
 * quiescence search over captures and promotions, principal-variation search,
 * null-move pruning, late-move reductions, and killer/history move ordering.
 * Repetition is detected against the game history supplied by the caller.
 *
 * This is the classic symbolic search that Deep Blue's strength rested on,
 * scaled to what one 604e can do.
 *
 * Author: DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "search.h"
#include "movegen.h"
#include "eval.h"
#include "tt.h"

#define MAX_PLY 64
#define REP_MAX 1024

static const int mvv_val[8] = { 0, 100, 320, 330, 500, 900, 0, 0 };

static U64 g_nodes;
static int g_stop;
static clock_t g_start;
static int g_hard_ms;
static int g_soft_ms;
static int g_node_limit;

static Move g_killers[MAX_PLY][2];
static int g_history[128][128];

static Move g_pv[MAX_PLY][MAX_PLY];
static int g_pvlen[MAX_PLY];

static U64 g_rep[REP_MAX];
static int g_repn;

void search_init(void)
{
    tt_init(65536);         /* ~1 MiB at 16 bytes/entry: fits the 604e L2 */
}

void search_set_hash(long entries)
{
    tt_init(entries);
}

void search_clear(void)
{
    tt_clear();
}

void search_set_history(const U64 *keys, int n)
{
    int i;
    g_repn = 0;
    for (i = 0; i < n && i < REP_MAX; i++)
        g_rep[g_repn++] = keys[i];
}

static int elapsed_ms(clock_t t0)
{
    return (int)((clock() - t0) * 1000L / (long) CLOCKS_PER_SEC);
}

static void rep_push(U64 key)
{
    if (g_repn < REP_MAX)
        g_rep[g_repn] = key;
    g_repn++;
}

static void rep_pop(void)
{
    if (g_repn > 0)
        g_repn--;
}

static int is_repetition(const Position *p)
{
    int i, reps;

    if (g_repn < 3)
        return 0;
    reps = p->halfmove;
    for (i = g_repn - 3; i >= 0 && reps >= 2; i -= 2, reps -= 2)
        if (g_rep[i] == p->key)
            return 1;
    return 0;
}

static int has_nonpawn(const Position *p)
{
    int sq, us = p->side;
    for (sq = 0; sq < 128; sq++) {
        int pc, t;
        if (!on_board(sq))
            continue;
        pc = p->board[sq];
        if (pc == EMPTY || PC_COLOR(pc) != us)
            continue;
        t = PC_TYPE(pc);
        if (t != T_PAWN && t != T_KING)
            return 1;
    }
    return 0;
}

/* --- move ordering ------------------------------------------------------ */

static int score_move(const Position *p, Move m, Move tt_move, int ply)
{
    int flags = M_FLAGS(m);

    if (m == tt_move)
        return 1000000;

    if (flags & (FLAG_CAPTURE | FLAG_PROMO)) {
        int victim, attacker;
        if (flags & FLAG_EP)
            victim = (p->side == WHITE) ? BP : WP;
        else
            victim = p->board[M_TO(m)];
        attacker = p->board[M_FROM(m)];
        return 100000 + mvv_val[PC_TYPE(victim)] * 16 - mvv_val[PC_TYPE(attacker)];
    }

    if (m == g_killers[ply][0])
        return 90000;
    if (m == g_killers[ply][1])
        return 89999;

    return g_history[M_FROM(m)][M_TO(m)];
}

static void order_moves(const Position *p, MoveList *ml, Move tt_move, int ply,
                        int *scores)
{
    int i, j;

    for (i = 0; i < ml->n; i++)
        scores[i] = score_move(p, ml->m[i], tt_move, ply);

    /* selection sort by descending score */
    for (i = 0; i < ml->n - 1; i++) {
        int best = i;
        for (j = i + 1; j < ml->n; j++)
            if (scores[j] > scores[best])
                best = j;
        if (best != i) {
            Move tm = ml->m[i]; ml->m[i] = ml->m[best]; ml->m[best] = tm;
            { int ts = scores[i]; scores[i] = scores[best]; scores[best] = ts; }
        }
    }
}

/* --- quiescence --------------------------------------------------------- */

static int quiesce(Position *p, int alpha, int beta, int ply)
{
    MoveList ml;
    Undo u;
    int i, best, stand;
    int scores[MAX_MOVES];

    if (g_stop)
        return 0;
    if ((g_nodes & 1023) == 0 && g_hard_ms > 0 &&
        elapsed_ms(g_start) >= g_hard_ms) {
        g_stop = 1;
        return 0;
    }
    g_nodes++;

    if (p->halfmove >= 100)
        return 0;

    stand = eval(p);
    if (stand >= beta)
        return stand;
    if (stand > alpha)
        alpha = stand;
    best = stand;

    if (ply >= MAX_PLY - 1)
        return best;

    gen_captures(p, &ml);
    order_moves(p, &ml, 0, ply, scores);

    for (i = 0; i < ml.n; i++) {
        Move m = ml.m[i];
        make_move(p, m, &u);
        if (sq_attacked(p, p->king_sq[p->side ^ 1], p->side)) {
            unmake_move(p, m, &u);
            continue;
        }
        {
            int score = -quiesce(p, -beta, -alpha, ply + 1);
            unmake_move(p, m, &u);
            if (g_stop)
                return best;
            if (score > best) {
                best = score;
                if (score > alpha) {
                    alpha = score;
                    if (alpha >= beta)
                        break;
                }
            }
        }
    }
    return best;
}

/* --- main search -------------------------------------------------------- */

static int negamax(Position *p, int depth, int alpha, int beta, int ply)
{
    MoveList ml;
    Undo u;
    int i;
    int best = -INF_SCORE;
    Move best_move = 0;
    int in_check, legal = 0, raised = 0;
    int old_alpha = alpha;
    Move tt_move = 0;
    int tt_score = 0, tt_flag = 0, tt_depth = 0;
    int scores[MAX_MOVES];

    g_pvlen[ply] = ply;

    if (g_stop)
        return 0;
    if ((g_nodes & 1023) == 0) {
        if (g_hard_ms > 0 && elapsed_ms(g_start) >= g_hard_ms) {
            g_stop = 1;
            return 0;
        }
        if (g_node_limit > 0 && g_nodes >= (U64) g_node_limit) {
            g_stop = 1;
            return 0;
        }
    }

    if (ply >= MAX_PLY - 1)
        return eval(p);

    /* game-state draws */
    if (p->halfmove >= 100)
        return 0;

    in_check = sq_attacked(p, p->king_sq[p->side], p->side ^ 1);

    /* transposition table */
    if (tt_probe(p->key, &tt_move, &tt_score, &tt_flag, &tt_depth, ply)) {
        if (ply > 0 && tt_depth >= depth) {
            if (tt_flag == TT_EXACT)
                return tt_score;
            if (tt_flag == TT_LOWER && tt_score >= beta)
                return tt_score;
            if (tt_flag == TT_UPPER && tt_score <= alpha)
                return tt_score;
        }
    }

    if (depth <= 0)
        return quiesce(p, alpha, beta, ply);

    if (ply > 0 && is_repetition(p))
        return 0;

    /* mate-distance pruning */
    if (alpha < -MATE_SCORE + ply)
        alpha = -MATE_SCORE + ply;
    if (beta > MATE_SCORE - ply - 1)
        beta = MATE_SCORE - ply - 1;
    if (alpha >= beta)
        return alpha;

    /* null-move pruning */
    if (!in_check && ply > 0 && depth >= 3 && has_nonpawn(p)) {
        int R = 2 + depth / 4;
        int nullscore;
        Undo nu;
        if (R >= depth)
            R = depth - 1;
        make_null(p, &nu);
        rep_push(p->key);
        nullscore = -negamax(p, depth - 1 - R, -beta, -beta + 1, ply + 1);
        rep_pop();
        unmake_null(p, &nu);
        if (g_stop)
            return 0;
        if (nullscore >= beta && nullscore < MATE_SCORE - 1000)
            return beta;
    }

    gen_legal(p, &ml);
    order_moves(p, &ml, tt_move, ply, scores);

    for (i = 0; i < ml.n; i++) {
        Move m = ml.m[i];
        int score;
        int is_quiet = !(M_FLAGS(m) & (FLAG_CAPTURE | FLAG_PROMO));
        int newdepth = depth - 1;

        legal++;
        make_move(p, m, &u);
        rep_push(p->key);
        g_nodes++;

        if (legal == 1) {
            score = -negamax(p, newdepth, -beta, -alpha, ply + 1);
        } else {
            int r = 0;
            if (is_quiet && !in_check && depth >= 3 && legal > 3) {
                r = (legal > 6) ? 2 : 1;
                if (r > newdepth)
                    r = newdepth;
                if (r < 0)
                    r = 0;
            }
            score = -negamax(p, newdepth - r, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && r > 0)
                score = -negamax(p, newdepth, -alpha - 1, -alpha, ply + 1);
            if (score > alpha && score < beta)
                score = -negamax(p, newdepth, -beta, -alpha, ply + 1);
        }

        rep_pop();
        unmake_move(p, m, &u);
        if (g_stop)
            return 0;

        if (score > best) {
            best = score;
            best_move = m;
            if (score > alpha) {
                int j;
                alpha = score;
                raised = 1;
                g_pv[ply][ply] = m;
                for (j = ply + 1; j < g_pvlen[ply + 1]; j++)
                    g_pv[ply][j] = g_pv[ply + 1][j];
                g_pvlen[ply] = g_pvlen[ply + 1];

                if (alpha >= beta) {
                    if (is_quiet) {
                        if (g_killers[ply][0] != m) {
                            g_killers[ply][1] = g_killers[ply][0];
                            g_killers[ply][0] = m;
                        }
                        g_history[M_FROM(m)][M_TO(m)] += depth * depth;
                        if (g_history[M_FROM(m)][M_TO(m)] > 500000) {
                            int a, b;
                            for (a = 0; a < 128; a++)
                                for (b = 0; b < 128; b++)
                                    g_history[a][b] /= 2;
                        }
                    }
                    break;
                }
            }
        }
    }

    if (legal == 0)
        return in_check ? (-MATE_SCORE + ply) : 0;

    if (g_stop)
        return 0;

    {
        int flag;
        if (best >= beta)
            flag = TT_LOWER;
        else if (best <= old_alpha)
            flag = TT_UPPER;
        else
            flag = TT_EXACT;
        tt_store(p->key, best_move, best, flag, depth, ply);
    }
    (void) raised;

    return best;
}

static void print_info(int depth, int score, clock_t t0)
{
    int elapsed = elapsed_ms(t0);
    long nps;
    int i;

    nps = (elapsed > 0) ? (long)((double) g_nodes * 1000.0 / (double) elapsed)
                        : (long) g_nodes;

    printf("info depth %d ", depth);
    if (score > MATE_SCORE - 1000)
        printf("score mate %d ", (MATE_SCORE - score + 1) / 2);
    else if (score < -MATE_SCORE + 1000)
        printf("score mate -%d ", (MATE_SCORE + score + 1) / 2);
    else
        printf("score cp %d ", score);
    printf("nodes %llu nps %ld time %d pv", g_nodes, nps, elapsed);
    for (i = 0; i < g_pvlen[0]; i++) {
        char buf[8];
        move_str(g_pv[0][i], buf);
        printf(" %s", buf);
    }
    printf("\n");
    fflush(stdout);
}

static void compute_time(const SearchLimit *lim, const Position *p)
{
    int t, inc;

    g_soft_ms = 0;
    g_hard_ms = 0;

    if (lim->movetime_ms > 0) {
        g_soft_ms = lim->movetime_ms;
        g_hard_ms = lim->movetime_ms;
        return;
    }
    if (lim->infinite)
        return;

    t = (p->side == WHITE) ? lim->wtime_ms : lim->btime_ms;
    inc = (p->side == WHITE) ? lim->winc_ms : lim->binc_ms;
    if (t < 0)
        return;
    if (inc < 0)
        inc = 0;

    g_soft_ms = t / 25 + inc / 2;
    g_hard_ms = t / 3;
    if (g_soft_ms < 10)
        g_soft_ms = 10;
    if (g_hard_ms < g_soft_ms)
        g_hard_ms = g_soft_ms;
    if (g_hard_ms > t - 100)
        g_hard_ms = t - 100;
    if (g_hard_ms < 1)
        g_hard_ms = 1;
}

Move search_go(Position *p, const SearchLimit *lim, SearchResult *res)
{
    int d, maxd;
    Move best = 0;
    int bestscore = 0;
    int completed = 0;
    clock_t t0;

    g_nodes = 0;
    g_stop = 0;
    g_node_limit = lim->nodes;
    memset(g_killers, 0, sizeof(g_killers));
    memset(g_history, 0, sizeof(g_history));

    t0 = clock();
    g_start = t0;

    compute_time(lim, p);
    maxd = (lim->depth > 0) ? lim->depth : (MAX_PLY - 2);

    tt_new_age();

    for (d = 1; d <= maxd; d++) {
        int score = negamax(p, d, -INF_SCORE, INF_SCORE, 0);
        if (g_stop)
            break;
        best = g_pv[0][0];
        bestscore = score;
        completed = d;
        if (!lim->silent)
            print_info(d, score, t0);
        if (g_soft_ms > 0 && elapsed_ms(t0) >= g_soft_ms)
            break;
    }

    if (res != NULL) {
        res->nodes = g_nodes;
        res->depth = completed;
        res->score = bestscore;
        res->time_ms = elapsed_ms(t0);
        res->best = best;
    }
    return best;
}
