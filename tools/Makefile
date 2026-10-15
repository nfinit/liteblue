# Companion tools for Lite Blue.
# The match harness links the engine's board code from ../src.

CC      = cc
CFLAGS  = -O2
LDLIBS  = -lm

all: match

match: match.c ../src/board.c ../src/movegen.c ../src/board.h ../src/movegen.h
	$(CC) $(CFLAGS) -I../src -o $@ match.c ../src/board.c ../src/movegen.c $(LDLIBS)

clean:
	rm -f match
