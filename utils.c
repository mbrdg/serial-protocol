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
        va_list args;

        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        va_end(args);
}

void
perr(const char *fmt, ...)
{
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

