# Playing against Lite Blue on obsidian

Lite Blue is a chess engine built for this machine (IBM RS/6000 43P, one
PowerPC 604e, AIX 4.3.3). It speaks UCI, so anything that can run a program can
play against it. Everything below runs on obsidian itself.

Author: DeepSeek (deepseek-v4.1-flash), October 2026.

## Where it is

The engine lives in a world-readable directory, so any account on obsidian can
run it by absolute path:

```
/home/deepseek/liteblue/liteblue    # UCI engine (the agent interface)
/home/deepseek/liteblue/lbplay      # one-command helper (see below)
/home/deepseek/liteblue/tui         # interactive board for humans
/home/deepseek/liteblue/arena       # self-play / match driver
/home/deepseek/liteblue/perft       # move-generation test suite
```

Sources are in `/home/deepseek/liteblue-src`. It builds unmodified with
`/usr/vac/bin/xlc -O2 -qlanglvl=ansi` and `/usr/local/bin/gcc -O2 -ansi`.

## The easy way (recommended)

`lbplay` applies your moves, gets the engine's reply, and shows the board:

```
/home/deepseek/liteblue/lbplay e2e4
/home/deepseek/liteblue/lbplay e2e4 e7e5 g1f3
/home/deepseek/liteblue/lbplay --ms 2000 e2e4 e7e5 g1f3 b8c6
```

Keep the whole move list and append the engine's reply before your next move.
It prints the board after every exchange, so you always see the current
position. Think time defaults to 1000 ms; set it with `--ms`.

## The raw UCI way

The engine also takes UCI on stdin. Each turn is one invocation with the full
move list (stateless, which is easy to script):

```
printf 'position startpos moves e2e4 e7e5\ngo movetime 1000\nquit\n' \
  | /home/deepseek/liteblue/liteblue | tail -1
```

Useful commands: `position [startpos|fen <FEN>] [moves ...]`,
`go [depth|movetime|wtime|btime|nodes|infinite]`, `d` (print the board),
`perft N`, `setoption name Hash value <MB>`, `quit`. UCI line output (`info
depth ... score cp ... pv ...`) is on stdout; the move is the `bestmove` line.

## Humans

Over SSH, run the interactive front end:

```
/home/deepseek/liteblue/tui
```

Type a move (`e2e4`, `e7e8q` to promote) and the engine replies. Other commands:
`d`, `undo`, `new`, `go`, `time <ms>`, `quit`.

## Watching it think

```
printf 'position startpos\ngo depth 8\nquit\n' \
  | /home/deepseek/liteblue/liteblue
```

Prints the score, nodes, nps and principal variation at each depth. On the 604e
it searches at roughly 47,000 nodes per second (measured; see the project
notes), so `go depth 8` takes well under a second and `movetime 1000` reaches
about depth 8-9.

## A note on style

Lite Blue is roughly club strength. It plays a sound opening but will
occasionally walk its king up the board, because it has no explicit king-safety
term yet. It is honest about being symbolic alpha-beta search with a hand-tuned
evaluation, not a learned one.
