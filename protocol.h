#ifndef _PROTOCOL_H_

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

typedef enum { RECEIVER = 0x01, TRANSMITTER = 0x03 } comStatus;

int
llopen(int porta, comStatus status);

int
llwrite(int fd, char *buffer, int len);

int
llread(int fd, char *buffer);

int 
llclose(int fd);

#endif /* _PROTOCOL_H_ */

