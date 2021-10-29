CC=gcc
CFLAGS= -Wall -Werror -pedantic -g

all: writer reader

writer: writenoncanonical.c
	$(CC) $(CFLAGS) $< -o wserial
reader: noncanonical.c
	$(CC) $(CFLAGS) $< -o rserial

.PHONY: clean
clean:
	rm -f wserial rserial
