/*
 * protocol.c
 * Serial port protocol
 * RC @ L.EIC 2122
 * Authors: Miguel Rodrigues & Nuno Castro
 */

#include "protocol.h"

/* macros */
#define BAUDRATE B38400
#define TIMEOUT 3
#define MAX_RETRIES 3

#define FLAG 0x7E
#define ESCAPE 0x7D

/* enums */ 
typedef enum { SET, DISC, UA, RR_0 , REJ_0, RR_1, REJ_1 } frameCmd;
static unsigned const char cmds[7] = { 0x03, 0x0B, 0x07, 0x03, 0x01, 0x83, 0x81 };
typedef enum { DISCONNECT = -2, ERROR = -1, OK = 0 } errorRead;

/* global variables */
static struct termios oldtio, newtio;
static int filedscptr;
static uint32_t temp_buffer_len;
static uint8_t connector, *temp_buffer, retries = 0, seqnum = 0;

static int 
term_conf_init(uint16_t port)
{
        char filename[12];
        snprintf(filename, 12, "/dev/ttyS%d", port);

        filedscptr = open(filename, O_RDWR | O_NOCTTY);
        if (filedscptr < 0) {
                fprintf(stderr, "err: open() -> code: %d\n", errno);
                return -1;
        }

        if (tcgetattr(filedscptr, &oldtio) == -1) {
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

        tcflush(filedscptr, TCIOFLUSH);
        if (tcsetattr(filedscptr, TCSANOW, &newtio) == -1) {
                fprintf(stderr, "err: tcsetattr() -> code: %d\n", errno);
                return -1;
        }

        fprintf(stdout, "log: new term attributes set\n");
        return filedscptr;
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
send_frame_US(int fd, uint8_t cmd, uint8_t addr) 
{
        unsigned char frame[5];
        frame[0] = FLAG;
        frame[1] = addr;
        frame[2] = cmds[cmd];
        frame[3] = frame[1] ^ frame[2];
        frame[4] = FLAG;

        if (write(fd, frame, sizeof(frame)) < 0) {
                fprintf(stderr, "err: write() code: %d\n", errno);
                return -1;
        }

        ++retries;
        fprintf(stdout, "log: sent frame with %d @ %d\n", cmd, addr);

        return 0;
}

static int 
read_frame_US(int fd, uint8_t cmd_mask, uint8_t addr)
{
        fprintf(stdout, "log: cmd_mask -> %d\n", cmd_mask);

        enum state { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP };
        enum state st = START;
        struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
        uint8_t frame[5], i;

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
                        for (i = 0; i < 7; i++) {
                                if (((cmd_mask >> i) & 0x01) && frame[st] == cmds[i]) {
                                        st = C_RCV;
                                        goto exit_A_RCV;
                                }
                        }

                        if (frame[st] == FLAG) {
                                st = FLAG_RCV;
                                frame[0] = FLAG;
                        } else {
                                st = START;
                        }
exit_A_RCV:
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
                        st = (frame[st] == FLAG) ? STOP : START;
                        break;
                default:
                        break;
                }
        }

        return 0;
}

void 
transmitter_alrm_handler_open(int unused) 
{
        alarm(TIMEOUT);
        send_frame_US(filedscptr, SET, RECEIVER);
}


static int 
llopen_receiver(int fd)
{
        read_frame_US(fd, (1 << SET), TRANSMITTER);
        send_frame_US(fd, UA, RECEIVER);
        
        return 0;
}

static int 
llopen_transmitter(int fd)
{
        signal(SIGALRM, transmitter_alrm_handler_open);

        alarm(TIMEOUT);
        send_frame_US(fd, SET, TRANSMITTER);
        read_frame_US(fd, (1 << UA), RECEIVER);
        alarm(0);
        retries = 0;

        return 0;
}

int 
llopen(int porta, uint8_t endpt)
{
        int fd;
        fd = term_conf_init(porta);
        if (fd < 0)
                return -1;
        
        connector = endpt;
        connector == RECEIVER ? llopen_receiver(fd) : llopen_transmitter(fd);
        return 0;
}


static uint8_t *
stuff_data(uint8_t *buffer, uint32_t *len)
{
        uint32_t i, inc;
        uint8_t bcc = buffer[0];
        for (i = 1; i < (*len); i++)
                bcc ^= buffer[i];

        for (i = 0; i < (*len); i++)
                if (buffer[i] == ESCAPE || buffer[i] == FLAG)
                    inc++;
        
        uint8_t nlen = (*len) + inc + 1;
        nlen = (bcc == FLAG || bcc == ESCAPE) ? nlen + 1 : nlen;

        uint8_t *data = (uint8_t *)malloc(nlen);
        if (data == NULL) {
                fprintf(stderr, "err: malloc() not enough memory\n");
                return NULL;
        }

        for (i = 0; i < nlen; i++) {
                if (buffer[i] == ESCAPE) {
                        data[i] = ESCAPE;
                        data[i+1] = 0x5D;
                        i++;
                } else if (buffer[i] == FLAG) {
                        data[i] = ESCAPE;
                        data[i+1] = 0x5E;
                        i++;
                } else {
                        data[i] = buffer[i];
                }
        }
        
        (*len) = nlen;
        return data;
}

static int
read_info_frame_response(int fd, uint8_t addr)
{
        uint8_t mask = 1 << RR_0 | 1 << RR_1 | 1 << REJ_0 | 1 << REJ_1;
        return read_frame_US(fd, mask, addr);
}

