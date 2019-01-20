.PHONY: all clean

BIN := tests

$(BIN): ctrie.c tests.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f -- $(BIN)
