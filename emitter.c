/*
 * emitter.c
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
#define MAXLEN 255

volatile int STOP = 0;

int
main (int argc, char **argv)
{
        if (argc < 2)
        {
                fprintf(stderr, "usage: %s <serialport>\n", argv[0]);
                exit(1);
        }

        struct termios oldtio, newtio;
        char buf[MAXLEN];

        /*  Open serial port device for reading and writing and not as controlling
            tty because we don't want to get killed if linenoise sends CTRL-C. */
        int fd;
        fd = open(argv[1], O_RDWR | O_NOCTTY);
        if (fd < 0) 
        {
                fprintf(stderr, "error: open() code: %d\n", errno);
                exit(-1); 
        }

        /* save current port settings */
        if (tcgetattr(fd, &oldtio) == -1) 
        { 
                fprintf(stderr, "error: tcgetattr() code: %d\n", errno);
                exit(-1);
        }

        memset(&newtio, '\0', sizeof(newtio));
        newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;

        /* set input mode (non-canonical, no echo,...) */
        newtio.c_lflag = 0;

        newtio.c_cc[VTIME] = 0;  /* inter-character timer unused */
        newtio.c_cc[VMIN] = 5;   /* blocking read until 5 chars received */

        /* VTIME e VMIN devem ser alterados de forma a proteger com um 
           temporizador a leitura do(s) proximo(s) caracter(es) */
        tcflush(fd, TCIOFLUSH);
        if (tcsetattr(fd, TCSANOW, &newtio) == -1)
        {
                fprintf(stderr, "error: tcsetattr() code: %d\n", errno);
                exit(-1);
        }

        fprintf(stdout, "info: new termios structure set\n");
        
        fgets(buf, MAXLEN, stdin);
        const char *nl = strchr(buf, '\n');
        int c = nl ? nl - buf : MAXLEN - 2;
        buf[c+1] = '\0';
    
        int res;
        res = write(fd, buf, strlen(buf) + 1);
        fprintf(stdout, "info: %d bytes written\n", res);
        
        c = 0;
        memset(buf, '\0', MAXLEN);
        while (!STOP)
        {
                res = read(fd, buf + c, sizeof(char));
                STOP = (buf[c] == '\0' || !res || c == (MAXLEN - 1));
                c++;
        }

        fprintf(stdout, "%s", buf);
        fprintf(stdout, "info: %d bytes read\n", c);
    
        /* revert to the old port settings */
        sleep(2);
        if (tcsetattr(fd, TCSANOW, &oldtio) == -1) 
        {
                fprintf(stderr, "error: tcsetattr() code: %d\n", errno);
                exit(-1);
        }

        close(fd);
        exit(0);
}

