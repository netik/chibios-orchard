#include "proto.h"

#ifndef _RINGBUF_H_
#define _RINGBUF_H_

/* adapted from:
 *  http://www.xappsoftware.com/wordpress/2012/09/27/a-simple-implementation-of-a-circular-queue-in-c-language/
 *
 */

void ringInit(ringBuffer *theQueue);
int ringIsEmpty(ringBuffer *theQueue);
int ringPutItem(ringBuffer *theQueue, PACKET *theItemValue);
int ringGetItem(ringBuffer *theQueue, uint8_t dequeue, PACKET *theItemValue);

#endif /* _RINGBUF_H_ */
