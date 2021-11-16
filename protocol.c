#include "protocol.h"

/* macros */
#define BAUDRATE B38400
#define TIMEOUT 3 
#define MAX_RETRIES 3

#define FLAG 0x7E

/* enums */ 
enum {
        SET = 0x03,
        DISC = 0x0B,
        UA = 0x07,
        RR = 0x03,
        REJ = 0x01
} cmdFrameUS;

/* global variables */
static struct termios oldtio, newtio;
static int file_descriptor, retries = 0;

static int 
term_conf_init(int porta)
{
        char filename[12];
        snprintf(filename, 12, "/dev/ttyS%d", porta);

        int file_descriptor;
        file_descriptor = open(filename, O_RWDR | O_NOCTTY);
        if (fd < 0) {
                fprintf(stderr, "err: open() -> code: %d\n", errno);
                return -1;
        }

        if (tcgetattr(fd, &oldtio) == -1) {
                fprintf(stderr, "err: tcgetattr() -> code: %d\n", errno);
                return -1;
        }

        memset(&newtio, '\0', sizeof(newtio));

        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;
        newtio.c_lflag = 0; /* set input mode (non-canonical, no echo...) */

        newtio.c_cc[VTIME] = 0;
        newtio.c_cc[VMIN] = 1;

        tcflush(fd, TCIOFLUSH);
        if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
                fprintf(stderr, "err: tcsetattr() -> code: %d\n", errno);
                return -1;
        }

        fprintf(stdout, "log: new term attributes set\n");
        return file_descriptor;
}

static int
term_conf_end(int fd)
{
        if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
                fprintf(stderr, "err: tcsetattr() -> code: %d\n", errno);
                return -1;
        }

        close(fd);
        return 0;
}


static int
send_frame_US(int fd, cmdFrameUS cmd, comStatus status) 
{
        char frame[5];
        frame[0] = FLAG;
        frame[1] = status;
        frame[2] = cmd;
        frame[3] = frame[1] ^ frame[2];
        frame[4] = FLAG;

        int wb;
        wb = write(fd, frame, sizeof(frame));
        if (wb < 0) {
                fprintf(stderr, "err: write() code: %d\n", errno);
                return -1;
        }

        ++retries;
        return 0;
}

static int 
read_frame_US(int fd, cmdFrameUS cmd, comStatus addr)
{
        enum { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP } state;
        state st = START;
        struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
        char frame[5];

        while (st != STOP && retries < MAX_RETRIES) {
                poll(&pfd, 1, 0);
                if (pfd.revents & POLLIN) {
                        if (read(fd, frame + st, 1) == -1) {
                                fprintf(stderr, "err: read() code: %d\n", errno);
                                return -1;
                        }
                }

                switch (st) {
                case START:
                        st = (frame[st] == FLAG) ? FLAG_RCV : START;
                        break;
                case FLAG_RCV:
                        if (frame[st] == addr)
                                st = A_RCV;
                        else if (frame[st] != FLAG)
                                st = START;
                        break;
                case A_RCV:
                        if (frame[st] == cmd) {
                                st = C_RCV;
                        } else if (frame[st] == FLAG) {
                                st = FLAG_RCV;
                                frame[0] = FLAG;
                        } else {
                                st = START;
                        }
                        break;
                case C_RCV:
                        if (frame[st] == (frame[st-1] ^ frame[st-2])) {
                                st = BCC_OK;
                        } else if (frame[st] == FLAG) {
                                st = FLAG_RCV;
                                frame[0] = FLAG;
                        } else {
                                st = START;
                        }
                        break;
                case BCC_OK:
                        st = (frame[st] == FLAG) ? stop : start;
                        break;
                default:
                        break;
                }
        }

        return 0;
}

void 
transmitter_alrm_handler(int unusded) 
{
        alarm(TIMEOUT);
        send_frame_US(file_descriptor, SET, TRANSMITTER);
}


static int 
llopen_receiver(int fd)
{
        read_frame_US(fd, SET, RECEIVER);
        send_frame_US(fd, UA, RECEIVER);
        
        return 0;
}

static int 
llopen_transmitter(int fd)
{
        signal(SIGALARM, transmitter_alrm_handler);

        alarm(TIMEOUT);
        send_frame_US(fd, SET, TRANSMITTER);
        read_frame_US(fd, UA, TRANSMITTER);
        alarm(0);

        return 0;
}


int 
llopen(int porta, comStatus status)
{
        int fd;
        fd = term_conf_init(porta);
        if (fd < 0)
                return -1;

        if (comStatus == RECEIVER)
                llopen_receiver(fd);
        else
                llopen_transmitter(fd);
}


int
llwrite(int fd, char *buffer, int len)
{
    return 0;
}


int 
llread(int fd, char *buffer)
{
    return 0;
}


int
llclose(int fd)
{
        sleep(2);
        return term_conf_end(fd);
}

