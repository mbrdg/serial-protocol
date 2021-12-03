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
#define KEY 0x20

#define IS_ESCAPE(c) (c == ESCAPE)
#define IS_FLAG(c) (c == FLAG)
#define ESCAPED_BYTE(c) (IS_ESCAPE(c) || IS_FLAG(c))

#define RESEND_REQUIRED 1

#define BIT_SET(m, i) (m & (0x1 << i))

/* commands */ 
typedef enum { SET, DISC, UA, RR_0 , REJ_0, RR_1, REJ_1 } frameCmd;
static const uint8_t cmds[7] = { 0x3, 0xB, 0x7, 0x5, 0x1, 0x85, 0x81 };
static const char cmds_str[7][6] = { "SET", "DISC", "UA", "RR_0", "REJ_0", "RR_1", "REJ_1" };

/* reading */
typedef enum { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, DATA, STOP } readState;

/* global variables */
static struct termios oldtio, newtio;
static struct sigaction sigact;

static int port_fd;

static uint8_t connector;
static volatile uint8_t retries, sequence_number = 0x0;
static int connection_alive;

static uint8_t buffer_frame[2*MAX_PACKET_SIZE+5];
static ssize_t buffer_frame_len;

/* forward declarations */
static int check_resending(uint8_t cmd);

/* util funcs */
static void
install_sigalrm(void (*handler)(int))
{
        sigact.sa_handler = handler;
        sigemptyset(&sigact.sa_mask);
        sigact.sa_flags = 0;
        sigaction(SIGALRM, &sigact, NULL);
}


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
        newtio.c_cc[VMIN] = 1; /* 1 char required to satisfy a read */

        tcflush(port_fd, TCIOFLUSH);
        if (tcsetattr(port_fd, TCSANOW, &newtio) == -1) {
                fprintf(stderr, "err: tcsetattr() -> code: %d\n", errno);
                return -1;
        }

        fprintf(stdout, "log: new termios attributes set\n");
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

        frame[0] = frame[4] = FLAG;
        frame[1] = addr;
        frame[2] = cmds[cmd];
        frame[3] = frame[1] ^ frame[2];

        if (write(fd, frame, sizeof(frame)) < 0) {
                fprintf(stderr, "err: write() code: %d\n", errno);
                return -1;
        }

        fprintf(stdout, "log: frame sent with %s @ %s\n", cmds_str[cmd],
                            (addr == TRANSMITTER) ? "TRMT" : "RECV");
        return 0;
}

