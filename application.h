/*
 * application.h
 * Serial port application protocol
 * RC @ L.EIC 2122
 * Authors: Miguel Rodrigues & Nuno Castro
 */

#ifndef _APPLICATION_H_

/* Control command for application packets */
typedef enum { DUMMY, DATA, START, STOP } ctrlCmd;
/* Parameter command for application packets */
typedef enum { SIZE, NAME } paramCmd;

#endif /* _APPLICATION_H_ */

