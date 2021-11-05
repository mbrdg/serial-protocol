CC=gcc
CFLAGS= -Wall -Werror -pedantic -g

all: sender receiver

sender: sender.c
	$(CC) $(CFLAGS) $< -o wserial
receiver: receiver.c
	$(CC) $(CFLAGS) $< -o rserial

.PHONY: clean
clean:
	rm -f wserial rserial
