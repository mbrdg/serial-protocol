CC=gcc
CFLAGS= -Wall -Werror -pedantic -g

all: sender receiver

sender: sender.c
	$(CC) $(CFLAGS) $< -o sndr
receiver: receiver.c
	$(CC) $(CFLAGS) $< -o recv

.PHONY: clean
clean:
	rm -f sndr recv
