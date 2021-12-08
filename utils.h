/*
 * utils.h
 * Serial port protocol utilitary functions
 * RC @ L.EIC 2122
 * Authors: Miguel Rodrigues & Nuno Castro
 */

#ifndef _UTILS_H_

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***
 * Writes a message to stdout
 * @param const char *[in] - message to be written
 */
void plog(const char *format, ...);

/***
 * Writes a message to stderr
 * @param const char *[in] - message to be written
 */
void perr(const char *format, ...);

/***
 * Verifies whether a condition is valid or not, 
 * if it valid then nothing is done, otherwise it writes the message
 * passed as argument and finishes the program imediatly 
 * @param const int[in] - condition result
 * @param const char *[in] - message to be written
 * @param const int[in] - program's exit code
 */
void passert(const int cond, const char *msg, const int code);

#endif /* _UTILS_H_ */

