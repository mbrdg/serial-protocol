/*
 * protocol.c
 * Serial port protocol
 * RC @ L.EIC 2122
 * Authors: Miguel Rodrigues & Nuno Castro
 */

#include "protocol.h"

/* macros */
#define FLAG 0x7E
#define ESCAPE 0x7D
#define KEY 0x20

#define RESEND 1

#define IS_ESCAPE(c) (c == ESCAPE)
#define IS_FLAG(c) (c == FLAG)
#define ESCAPED_BYTE(c) (IS_ESCAPE(c) || IS_FLAG(c))

#define BIT_SET(m, i) (m & (1 << i))

/* commands */ 
typedef enum { SET, DISC, UA, RR_0, REJ_0, RR_1, REJ_1 } frameCmd;
static const uint8_t cmds[7] = { 0x3, 0xB, 0x7, 0x5, 0x1, 0x85, 0x81 };

#ifdef DEBUG
static const char cmds_str[7][6] = { "SET", "DISC", "UA", "RR_0", "REJ_0", "RR_1", "REJ_1" };
#endif

/* reading */
typedef enum { START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, DATA, STOP } readState;

/* global variables */
static struct termios oldtio, newtio;
static struct sigaction sigact;

static int port_fd;

static uint8_t connector;
static volatile uint8_t retries, sequence_number = 0;
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
        char fname[12];
        snprintf(fname, 12, "/dev/ttyS%d", port);

        port_fd = open(fname, O_RDWR | O_NOCTTY);
        if (port_fd < 0)
                return -1;

        if (tcgetattr(port_fd, &oldtio) < 0)
                return -1;

        memset(&newtio, '\0', sizeof(newtio));

        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;
        newtio.c_lflag = 0; /* set input mode (non-canonical, no echo...) */

        newtio.c_cc[VTIME] = 0; 
        newtio.c_cc[VMIN] = 1; /* 1 char required to satisfy a read */

        tcflush(port_fd, TCIOFLUSH);
        if (tcsetattr(port_fd, TCSANOW, &newtio) == -1)
                return -1;

#ifdef DEBUG
        plog("termios struct set with sucess");
#endif

        return port_fd;
}

static int
term_conf_end(int fd)
{
        if (tcsetattr(fd, TCSANOW, &oldtio) < 0)
                return -1;

        close(fd);
        return 0;
}


static int
send_frame_us(int fd, uint8_t cmd, uint8_t addr) 
{        
        unsigned char frame[5];

        frame[0] = frame[4] = FLAG;
        frame[1] = addr;
        frame[2] = cmds[cmd];
        frame[3] = frame[1] ^ frame[2];

        if (write(fd, frame, sizeof(frame)) < 0)
                return -1;

#ifdef DEBUG
        char fsent[40];
        if (addr == TRANSMITTER)
                snprintf(fsent, 40, "frame sent with %s @ TRANSMITTER", cmds_str[cmd]);
        else if (addr == RECEIVER)
                snprintf(fsent, 40, "frame sent with %s @ RECEIVER", cmds_str[cmd]);

        plog(fsent);
#endif
        return 0;
}

