CC      ?= gcc
CFLAGS  ?= -O2
CFLAGS  += -std=c99 -Wall -Wextra -D_DEFAULT_SOURCE

SRC = src/main.c src/sha1.c src/object.c src/refs.c src/filelist.c \
      src/tree.c src/commit.c src/util.c
OBJ = $(SRC:.c=.o)

minigit: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

src/%.o: src/%.c src/*.h
	$(CC) $(CFLAGS) -c $< -o $@

test: minigit
	sh tests/run_tests.sh

clean:
	rm -f minigit $(OBJ)

.PHONY: test clean
