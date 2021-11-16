/*
 * protocol.h
 * Serial port protocol
 * RC @ L.EIC 2122
 * Authors: Miguel Rodrigues & Nuno Castro
 */

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

#define RECEIVER 0x01
#define TRANSMITTER 0x03

/***
 * Sets up the terminal, in order to send information packets
 * @param int[in] - port x corresponding to the file /dev/ttySx
 * @param char[in] - determines whether is the RECEIVER or TRANSMITTER called
 * @param int[out] - file descriptor corresponding to the opened file 
 */
int
llopen(int port, char endpt);

/***
 * Writes a given chunck of information in the file pointed by the first param
 * @param int[in] - file descriptor pointing to the file where information will be written
 * @param char *[in] - information to be written
 * @param int[in] - size in bytes of the chunck of information 
 * @param int[out] - number of bytes written
 */
int
llwrite(int fd, char *buffer, int len);

/***
 * Reads a given chunck of information in the file pointed by the first param
 * @param int[in] - file descriptor pointing to the file where information will be read
 * @param char *[in] - place where to place the information after performing the reading 
 * @param int[out] - number of bytes read
 */
int
llread(int fd, char *buffer);

/***
 * Reverts to the previous terminal settings and shutdowns all the resources in use
 * @param int[in] - file descriptor corresponding to the opened file 
 * @param int[out] - 0 if no errors occur, negative value otherwise
 */
int 
llclose(int fd);

#endif /* _PROTOCOL_H_ */