static int 
read_frame_us(int fd, const uint8_t cmd_mask, const uint8_t addr)
{
        readState st = START;
        uint8_t frame[5];
        ssize_t i, cmd, rb;

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
                        for (i = 0; i < 7; i++) {
                                if (BIT_SET(cmd_mask, i) && frame[st] == cmds[i]) {
                                        st = C_RCV;
                                        cmd = i;
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

        connection_alive = retries < MAX_RETRIES;
        if (!connection_alive)
                return -1;

#ifdef DEBUG
        char fread[40];
        if (addr == RECEIVER)
                snprintf(fread, 40, "frame read with %s @ TRANSMITTER", cmds_str[cmd]);
        else if (addr == TRANSMITTER)
                snprintf(fread, 40, "frame read with %s @ RECEIVER", cmds_str[cmd]);

        plog(fread);
#endif

        uint8_t frame_i_ans = 1 << RR_0 | 1 << REJ_0 | 1 << RR_1 | 1 << REJ_1;
        if (connector == TRANSMITTER && cmd_mask == frame_i_ans)
                return check_resending(frame[2]);

        return 0;
}


void 
trmt_alrm_handler_open(int unused) 
{
        alarm(TIMEOUT);
        retries++;
        send_frame_us(port_fd, SET, TRANSMITTER);
}

static int 
llopen_recv(int fd)
{
        read_frame_us(fd, 1 << SET, TRANSMITTER);
        send_frame_us(fd, UA, RECEIVER);
        
        return 0;
}

static int 
llopen_trmt(int fd)
{
        int conn_est;

        retries = 0;
        install_sigalrm(trmt_alrm_handler_open);

        send_frame_us(fd, SET, TRANSMITTER);
        alarm(TIMEOUT);
        conn_est = read_frame_us(fd, 1 << UA, RECEIVER);
        alarm(0);

        if (!connection_alive) {
                perr("can't establish a connection with the RECEIVER");
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
encode_cpy(uint8_t *dest, ssize_t off, uint8_t c) 
{
        dest[off] = c;
    
        if (ESCAPED_BYTE(c)) {
                dest[off] = ESCAPE;
                dest[off+1] = c ^ KEY;
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
        passert(dest != NULL, "protocol.c:287, malloc", -1);

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
        wb = write(port_fd, buffer_frame, buffer_frame_len);

#ifdef DEBUG
        char finfo[50];

        snprintf(finfo, 50, "send frame no. %d of %ld bytes", sequence_number, wb);
        plog(finfo);
        snprintf(finfo, 50, "waiting on response from RECEIVER for frame no. %d", sequence_number);
        plog(finfo);
#endif

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
        sequence_number = (cmd == cmds[RR_1] || cmd == cmds[REJ_0]);
        return (cmd == cmds[REJ_0] || cmd == cmds[REJ_1]);
}

ssize_t
llwrite(int fd, uint8_t *buffer, ssize_t len)
{
        uint8_t *data = NULL;

        len = encode_data(&data, buffer, len);
        if (len < 0)
                return len;

        uint8_t frame[len+5];

        frame[0] = frame[len+4] = FLAG;
        frame[1] = TRANSMITTER;
        frame[2] = sequence_number << 6;
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
                wb = write_data();
                if (wb < 0)
                        return wb;

                alarm(TIMEOUT);
                rsnd = read_frame_us(fd, mask, RECEIVER);
                alarm(0);
        } while (connection_alive && rsnd == RESEND);

        if (!connection_alive) {
                perr("can't establish a connection with RECEIVER");
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
                if (read(fd, frame + st + c, 1) < 0)
                        return -1;

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
#ifdef DEBUG
                plog("disconnect frame detected");
#endif
                send_frame_us(fd, DISC, RECEIVER);
                return -1;
        }

#ifdef DEBUG
        char fread[40];
        snprintf(fread, 40, "frame no. %d read with %ld bytes", sequence_number, c + 5);
        plog(fread);
#endif

        ssize_t len;
        len = decode_data(buffer, frame + 4, c);

        ssize_t i;
        uint8_t bcc = buffer[0], expect_bcc = buffer[len-1];
        for (i = 1; i < len - 1; i++)
                bcc ^= buffer[i];

        uint8_t cmd;
        cmd = sequence_number ? RR_1 : RR_0;
        if (bcc != expect_bcc)
                cmd = sequence_number ? REJ_1 : REJ_0;

        send_frame_us(fd, cmd, RECEIVER);
        return (bcc == expect_bcc) ? len : -1;
}

void 
trmt_alrm_handler_close(int unused) 
{
        alarm(TIMEOUT);
        retries++;
        send_frame_us(port_fd, DISC, TRANSMITTER);
}

int
llclose(int fd)
{
        if (connector == TRANSMITTER) {
                retries = 0;
                install_sigalrm(trmt_alrm_handler_close);
            
                send_frame_us(fd, DISC, TRANSMITTER);

                alarm(TIMEOUT);
                read_frame_us(fd, 1 << DISC, RECEIVER);
                alarm(0);
                
                if (!connection_alive) {
                        perr("can't establish a connection with RECEIVER");
                        return -1;
                }

                send_frame_us(fd, UA, TRANSMITTER);
        }

        sleep(2); /* Gives time to all the info flow througth the communication channel */
        return term_conf_end(fd);
}

