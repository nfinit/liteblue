# Lite Blue

A portable chess engine in C89, for AIX and other Unix systems.
By DeepSeek (deepseek-v4.1-flash).

Lite Blue is deliberately single-node. It was written for one IBM RS/6000 43P
(PowerPC 604e, 250 MHz, AIX 4.3.3) as the small answer to Deep Blue, but it
builds unmodified on ordinary Unix.

## Features

- 0x88 board representation.
- Negamax alpha-beta search: iterative deepening, a Zobrist transposition table,
  quiescence search, null-move pruning, late-move reductions, principal-variation
  search, and killer/history move ordering.
- Hand-tuned evaluation: material, piece-square tables, and a tapered king.
- Three front ends over one library:
  - `liteblue` - UCI, the authoritative interface.
  - `tui` - an interactive terminal board.
  - `arena` - a self-play / match driver.

## Build

```
cd src
make                                                 # cc
make CC=/usr/vac/bin/xlc CFLAGS="-O2 -qlanglvl=ansi" # IBM XL C
make CC=gcc CFLAGS="-O2 -ansi"                       # GCC
```

## Run

UCI:

```
printf 'position startpos\ngo depth 8\nquit\n' | ./liteblue
```

Interactive: `./tui` (type a move like `e2e4`, or `d`, `undo`, `new`, `go`,
`time <ms>`, `quit`).

Self-play: `./arena 500 40` (500 ms per move, up to 40 plies).

## Tools

`tools/match` plays two engine binaries against each other over UCI and reports
the score and an Elo delta; it is how engine changes are measured here.

```
cd tools && make
./match ../src/liteblue ../src/liteblue 6 6 200
```

Usage: `match ENGINE_A ENGINE_B [depthA] [depthB] [max_plies]`. Each opening is
played twice, colours swapped.

## Correctness

- Move generation is perft-verified against the standard test positions, with the
  Zobrist key recomputed and compared at every internal node.
- The search is deterministic across XL C 5.0.2 and GCC 2.95.3 (byte-identical
  node counts).
- `make check` runs a repetition regression test and the perft suite.

Measured on the 250 MHz PowerPC 604e: about 47,000 nodes per second.

## Version

1.0.0; see `CHANGELOG.md`. The version lives in `src/version.h` and is reported
over UCI on the `id name` line.

## Licence

MIT. See `LICENSE`.
