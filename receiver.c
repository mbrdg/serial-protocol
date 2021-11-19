/*
 * receiver.c
 * Non-Canonical Input Processing
 * RC @ L.EIC 2122
 * Author: Miguel Rodrigues
 */

#include <stdint.h>

#include "protocol.h"

int 
main(int argc, char **argv)
{
        if (argc < 3)
        {
                fprintf(stderr, "usage: %s <port> <filename>\n", argv[0]);
                return 1;
        }

        int fd;
        fd = llopen(atoi(argv[1]), RECEIVER);

        int fd_file;
        fd_file = open(argv[2], O_CREAT | O_WRONLY, 0666);

        uint8_t *fragment = NULL;
        int i, rb, len;
        if (fd_file > 0) {
                for (i = 0; ; i++) {
                        rb = llread(fd, fragment);
                        if (rb == -2)
                            break;
                        
                        switch (fragment[0]) {
                        case 0x01:
                                len = fragment[2] * 256 + fragment[3];
                                write(fd_file, fragment + 4, len);
                                free(fragment);
                                break;
                        case 0x02:
                                break;
                        case 0x03:
                                break;
                        default:
                                break;
                        }
                }
        }

        close(fd_file);
        llclose(fd);

        return 0;
}

