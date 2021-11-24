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

#define IS_ESCAPE(c) (c == ESCAPE)
#define IS_FLAG(c) (c == FLAG)
#define ESCAPED_BYTE(c) (IS_ESCAPE(c) || IS_FLAG(c))

/* commands */ 
typedef enum { SET, DISC, UA, RR_0 , REJ_0, RR_1, REJ_1 } frameCmd;
static const uint8_t cmds[7] = { 0x3, 0xB, 0x7, 0x5, 0x1, 0x85, 0x81 };
static const char cmds_str[7][6] = { "SET", "DISC", "UA", "RR_0", "REJ_0", "RR_1", "REJ_1" };

/* reading */
typedef enum { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, DATA, STOP } readState;

/* global variables */
static struct termios oldtio, newtio;
static int port_fd;
static uint8_t connector, sequence_number = 0x0, retries = 0;

static int 
term_conf_init(int port)
{
        char filename[12];
        snprintf(filename, 12, "/dev/ttyS%d", port);

        port_fd = open(filename, O_RDWR | O_NOCTTY);
        if (port_fd < 0) {
                fprintf(stderr, "err: open() -> code: %d\n", errno);
                return -1;
        }

        if (tcgetattr(port_fd, &oldtio) == -1) {
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

        tcflush(port_fd, TCIOFLUSH);
        if (tcsetattr(port_fd, TCSANOW, &newtio) == -1) {
                fprintf(stderr, "err: tcsetattr() -> code: %d\n", errno);
                return -1;
        }

        fprintf(stdout, "log: new term attributes set\n");
        return port_fd;
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

        // ++retries;
        fprintf(stdout, "log: sent frame with %s @ %s\n", cmds_str[cmd], 
                (addr == TRANSMITTER) ? "TRANSMITTER" : "RECEIVER");
        return 0;
}

static int 
read_frame_US(int fd, uint8_t cmd_mask, uint8_t addr)
{
        readState st = START;
        struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
        uint8_t frame[5];
        int i, j;

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
                        st = IS_FLAG(frame[st]) ? FLAG_RCV : START;
                        break;
                case FLAG_RCV:
                        if (frame[st] == addr)
                                st = A_RCV;
                        else if (frame[st] != FLAG)
                                st = START;
                        break;
                case A_RCV:
                        for (i = 0; i < 7; i++) {
                                if ((cmd_mask & (0x01 << i)) && frame[st] == cmds[i]) {
                                        st = C_RCV;
                                        j = i;
                                }
                        }

                        if (st != C_RCV) {
                                st = IS_FLAG(frame[st]) ? FLAG_RCV : START;
                                frame[0] = FLAG;
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
                        st = IS_FLAG(frame[st]) ? STOP : START;
                        break;
                default:
                        break;
                }
        }

         if (connector == TRANSMITTER) {
                if (frame[2] == cmds[RR_1] || frame[2] == cmds[REJ_1])
                        sequence_number = 0x40;
                else if (frame[2] == cmds[RR_0] || frame[2] == cmds[REJ_0])
                        sequence_number = 0x0;
        }
        
        fprintf(stdout, "log: read frame with %s @ %s\n", cmds_str[j], 
                (addr == TRANSMITTER) ? "TRANSMITTER" : "RECEIVER");
        return 0;
}

