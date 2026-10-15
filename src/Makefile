# Lite Blue - a chess engine for obsidian (C89).
# Builds with xlc, gcc or cc. On obsidian: /usr/vac/bin/xlc or /usr/local/bin/gcc.

CC      = cc
CFLAGS  = -O2

CORE    = board.c movegen.c eval.c tt.c search.c
HDRS    = types.h board.h movegen.h eval.h tt.h search.h

all: perft liteblue tui arena

perft: perft.c board.c movegen.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ perft.c board.c movegen.c

liteblue: liteblue.c $(CORE) $(HDRS)
	$(CC) $(CFLAGS) -o $@ liteblue.c $(CORE)

tui: tui.c $(CORE) $(HDRS)
	$(CC) $(CFLAGS) -o $@ tui.c $(CORE)

arena: arena.c $(CORE) $(HDRS)
	$(CC) $(CFLAGS) -o $@ arena.c $(CORE)

clean:
	rm -f perft liteblue tui arena
