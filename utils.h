/*
 * utils.h
 * Serial port protocol utilitary functions
 * RC @ L.EIC 2122
 * Authors: Miguel Rodrigues & Nuno Castro
 */

#ifndef _UTILS_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void plog(const char *msg);
void perr(const char *msg);
void passert(const int cond, const char *msg, const int code);

#endif /* _UTILS_H_ */

