CC=gcc
CFLAGS= -Wall -Werror -pedantic -g

all: emitter receiver

emitter: emitter.c
	$(CC) $(CFLAGS) $< -o wserial
receiver: receiver.c
	$(CC) $(CFLAGS) $< -o rserial

.PHONY: clean
clean:
	rm -f wserial rserial
