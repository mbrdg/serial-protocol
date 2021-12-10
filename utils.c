/*
 * utils.c
 * Serial port protocol utilitary functions
 * RC @ L.EIC 2122
 * Authors: Miguel Rodrigues & Nuno Castro
 */

#include "utils.h"


void 
plog(const char *fmt, ...)
{
        fprintf(stdout, "log: ");
        va_list args;

        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        va_end(args);
}

void
perr(const char *fmt, ...)
{
        fprintf(stderr, "err: ");
        va_list args;

        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
}

void 
passert(const int cond, const char *msg, const int code)
{
        if (!cond) {
                fprintf(stderr, "die: %s :: %s\n", msg, strerror(errno));
                exit(code);
        }
}


const clock_t
bclk(void) 
{
        plog("clock: began\n");
        const clock_t start = clock();
        return start;
}

void 
eclk(const clock_t *start)
{
        clock_t end = clock();
        double elapsed = (double)(end - *start) * 1000.0 / CLOCKS_PER_SEC;
        plog("clock: ended\n");
        plog("clock: took %.5f ms\n", elapsed);
}

