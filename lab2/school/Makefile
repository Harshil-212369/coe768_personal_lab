# Lab 2 school build: must stay warning-free on the Fedora lab VM.
CC     = gcc
CFLAGS = -std=gnu89 -Wall -Wextra
BIN    = build

all: $(BIN)/hello_server $(BIN)/hello_client

$(BIN)/%: %.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN):
	mkdir -p $(BIN)

clean:
	rm -rf $(BIN)

.PHONY: all clean