void 
transmitter_alrm_handler_open(int unused) 
{
        alarm(TIMEOUT);
        send_frame_US(port_fd, SET, RECEIVER);
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
llopen(int port, uint8_t endpt)
{
        int fd;
        fd = term_conf_init(port);
        if (fd < 0)
                return -1;
        
        connector = endpt;
        connector == RECEIVER ? llopen_receiver(fd) : llopen_transmitter(fd);
        return fd;
}


/*
static void 
transmitter_alrm_handler_write(int unused)
{
        //llwrite(port_fd, temp_buffer, temp_buffer_len);
}
*/

static void
encode_cpy(uint8_t *dest, uint32_t offset, uint8_t c) {
        if (ESCAPED_BYTE(c)) {
                dest[offset] = ESCAPE;
                dest[offset+1] = c ^ 0x20;
        } else {
                dest[offset] = c;
        }
}

static ssize_t
encode_data(uint8_t **dest, const uint8_t *src, uint32_t len)
{
        int i;
        uint8_t bcc = src[0];
        for (i = 1; i < len; i++)
                bcc ^= src[i];

        uint32_t inc = 0;
        for (i = 0; i < len; i++)
                inc += ESCAPED_BYTE(src[i]);
        
        uint32_t nlen = len + inc + ESCAPED_BYTE(bcc) + 1;
        *dest = (uint8_t *)malloc(nlen);
        if (dest == NULL) {
                fprintf(stderr, "err: malloc() -> code: %d\n", errno);
                return -1;
        }

        int j;
        for (i = 0, j = 0; j < len; i += ESCAPED_BYTE(src[j]) + 1, j++)
                encode_cpy(*dest, i, src[j]);
        encode_cpy(*dest, len + inc, bcc);
        
        return nlen;
}

ssize_t
llwrite(int fd, uint8_t *buffer, uint32_t len)
{
        uint8_t *data = NULL;
        if ((len = encode_data(&data, buffer, len)) < 0) {
                fprintf(stderr, "err: encode_data() -> code: %d\n", len);
                return len;
        }
        fprintf(stdout, "log: frame no. %d of length %d ready to be sent\n", sequence_number, len);

        uint8_t frame[len+5];
        frame[0] = frame[len+4] = FLAG;
        frame[1] = TRANSMITTER;
        frame[2] = sequence_number;
        frame[3] = frame[1] ^ frame[2];

        memcpy(frame + 4, data, len);
        free(data);

        printf("log: bcc: %02x, seq_num: %02x\n", frame[len+3], frame[2]);
        // ++retries;
        ssize_t wb;
        if ((wb = write(fd, frame, sizeof(frame))) < 0) {
                fprintf(stderr, "err: read() -> code: %d\n", errno);
                return -1;
        }

        fprintf(stdout, "log: frame no. %d of length %d sent -> wb: %ld\n", sequence_number, len, wb);
        fprintf(stdout, "log: waiting on response from RECEIVER for frame no. %d\n", sequence_number);
        // signal(SIGALRM, transmitter_alrm_handler_write);
        // alarm(TIMEOUT);

        uint8_t mask = 1 << RR_0 | 1 << REJ_0 | 1 << RR_1 | 1 << REJ_1;
        read_frame_US(fd, mask, RECEIVER);

        // alarm(0);
        retries = 0;

        return wb;
}


static ssize_t
decode_data(uint8_t *dest, const uint8_t *src, uint32_t len)
{
        ssize_t i, j;
        uint32_t dec = 0;
        for (i = 0; i < len; i++)
                dec += IS_ESCAPE(src[i]);

        for (i = 0, j = 0; j < len - dec; i++, j++)
                dest[j] = IS_ESCAPE(src[i]) ? (src[++i] ^ 0x20) : src[i];

        return len - dec;
}

ssize_t
llread(int fd, uint8_t *buffer)
{
        sleep(1);
        readState st = START;
        struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };

        uint8_t frame[2*MAX_PACKET_SIZE+5];
        uint8_t disconnect = 0;
        uint32_t c = 0;

        while (st != STOP) {
                poll(&pfd, 1, 0);
                if (pfd.revents & POLLIN) {
                        if (read(fd, frame + st + c, 1) < 0) {
                                fprintf(stderr, "err: read() -> code: %d\n", errno);
                                return -1;
                        }
                }

                switch (st) {
                case START:
                        st = IS_FLAG(frame[st]) ? FLAG_RCV : START;
                        break;
                case FLAG_RCV:
                        if (frame[st] == TRANSMITTER)
                                st = A_RCV;
                        else if (frame[st] != FLAG)
                                st = START;
                        break;
                case A_RCV:
                        if (frame[st] == 0x0 || frame[st] == (1 << 6)) {
                                printf("seq_num: %d\n", sequence_number);
                                st = C_RCV;
                        } else if (frame[st] == cmds[DISC]) {
                                st = C_RCV;
                                disconnect = 1;
                        } else if (IS_FLAG(frame[st])) {
                                st = FLAG_RCV;
                                frame[0] = FLAG;
                        } else {
                                st = START;
                        }
                        break;
                case C_RCV:
                        if (frame[st] == (frame[st-1] ^ frame[st-2])) {
                                st = BCC_OK;
                        } else if (IS_FLAG(frame[st])) {
                                st = FLAG_RCV;
                                frame[0] = FLAG;
                        } else {
                                st = START;
                        }
                        break;
                case BCC_OK:
                        st = IS_FLAG(frame[st]) ? STOP : DATA; 
                        break;
                case DATA:
                        st = IS_FLAG(frame[st+c]) ? STOP : DATA;
                        c++;
                        break;
                default:
                        break;
                }
        }

        if (disconnect) {
                fprintf(stdout, "log: disconnect frame detected\n");
                send_frame_US(fd, DISC, RECEIVER);
                return -1;
        }

        uint32_t len;
        len = decode_data(buffer, frame + 4, c);

        int i;
        const uint8_t expected_bcc = buffer[len-1];
        uint8_t bcc = buffer[0];
        for (i = 1; i < len - 1; i++)
                bcc ^= buffer[i];

        fprintf(stdout, "log: bcc = %02x, expected_bcc = %02x\n", bcc, expected_bcc);

        uint8_t cmd;
        if (bcc != expected_bcc)
                cmd = sequence_number ? REJ_1 : REJ_0;
        else
                cmd = sequence_number ? RR_0 : RR_1;

        sequence_number = !sequence_number;

        send_frame_US(fd, cmd, RECEIVER);
        return (bcc == expected_bcc) ? len : -1;
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

