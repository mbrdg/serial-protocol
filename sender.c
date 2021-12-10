/*
 * sender.c
 * Serial port protocol sender application
 * RC @ L.EIC 2122
 * authors: Miguel rodrigues & Nuno castro
 */

#include <sys/stat.h>

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "application.h"
#include "protocol.h"
#include "utils.h"


int
main (int argc, char **argv)
{
        if (argc < 3) {
                fprintf(stderr, "usage: %s <port> <filename>\n", argv[0]);
                return 1;
        }
#ifdef debug
        const clock_t begin = bclk();
#endif
        int fd_file;
        fd_file = open(argv[2], O_RDONLY);
        passert(fd_file >= 0, "sender.c :: open", -1);

        int fd;
        fd = llopen(atoi(argv[1]), TRANSMITTER);
        passert(fd >= 0, "sender.c :: llopen", -1);

        uint8_t frag[MAX_PACKET_SIZE];

        struct stat st;
        fstat(fd_file, &st);
        const off_t size_file = st.st_size;

        frag[0] = START;
        frag[1] = SIZE;
        frag[2] = sizeof(off_t);
        memcpy(frag + 3, &size_file, sizeof(off_t));

        int wb;
        wb = llwrite(fd, frag, 3 + sizeof(off_t));
        passert(wb >= 0, "sender.c :: llwrite", -1);  

        uint16_t n;
        n = size_file / (MAX_PACKET_SIZE - 4);
        n += (size_file % (MAX_PACKET_SIZE - 4));

        ssize_t rb;
        int i;
        for (i = 0; i < n; i++) {
                rb = read(fd_file, frag + 4, MAX_PACKET_SIZE - 4);    

                frag[0] = DATA;
                frag[1] = i % 255;
                frag[2] = rb / 256;
                frag[3] = rb % 256;

                wb = llwrite(fd, frag, rb + 4);
                passert(wb >= 0, "sender.c :: llwrite", -1);
        }

        frag[0] = STOP;
        frag[1] = SIZE;
        frag[2] = sizeof(off_t);
        memcpy(frag + 3, &size_file, sizeof(off_t));

        wb = llwrite(fd, frag, 3 + sizeof(off_t));
        passert(wb >= 0, "sender.c :: llwrite", -1);

        llclose(fd);
        close(fd_file);
#ifdef debug
        eclk(&begin);
#endif
        return 0;
}