static void 
transmitter_alrm_handler_write(int unused)
{
        llwrite(filedscptr, temp_buffer, temp_buffer_len);
}

int
llwrite(int fd, uint8_t *buffer, uint32_t len)
{
        temp_buffer = buffer;
        temp_buffer_len = len;

        uint8_t *data;
        data = stuff_data(buffer, &len);
        if (data == NULL)
            return -1;

        uint8_t frame[len+6];
        frame[0] = FLAG;
        frame[1] = TRANSMITTER;
        frame[2] = (seqnum & 0x01) << 7;
        frame[3] = frame[1] ^ frame[2];
        memcpy(frame + 4, data, len);
        frame[len+5] = FLAG;

        free(data);
        ++retries;

        int wb;
        if ((wb = write(fd, frame, len)) < 0) {
                fprintf(stderr, "err: read() -> code: %d\n", errno);
                return -1;
        }

        signal(SIGALRM, transmitter_alrm_handler_write);

        alarm(TIMEOUT);
        read_info_frame_response(fd, RECEIVER);
        alarm(0);
        retries = 0;

        return wb;
}


static int
destuff_data(uint8_t *buffer, uint32_t *len)
{
        uint32_t i, j, inc;
        for (i = 0; i < (*len); i++)
            inc += (buffer[i] == ESCAPE);

        uint32_t nlen = (*len) - inc;
        uint8_t *data;
        data = (uint8_t *)malloc(nlen);
        if (data == NULL) {
                fprintf(stderr, "err: malloc() not enough memory\n");
                return -255;
        }

        for (i = 0, j = 0; i < (*len); j++, i++) {
                if (buffer[i] == ESCAPE) 
                        data[j] = (buffer[i+1] == 0x5E) ? FLAG : ESCAPE;
                else
                        data[j] = buffer[i];
                i += (buffer[i] == ESCAPE);

        }

        uint8_t bcc = data[0];
        for (i = 1; i < nlen - 1; i++)
            bcc ^= data[i];
        
        (*len) = (bcc == data[nlen-1]) ? nlen : 0;
        buffer = data;
        return -(bcc == data[nlen-1]);
}

static int 
send_info_frame_response(int fd, uint8_t addr, uint8_t* header, errorRead err)
{
        frameCmd cmd;
        if (err == ERROR) 
                cmd = (seqnum == 1) ? REJ_1 : REJ_0;
        else if (err == DISCONNECT)
                cmd = DISC;
        else
                cmd = (seqnum == 1) ? RR_0 : RR_1;
        return send_frame_US(fd, cmd, addr);
}

int
llread(int fd, uint8_t *buffer)
{
        enum state { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, DATA, STOP };
        enum state st = START;
        struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
        uint8_t frame_header[4], frame[MAX_PACKET_SIZE], c_read, prev_seqnum = seqnum;
        uint32_t c = 0;
        errorRead err = OK;

        while (st != STOP) {
                poll(&pfd, 1, 0);
                if (read(fd, &c_read, 1) == -1) {
                        fprintf(stderr, "err: read() -> code: %d\n", errno);
                        return -1;
                }

                switch (st) {
                case START:
                        frame_header[st] = c_read;
                        st = (frame_header[st] == FLAG) ? FLAG_RCV : START;
                        break;
                case FLAG_RCV:
                        frame_header[st] = c_read;
                        if (frame_header[st] == TRANSMITTER)
                                st = A_RCV;
                        else if (frame_header[st] != FLAG)
                                st = START;
                        break;
                case A_RCV:
                        frame_header[st] = c_read;
                        if (frame_header[st] == 0x00 || frame_header[st] == 0x70) {
                                st = C_RCV;
                                seqnum = (frame_header[st] >> 7) & 0x01;
                        } else if (frame_header[st] == DISC) {
                                st = C_RCV;
                                err = DISCONNECT;  
                        } else if (frame_header[st] == FLAG) {
                                st = FLAG_RCV;
                                frame_header[0] = FLAG;
                        } else {
                                st = START;
                        }
                        break;
                case C_RCV:
                        frame_header[st] = c_read;
                        if (frame_header[st] == (frame[st-1] ^ frame[st-2])) {
                                st = BCC_OK;
                        } else if (frame_header[st] == FLAG) {
                                st = (err == DISCONNECT) ? STOP : FLAG_RCV;
                                frame_header[0] = FLAG;
                        } else {
                                st = START;
                        }
                        break;
                case DATA:
                case BCC_OK:
                        frame[c] = c_read;
                        st = (frame[c] == FLAG) ? STOP : DATA; 
                        c++;
                        break;
                default:
                        break;
                }
        }

        uint32_t len = sizeof(frame) - 1;
        destuff_data(frame, &len);
        
        err = (len == -1) ? ERROR : err;
        send_info_frame_response(fd, RECEIVER, frame_header, err);
        if (len < 0)
            return err;

        len = (prev_seqnum == seqnum) ? 0 : len;
        return len;
}


int
llclose(int fd)
{
        if (connector == TRANSMITTER) {
                send_frame_US(fd, DISC, TRANSMITTER);
                read_frame_US(fd, (1 << DISC), RECEIVER);
                send_frame_US(fd, UA, TRANSMITTER);
        }

        sleep(2); /* Gives time to all the info flow througth the communication channel */
        return term_conf_end(fd);
}