static int 
read_frame_US(int fd, const uint8_t cmd_mask, const uint8_t addr)
{
        readState st = START;
        uint8_t frame[5];
        ssize_t i, j, rb;

        while (st != STOP && retries < MAX_RETRIES) {
                rb = read(fd, frame + st, 1);
                if (rb < 0 && errno == EINTR)
                        continue;

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
                        for (i = 0; i < 7; i++)
                                if (BIT_SET(cmd_mask, i) && frame[st] == cmds[i]) {
                                        st = C_RCV; j = i;
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

        connection_alive = (retries < MAX_RETRIES);
        if (!connection_alive)
                return -1;

        fprintf(stdout, "log: frame read with %s @ %s\n", cmds_str[j], 
                            (addr == TRANSMITTER) ? "TRMT" : "RECV");

        uint8_t frame_I_ans = 1 << RR_0 | 1 << REJ_0 | 1 << RR_1 | 1 << REJ_1;
        if (connector == TRANSMITTER && cmd_mask == frame_I_ans)
                return check_resending(frame[2]);

        return 0;
}


void 
trmt_alrm_handler_open(int unused) 
{
        alarm(TIMEOUT);
        retries++;
        send_frame_US(port_fd, SET, TRANSMITTER);
}

static int 
llopen_recv(int fd)
{
        read_frame_US(fd, (1 << SET), TRANSMITTER);
        send_frame_US(fd, UA, RECEIVER);
        
        return 0;
}

static int 
llopen_trmt(int fd)
{
        int conn_est;

        retries = 0;
        install_sigalrm(trmt_alrm_handler_open);

        send_frame_US(fd, SET, TRANSMITTER);
        alarm(TIMEOUT);
        conn_est = read_frame_US(fd, (1 << UA), RECEIVER);
        alarm(0);

        if (!connection_alive) {
                fprintf(stderr, "err: can't establish a connection with RECV\n");
                return -1;
        }

        return conn_est;
}

int 
llopen(int port, const uint8_t endpt)
{
        int fd;
        fd = term_conf_init(port);
        if (fd < 0)
                return -1;
        
        int cnct;
        cnct = (endpt == TRANSMITTER) ? llopen_trmt(fd) : llopen_recv(fd);
        if (cnct < 0)
                return cnct;

        connector = endpt;
        return fd;
}


static void
encode_cpy(uint8_t *dest, ssize_t offset, uint8_t c) {
        if (ESCAPED_BYTE(c)) {
                dest[offset] = ESCAPE;
                dest[offset+1] = c ^ KEY;
        } else {
                dest[offset] = c;
        }
}

static ssize_t
encode_data(uint8_t **dest, const uint8_t *src, ssize_t len)
{
        ssize_t i, j;
        uint8_t bcc = src[0];
        for (i = 1; i < len; i++)
                bcc ^= src[i];

        ssize_t inc = 0;
        for (i = 0; i < len; i++)
                inc += ESCAPED_BYTE(src[i]);
        
        ssize_t nlen = len + inc + ESCAPED_BYTE(bcc) + 1;
        *dest = (uint8_t *)malloc(nlen);
        if (dest == NULL) {
                fprintf(stderr, "err: malloc() -> code: %d\n", errno);
                return -1;
        }

        for (i = 0, j = 0; j < len; i += ESCAPED_BYTE(src[j]) + 1, j++)
                encode_cpy(*dest, i, src[j]);
        encode_cpy(*dest, len + inc, bcc);
        
        return nlen;
}

static ssize_t
decode_data(uint8_t *dest, const uint8_t *src, ssize_t len)
{
        ssize_t i, j;
        ssize_t dec = 0;
        for (i = 0; i < len; i++)
                dec += IS_ESCAPE(src[i]);

        for (i = 0, j = 0; j < len - dec; i++, j++)
            dest[j] = IS_ESCAPE(src[i]) ? (src[++i] ^ KEY) : src[i];

        return len - dec;
}


static ssize_t
write_data(void)
{
        ssize_t wb;
        if ((wb = write(port_fd, buffer_frame, buffer_frame_len)) < 0) {
                fprintf(stderr, "err: read() -> code: %d\n", errno);
        } else {
                fprintf(stdout, "log: send frame no. %d of %ld bytes\n", sequence_number >> 6, wb);
                fprintf(stdout, "log: waiting on response from RECV for frame no. %d\n", sequence_number >> 6);
        }

        return wb;
}

void
trmt_alrm_handler_write(int unused) 
{
        alarm(TIMEOUT);
        ++retries;
        write_data();
}

static int 
check_resending(const uint8_t cmd)
{
        if (cmd == cmds[RR_1] || cmd == cmds[REJ_0])
                sequence_number = 0x40;
        else 
                sequence_number = 0x0;

        return !(cmd == cmds[RR_0] || cmd == cmds[RR_1]);
}

ssize_t
llwrite(int fd, uint8_t *buffer, ssize_t len)
{
        uint8_t *data = NULL;

        if ((len = encode_data(&data, buffer, len)) < 0) {
                fprintf(stderr, "err: encode_data() -> code: %ld\n", len);
                return len;
        }

        uint8_t frame[len+5];

        frame[0] = frame[len+4] = FLAG;
        frame[1] = TRANSMITTER;
        frame[2] = sequence_number;
        frame[3] = frame[1] ^ frame[2];
        memcpy(frame + 4, data, len);

        free(data);

        buffer_frame_len = sizeof(frame);
        memcpy(buffer_frame, frame, buffer_frame_len);

        retries = 0;
        install_sigalrm(trmt_alrm_handler_write);

        ssize_t wb;
        int rsnd;
        uint8_t mask = 1 << RR_0 | 1 << REJ_0 | 1 << RR_1 | 1 << REJ_1;

        do {
                if ((wb = write_data()) < 0)
                        return wb;

                alarm(TIMEOUT);
                rsnd = read_frame_US(fd, mask, RECEIVER);
                alarm(0);
        } while (connection_alive && rsnd == RESEND_REQUIRED);

        if (!connection_alive) {
                fprintf(stderr, "err: can't establish a connection with RECV\n");
                return -1;
        }

        return wb;
}


ssize_t
llread(int fd, uint8_t *buffer)
{
        readState st = START;
        uint8_t frame[2*MAX_PACKET_SIZE+5];
        uint8_t disc = 0;
        ssize_t c = 0;

        while (st != STOP) {
                if (read(fd, frame + st + c, 1) < 0) {
                        fprintf(stderr, "err: read() -> code: %d\n", errno);
                        return -1;
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
                        if (frame[st] == 0x0 || frame[st] == 0x40) {
                                sequence_number = !frame[st];
                                st = C_RCV;
                        } else if (frame[st] == cmds[DISC]) {
                                st = C_RCV;
                                disc = 1;
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

        if (disc) {
                fprintf(stdout, "log: disconnect frame detected\n");
                send_frame_US(fd, DISC, RECEIVER);
                return -1;
        }

        ssize_t len;
        len = decode_data(buffer, frame + 4, c);

        ssize_t i;
        uint8_t bcc = buffer[0], expect_bcc = buffer[len-1];
        for (i = 1; i < len - 1; i++)
                bcc ^= buffer[i];

        fprintf(stdout, "log: frame no. %d read with %ld bytes\n", sequence_number, len + 5);

        uint8_t cmd;
        cmd = sequence_number ? RR_1 : RR_0;
        if (bcc != expect_bcc)
                cmd = sequence_number ? REJ_1 : REJ_0;

        sleep(1);
        send_frame_US(fd, cmd, RECEIVER);
        return (bcc == expect_bcc) ? len : -1;
}

void 
trmt_alrm_handler_close(int unused) 
{
        alarm(TIMEOUT);
        retries++;
        send_frame_US(port_fd, DISC, TRANSMITTER);
}

int
llclose(int fd)
{
        if (connector == TRANSMITTER) {
                retries = 0;
                install_sigalrm(trmt_alrm_handler_close);
            
                send_frame_US(fd, DISC, TRANSMITTER);

                alarm(TIMEOUT);
                read_frame_US(fd, (1 << DISC), RECEIVER);
                alarm(0);
                
                if (!connection_alive) {
                        fprintf(stderr, "err: can't establish a connection with RECV\n");
                        return -1;
                }

                send_frame_US(fd, UA, TRANSMITTER);
        }

        sleep(2); /* Gives time to all the info flow througth the communication channel */
        return term_conf_end(fd);
}

