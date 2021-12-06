CC=cc
CFLAGS= -Wall -Werror -pedantic -g

OPTIONS= -D BAUDRATE=B38400 -D TIMEOUT=3 -D MAX_RETRIES=3 -D MAX_PACKET_SIZE=100
DEBUG= -D DEBUG

OBJ=utils.c protocol.c

all: sender receiver docs

sender: $(OBJ) sender.c
	$(CC) $(CFLAGS) $(OPTIONS) $(DEBUG) $^ -o sndr
receiver: $(OBJ) receiver.c
	$(CC) $(CFLAGS) $(OPTIONS) $(DEBUG) $^ -o recv

.PHONY: setup docs clean
setup:
	sudo socat -d -d PTY,link=/dev/ttyS10,mode=777 PTY,link=/dev/ttyS11,mode=777
docs:
	pandoc -V lang=pt -V geometry:margin=1in -V fontsize=11pt --toc README.md -s -o README.pdf
clean:
	rm -f sndr recv README.pdf

