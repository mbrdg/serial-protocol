CC=cc
CFLAGS= -Wall -Werror -pedantic -g

BIN=./bin
DOC=./doc

OPTIONS= -D BAUDRATE=B38400 -D TOUT=10 -D MAX_RETRIES=3 -D MAX_PACKET_SIZE=100
STATS=-D FER=0 -D TPROP=0  # FER must be a value between 0 and 100
DEBUG= -D DEBUG

all: sndr recv docs

OBJ=utils.c protocol.c

sndr: $(OBJ) sender.c
	$(CC) $(CFLAGS) $(OPTIONS) $(STATS) $(DEBUG) $^ -o $(BIN)/$@
recv: $(OBJ) receiver.c
	$(CC) $(CFLAGS) $(OPTIONS) $(STATS) $(DEBUG) $^ -o $(BIN)/$@

.PHONY: setup docs clean
setup:
	socat -d -d PTY,link=/dev/ttyS10,mode=777 PTY,link=/dev/ttyS11,mode=777
docs:
	pandoc -V lang=pt -V geometry:margin=1in -V fontsize=11pt --toc README.md -s -o $(DOC)/README.pdf
clean:
	rm -f $(BIN)/* $(DOC)/README.pdf

