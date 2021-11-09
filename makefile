CC=gcc
CFLAGS= -Wall -Werror -pedantic -g

all: sender receiver

sender: sender.c
	$(CC) $(CFLAGS) $< -o sndr
receiver: receiver.c
	$(CC) $(CFLAGS) $< -o recv

setup:
	sudo socat -d -d PTY,link=/dev/ttyS10,mode=777 PTY,link=/dev/ttyS11,mode=777

.PHONY: clean
clean:
	rm -f sndr recv
