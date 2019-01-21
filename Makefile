.PHONY: all clean

BIN := tests

CFLAGS += -ggdb3 -std=gnu99

$(BIN): ctrie.c tests.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f -- $(BIN) vgcore.*
