/*
 * utils.h
 * Serial port protocol utilitary functions
 * RC @ L.EIC 2122
 * Authors: Miguel Rodrigues & Nuno Castro
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

/***
 * Begins a clock
 * @param const clock_t[out] - clock's current timestamp
 */
const clock_t bclk(void);

/***
 * Finishes a clock 
 * @param const clock_t *[in] - clock's begin timestamp
 */
void eclk(const clock_t *start);

#endif /* _UTILS_H_ */

