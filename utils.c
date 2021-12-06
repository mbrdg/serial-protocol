/*
 * utils.c
 * Serial port protocol utilitary functions
 * RC @ L.EIC 2122
 * Authors: Miguel Rodrigues & Nuno Castro
 */

#include "utils.h"

void 
plog(const char *msg)
{
        fprintf(stdout, "log: %s\n", msg);
}

void
perr(const char *msg)
{
        fprintf(stderr, "err: %s\n", msg);
}

void 
passert(const int cond, const char *msg, const int code)
{
        if (!cond) {
                fprintf(stderr, "err: %s :: %s\n", msg, strerror(errno));
                exit(code);
        }
}

