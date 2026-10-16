# Changelog

Versioning is `MAJOR.MINOR.PATCH`. `MAJOR` for incompatible changes (protocol,
move/board format, interface); `MINOR` for new features or measurable changes in
strength or behaviour; `PATCH` for bug fixes and other non-behavioural changes.

## 1.2.0 - 2026-10-15

- Add a **pawn-structure** evaluation: bonuses for passed pawns by how close
  they are to promoting, and penalties for doubled and isolated pawns. Measured
  over 512 openings (1024 games) with the match harness at depth 6: **+33 Elo**
  versus 1.1.0.

## 1.1.0 - 2026-10-15

- Add a pawn-shield **king-safety** term to the evaluation, scaled by the
  material phase. In self-play the engine now castles instead of walking its
  king up the board. Measured with the match harness over 32 openings: **+38 Elo
  at depth 6** and **+89 Elo at depth 8** against 1.0.0 (error bars are wide at
  this sample size, but the direction is consistent and grows with depth).

## 1.0.0 - 2026-10-15

First release: a correct, playable, portable C89 chess engine for AIX and other
Unix systems.

- 0x88 board; negamax alpha-beta search with iterative deepening, a transposition
  table, quiescence search, null-move pruning, late-move reductions,
  principal-variation search, and killer/history move ordering.
- Hand-tuned evaluation: material, piece-square tables, and a tapered king.
- Three front ends over one library: **UCI** (the authoritative interface), an
  interactive **TUI**, and a self-play **arena**.
- Move generation perft-verified against the standard suite with the Zobrist key
  recomputed and compared at every internal node; the search is deterministic
  across XL C 5.0.2 and GCC 2.95.3 (byte-identical node counts).
- Builds unmodified with `xlc`, `gcc` or `cc`; `make check` runs a repetition
  regression test and the perft suite.

### Fixed (review by Claude Opus 5.5)

- Repetition history could be read past the end of its buffer with a game
  history longer than `REP_MAX`.
- A zeroed `SearchLimit` was read as "one millisecond"; a non-positive clock is
  now treated as "not set", and `search_limit_default()` is provided.
- `search_go` could return no move; it now always returns a legal one if any.
- `tt_init` leaked the previous table on `setoption name Hash`.
