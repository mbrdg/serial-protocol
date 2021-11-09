/*
 * receiver.c
 * Non-Canonical Input Processing
 * RC @ L.EIC 2122
 * Author: Miguel Rodrigues
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#define BAUDRATE B38400
#define FLAG 0x7E

int
read_set_msg(int tty_fd)
{
        const char ERROR[] = "error: [SET]";

        unsigned char set[5];
        int rb;
        rb = read(tty_fd, set, sizeof(set));

        if (rb == -1) 
        {
                fprintf(stderr, "%s read() code: %d\n", ERROR, errno);
                goto error_handler;
        }
        
        if (set[0] != set[4] || set[0] != FLAG)
        {
                fprintf(stderr, "%s flags not found\n", ERROR);
                goto error_handler;
        }
        if (set[3] != (set[1] ^ set[2]))
        {
                fprintf(stderr, "%s parity can't be checked\n", ERROR);
                goto error_handler;
        }

        return 0;

error_handler:
        fprintf(stderr, "%s can't establish a connection\n", ERROR);
        return -1;
}

int 
send_ua_msg(int tty_fd) 
{
        const unsigned char CMD_SNT = 0x01;
        const unsigned char UA_CMD = 0x07;

        unsigned char ua[5];
        ua[0] = FLAG;
        ua[1] = CMD_SNT;
        ua[2] = UA_CMD;
        ua[3] = ua[1] ^ ua[2];
        ua[4] = FLAG;

        int wb;
        wb = write(tty_fd, ua, sizeof(ua));

        if (wb == -1) 
        {
                fprintf(stderr, "error: [UA] write() code: %d\n", errno);
                return -1;
        }
        
        fprintf(stdout, "info: [UA] sent to the sender\n");
        return 0;
}


int 
main(int argc, char **argv)
{
        if (argc < 2)
        {
                fprintf(stderr, "usage: %s <serialport>\n", argv[0]);
                return 1;
        }

        struct termios oldtio, newtio;

        /*  Open serial port device for reading and writing and not as controlling
            tty because we don't want to get killed if linenoise sends CTRL-C. */
        int fd;
        fd = open(argv[1], O_RDWR | O_NOCTTY);
        if (fd < 0) 
        {
                fprintf(stderr, "error: open() code: %d\n", errno);
                return -1;
        }

        /* save current port settings */
        if (tcgetattr(fd, &oldtio) == -1) 
        {
                fprintf(stderr, "error: tcgetattr() code: %d\n", errno);
                return -1;
        }

        memset(&newtio, '\0', sizeof(newtio));
        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;

        /* set input mode (non-canonical, no echo,...) */
        newtio.c_lflag = 0;

        newtio.c_cc[VTIME] = 0;  /* inter-character timer unused */
        newtio.c_cc[VMIN] = 5;   /* blocking read until 5 chars received */

        /*  VTIME e VMIN devem ser alterados de forma a proteger com
            um temporizador a leitura do(s) proximo(s) caracter(es) */
        tcflush(fd, TCIOFLUSH);
        if (tcsetattr(fd,TCSANOW,&newtio) == -1) 
        {
                fprintf(stderr, "error: tcsetattr() code: %d\n", errno);
                return -1;
        }

        fprintf(stdout, "info: new termios structure set\n");

        read_set_msg(fd);
        send_ua_msg(fd);

        sleep(2);
        if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
        {
                fprintf(stderr, "error: tcsetattr() code: %d\n", errno);
                return -1;
        }
        
        close(fd);
        return 0;
}

