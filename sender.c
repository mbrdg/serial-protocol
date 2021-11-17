/*
 * sender.c
 * Non-Canonical Input Processing 
 * RC @ L.EIC 2122
 * Author: Miguel Rodrigues & Nuno Castro
 */

#include <stdio.h>

#include "protocol.h"

int
main (int argc, char **argv)
{
        if (argc < 3)
        {
                fprintf(stderr, "usage: %s <port> <filename>\n", argv[0]);
                return 1;
        }

        int fd;
        fd = llopen(atoi(argv[1]), TRANSMITTER);

        int fd_file;
        fd_file = open(argv[2], O_RDONLY);

        char fragment[MAX_PACKET_SIZE/2];
        int fragment_len = MAX_PACKET_SIZE / 2 - 4, i;
        if (fd_file > 0) {                
                for (i = 0; read(fd_file, fragment + 4, fragment_len) > 0; i++) {
                        fragment[0] = 0x01;
                        fragment[1] = i % 255;
                        fragment[2] = fragment_len / 256;
                        fragment[3] = fragment_len % 256;

                        llwrite(fd, fragment, MAX_PACKET_SIZE / 2);
                }
        }
        
        close(fd_file);
        llclose(fd);

        return 0;
}

