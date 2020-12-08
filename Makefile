.PHONY: all clean run-tests

BIN := tests
ASM := ctrie.s
SRCS := ctrie.c tests.c

all: $(BIN) $(ASM)

CFLAGS += -ggdb3 -std=gnu11 -Wall --pedantic -O3

$(BIN): ctrie.c tests.c Makefile
	$(CC) $(CFLAGS) -o $@ $(SRCS)

$(ASM): ctrie.c Makefile
	$(CC) $(CFLAGS) -S -o $@ $<

run-tests: $(BIN)
	valgrind ./$(BIN)

clean:
	rm -f -- $(BIN) $(ASM) vgcore.*
