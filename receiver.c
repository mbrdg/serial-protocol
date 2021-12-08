/*
 * receiver.c
 * Serial port protocol receiver application
 * RC @ L.EIC 2122
 * Authors: Miguel Rodrigues & Nuno Castro
 */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "application.h"
#include "protocol.h"
#include "utils.h"


int 
main(int argc, char **argv)
{
        if (argc < 3) {
                fprintf(stderr, "usage: %s <port> <filename>\n", argv[0]);
                return 1;
        }

        int fd_file;
        fd_file = open(argv[2], O_CREAT | O_WRONLY, 0666);
        passert(fd_file >= 0, "receiver.c :: open", -1);

        int fd;
        fd = llopen(atoi(argv[1]), RECEIVER);
        passert(fd >= 0, "receiver.c :: llopen", -1);

        uint8_t pkgn = 0;
        uint8_t frag[MAX_PACKET_SIZE];
        while (1) {
                ssize_t rb;
                rb = llread(fd, frag);
                if (rb < 0)
                        continue;

                ssize_t len;
                switch (frag[0]) {
                case DATA:
                        if (frag[1] >= (pkgn % 255)) {
                                len = frag[2] * 256 + frag[3];
                                write(fd_file, frag + 4, len);
                                ++pkgn;
                        }
                        break;
                case START:
                        break;
                case STOP:
                        llread(fd, frag);   /* Take the last disc frame */
                        goto finish;
                default:
                        break;
                }
        }
        
finish:
        llclose(fd);
        close(fd_file);
        return 0;
}

