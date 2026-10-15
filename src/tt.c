/*
 * tt.c - transposition table for Lite Blue (C89).
 *
 * Entries are packed into 16 bytes (two 64-bit words: key + data) so that a
 * table sized to the 604e's 1 MiB L2 cache fits without spilling to DRAM.
 *
 * data layout:
 *   bits  0-21  move (from, to, promotion, flags)
 *   bits 22-37  score, biased by 32768
 *   bits 38-45  depth
 *   bits 46-47  bound flag (1 exact, 2 lower, 3 upper)
 *   bit  48     generation age
 *
 * Author: DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include <stdlib.h>
#include <string.h>
#include "tt.h"

typedef struct {
    U64 key;
    U64 data;
} TTEntry;

static TTEntry *table;
static long tsize;      /* slots */
static long tmask;
static long tfill;
static int age;

#define TT_MOVE(d)  ((Move)((d) & 0x3FFFFFULL))
#define TT_SCORE(d) ((int)(((d) >> 22) & 0xFFFFULL) - 32768)
#define TT_DEPTH(d) ((int)(((d) >> 38) & 0xFFULL))
#define TT_FLAG(d)  ((int)(((d) >> 46) & 0x3ULL))
#define TT_AGE(d)   ((int)(((d) >> 48) & 0x1ULL))

#define TT_PACK(mv, sc, dp, fl, ag) \
    ( ((U64)(mv) & 0x3FFFFFULL) \
    | (((U64)((sc) + 32768) & 0xFFFFULL) << 22) \
    | (((U64)(dp) & 0xFFULL) << 38) \
    | (((U64)(fl) & 0x3ULL) << 46) \
    | (((U64)(ag) & 0x1ULL) << 48) )

void tt_init(long entries)
{
    long e = 1;

    if (entries < 1024)
        entries = 1024;
    while (e * 2 <= entries)
        e *= 2;

    table = NULL;
    while (e >= 1024) {
        table = (TTEntry *) calloc((size_t) e, sizeof(TTEntry));
        if (table != NULL)
            break;
        e /= 2;
    }
    if (table == NULL) {
        e = 1024;
        table = (TTEntry *) calloc((size_t) e, sizeof(TTEntry));
    }
    tsize = e;
    tmask = e - 1;
    tfill = 0;
    age = 0;
}

void tt_clear(void)
{
    if (table != NULL)
        memset(table, 0, (size_t) tsize * sizeof(TTEntry));
    tfill = 0;
}

void tt_new_age(void)
{
    age ^= 1;
}

long tt_size(void)
{
    return tsize;
}

long tt_hashfull(void)
{
    long i, n = 0;
    if (table == NULL)
        return 0;
    for (i = 0; i < tsize; i++)
        if (table[i].data != 0 && TT_AGE(table[i].data) == age)
            n++;
    return n;
}

int tt_probe(U64 key, Move *move, int *score, int *flag, int *depth, int ply)
{
    TTEntry *e;
    U64 d;
    int sc;

    if (table == NULL)
        return 0;
    e = &table[(long)(key & (U64) tmask)];
    if (e->key != key || e->data == 0)
        return 0;

    d = e->data;
    sc = TT_SCORE(d);
    if (sc > MATE_SCORE - 1000)
        sc -= ply;
    else if (sc < -MATE_SCORE + 1000)
        sc += ply;

    *move = TT_MOVE(d);
    *score = sc;
    *flag = TT_FLAG(d);
    *depth = TT_DEPTH(d);
    return 1;
}

void tt_store(U64 key, Move move, int score, int flag, int depth, int ply)
{
    TTEntry *e;
    U64 d;

    if (table == NULL)
        return;

    e = &table[(long)(key & (U64) tmask)];

    /* Keep a deeper entry from the current generation over a shallow one. */
    if (e->data != 0 && e->key != key) {
        int ed = TT_DEPTH(e->data);
        int ea = TT_AGE(e->data);
        if (ed > depth && ea == age)
            return;
        if (e->key != 0 && TT_AGE(e->data) == age && ed == depth)
            return;
    }

    if (score > MATE_SCORE - 1000)
        score += ply;
    else if (score < -MATE_SCORE + 1000)
        score -= ply;

    if (e->data == 0)
        tfill++;

    d = TT_PACK(move, score, depth, flag, age);
    e->key = key;
    e->data = d;
}
