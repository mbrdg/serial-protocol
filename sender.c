/*
 * sender.c
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
#include <signal.h>

#define BAUDRATE B38400
#define FLAG 0x7E

int fd, retries = 0, connection = 0;

/* forward declarations */
int send_set_msg(void);
void sig_alrm_handler(int signum) { send_set_msg(); }


int 
send_set_msg(void)
{
        const unsigned char CMD_SNT = 0x03;
        const unsigned char SET_CMD = 0x03;

        fprintf(stdout, "info: [SET] building - current try: %d\n", ++retries);

        unsigned char set[5];
        set[0] = FLAG;
        set[1] = CMD_SNT;
        set[2] = SET_CMD;
        set[3] = set[1] ^ set[2];
        set[4] = FLAG;

        /* sending the msg to the other end */
        int wb;
        wb = write(fd, set, sizeof(set));

        if (wb == -1)
        {
                fprintf(stderr, "error: [SET] write() code: %d\n", errno);
                return -1;
        }

        fprintf(stdout, "info: [SET] sent to the receiver\n");
        return 0;
}

int 
read_ua_msg(void)
{
        const unsigned char ADDR = 0x01;
        const unsigned char CMD_UA = 0x07;

        unsigned char ua[5];
        enum State { start, flag_rcv, a_rcv, c_rcv, bcc_ok, stop };
        enum State st = start;

        while (st != stop)
        {
                if (read(fd, ua + st, 1) == -1) 
                {
                        fprintf(stderr, "error: [UA] read() code %d\n", errno);
                        return -1;
                }

                switch (st) {
                case start:
                        st = (ua[st] == FLAG) ? flag_rcv : start;
                        break;
                case flag_rcv:
                        if (ua[st] == ADDR)
                                st = a_rcv;
                        else if (ua[st] != FLAG)
                                st = start;
                        break;
                case a_rcv:
                        if (ua[st] == CMD_UA)
                                st = c_rcv;
                        else if (ua[st] == FLAG)
                        {
                                st = flag_rcv;
                                ua[0] = FLAG;
                        }
                        else 
                                st = start;
                        break;
                case c_rcv:
                        if (ua[st] == (ua[st-2] ^ ua[st-1]))
                                st = bcc_ok;
                        else if (ua[st] == FLAG)
                        {
                                st = flag_rcv;
                                ua[0] = FLAG;
                        }
                        else
                                st = start;
                        break;
                case bcc_ok:
                        st = (ua[st] == FLAG) ? stop : start;
                        break;
                case stop:
                        break;
                }
        }
        
        connection = 1;
        fprintf(stdout, "info: [UA] read successfully\n");
        return 0;
}


int
main (int argc, char **argv)
{
        if (argc < 2)
        {
                fprintf(stderr, "usage: %s <serialport>\n", argv[0]);
                return 1;
        }

        struct termios oldtio, newtio;

        /*  Open serial port device for reading and writing and not as controlling
            tty because we don't want to get killed if linenoise sends CTRL-C. */
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
        newtio.c_cc[VMIN] = 1;   /* blocking read until 1 char received */

        /* VTIME e VMIN devem ser alterados de forma a proteger com um 
           temporizador a leitura do(s) proximo(s) caracter(es) */
        tcflush(fd, TCIOFLUSH);
        if (tcsetattr(fd, TCSANOW, &newtio) == -1)
        {
                fprintf(stderr, "error: tcsetattr() code: %d\n", errno);
                return -1;
        }

        fprintf(stdout, "info: new termios structure set\n");

        // signal(SIGALRM, sig_alrm_handler);
        send_set_msg();
        //while (retries < 3 && !connection)
        //{
        //        alarm(3);
                read_ua_msg();
        //}

        //alarm(0);
        if (!connection)
                fprintf(stderr, "error: connection timeout\n");
        sleep(2);

        /* revert to the old port settings */
        if (tcsetattr(fd, TCSANOW, &oldtio) == -1) 
        {
                fprintf(stderr, "error: tcsetattr() code: %d\n", errno);
                return -1;
        }

        close(fd);
        return 0;
}

