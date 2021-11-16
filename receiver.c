/*
 * receiver.c
 * Non-Canonical Input Processing
 * RC @ L.EIC 2122
 * Author: Miguel Rodrigues
 */

#include "protocol.h"

int 
main(int argc, char **argv)
{
        if (argc < 2)
        {
                fprintf(stderr, "usage: %s <port>\n", argv[0]);
                return 1;
        }

        int fd;
        fd = llopen(atoi(argv[1]), RECEIVER);
        llclose(fd);

        return 0;
}

