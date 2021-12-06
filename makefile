CC=cc
CFLAGS= -Wall -Werror -pedantic -g

OPTIONS= -D BAUDRATE=B38400 -D TIMEOUT=3 -D MAX_RETRIES=3
DEBUG= -D DEBUG

OBJ=utils.c protocol.c

all: sender receiver

sender: $(OBJ) sender.c
	$(CC) $(CFLAGS) $(OPTIONS) $(DEBUG) $^ -o sndr
receiver: $(OBJ) receiver.c
	$(CC) $(CFLAGS) $(OPTIONS) $(DEBUG) $^ -o recv

.PHONY: clean setup
setup:
	sudo socat -d -d PTY,link=/dev/ttyS10,mode=777 PTY,link=/dev/ttyS11,mode=777
clean:
	rm -f sndr recv
