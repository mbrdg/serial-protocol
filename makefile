CC=gcc
CFLAGS= -Wall -Werror -pedantic -g

all: broadcaster receiver

broadcaster: broadcaster.c
	$(CC) $(CFLAGS) $< -o wserial
receiver: receiver.c
	$(CC) $(CFLAGS) $< -o rserial

.PHONY: clean
clean:
	rm -f wserial rserial
