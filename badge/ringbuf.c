#include <stdlib.h>
#include <string.h>
#include "orchard-app.h"
#include "orchard-ui.h"
#include "radio_lld.h"
#include "rand.h"
#include "proto.h"
#include "ringbuf.h"

/* simplistic ring buffer implementation */

void ringInit(ringBuffer *theQueue) { 
  int i;

  theQueue->validItems  =  0;
  theQueue->first       =  0;
  theQueue->last        =  0;

  for(i=0; i<PROTO_RING_SIZE; i++) {
     memset(&theQueue->data[i], 0, sizeof(PACKET));
  }
  return;
}

int ringIsEmpty(ringBuffer *theQueue) { 
  if (theQueue->validItems == 0)
    return(1);
  else
    return(0);
}

int ringGetItem(ringBuffer *theQueue, uint8_t dequeue, PACKET *theItemValue) {
  /* setting dequeue to false will allow you to peek at the top of the ring */
  if (ringIsEmpty(theQueue)) {
    return(-1);
  } else { 
    memcpy(theItemValue,&theQueue->data[theQueue->first], sizeof(PACKET));

    if (dequeue) { 
      theQueue->first = (theQueue->first+1) % PROTO_RING_SIZE;
      theQueue->validItems--;
    }
    
    return(0);
  }
}

int ringPutItem(ringBuffer *theQueue, PACKET *theItem) { 
  if (theQueue->validItems>=PROTO_RING_SIZE) { 
    return(-1);
  } else { 
    theQueue->validItems++;
    memcpy(&theQueue->data[theQueue->last], theItem, sizeof(PACKET));
    theQueue->last = (theQueue->last+1)%PROTO_RING_SIZE;
  }

  return 0;
}
