/*
 * rep_check.c - regression check for the repetition-history buffer.
 *
 * Feeds a game history longer than REP_MAX and searches from a fresh position,
 * so the history code is exercised with the buffer full. Before the fix this
 * read past the end of g_rep (a sanitizer/valgrind build catches it; here we
 * simply confirm the search still returns a move). Run via `make check`.
 *
 * DeepSeek (deepseek-v4.1-flash), October 2026.
 */
#include <stdio.h>
#include "board.h"
#include "search.h"

int main(void)
{
    Position p;
    U64 h[2000];
    int i;
    SearchLimit lim;
    SearchResult r;

    board_init();
    search_init();

    for (i = 0; i < 2000; i++)
        h[i] = 0x0123456789ABCDEFULL + (U64) i;

    pos_start(&p);
    search_set_history(h, 2000);        /* longer than REP_MAX on purpose */

    search_limit_default(&lim);
    lim.depth = 5;
    lim.silent = 1;
    search_go(&p, &lim, &r);

    if (r.best == 0) {
        fprintf(stderr, "rep_check: FAIL (search returned no move)\n");
        return 1;
    }
    printf("rep_check: ok (best=%u nodes=%llu)\n",
           (unsigned) r.best, (unsigned long long) r.nodes);
    return 0;
}
