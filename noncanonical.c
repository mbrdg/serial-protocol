/*Non-Canonical Input Processing*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

#define BAUDRATE B38400

volatile int STOP = 0;

int main(int argc, char **argv)
{
        if (argc < 2)
        {
                fprintf(stderr, "usage: %s <serial-port>\n", argv[0]);
                exit(1);
        }

        int fd, c, res;
        struct termios oldtio, newtio;
        char buf[255];

    /*
        Open serial port device for reading and writing and not as controlling tty
        because we don't want to get killed if linenoise sends CTRL-C.
    */  
    
        fd = open(argv[1], O_RDWR | O_NOCTTY);
        if (fd < 0) 
        {
                perror(argv[1]);
                exit(-1);
        }

        /* save current port settings */
        if (tcgetattr(fd,&oldtio) == -1) 
        { 
                perror("tcgetattr");
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

    /* 
        VTIME e VMIN devem ser alterados de forma a proteger com
        um temporizador a leitura do(s) próximo(s) caracter(es)
    */
        tcflush(fd, TCIOFLUSH);
        if (tcsetattr(fd,TCSANOW,&newtio) == -1) 
        {
                perror("tcsetattr");
                exit(-1);
        }

        printf("new termios structure set\n");

        c = 0;
        while (!STOP)
        {       
                res = read(fd, buf + c, sizeof(char));
                STOP = (buf[c] == '\0' || res == 0);
                c++;
        }

        printf("received: %s", buf);
        printf("%d bytes read\n", c);

        sleep(2);
        tcsetattr(fd, TCSANOW, &oldtio);
        
        close(fd);
        exit(0);
}
