#include "radio_lld.h"

#ifndef _PROTO_H_
#define _PROTO_H_

#define PROTOCOL_TEST		99

/*
 * States
 */

#define PROTO_STATE_IDLE	0x01
#define PROTO_STATE_CONNECTED	0x02
#define PROTO_STATE_WAITACK	0x03

/*
 * Messages
 */

#define PROTO_SYN		0x01
#define PROTO_ACK		0x02
#define PROTO_RST		0x03

/*
 * We tell the compiler to make this structure packed so that it
 * doesn't try to silently insert any padding for alignment
 * purposes.
 */

#pragma pack(1)
typedef struct packet {
  uint32_t		prot_seq;
  uint8_t		prot_msg; // flags
  uint8_t		prot_payload[46]; // sizeof(user)
} PACKET;

#pragma pack()

#define PROTO_TICK_US           500000
#define PROTO_RING_SIZE         3

typedef struct _ringBuf
{
  int     first;
  int     last;
  int     validItems;
  PACKET  data[PROTO_RING_SIZE];
} ringBuffer;

typedef struct _ProtoHandles {
  uint32_t		netid;
  uint32_t		txseq;
  ringBuffer		txring;
  uint32_t		rxseq;
  uint8_t		state;
  uint8_t               mtu; /* packet size */
  uint32_t              last_tick_time;
  int			interval;
  int			intervals_since_last_contact;

  /* workspaces */
  PACKET                wpkt; /* workspace packet for funcs */
  
  /* callbacks */
  void                  (*cb_ack)(KW01_PKT *);
  void                  (*cb_recv)(KW01_PKT *);
  void                  (*cb_timeout)(KW01_PKT *);
} ProtoHandles;

extern void protoInit (OrchardAppContext * context);
extern int msgSend (OrchardAppContext * context, void *);
extern void rstSend (OrchardAppContext * context);
extern void tickHandle (OrchardAppContext * context);
extern int rxHandle (OrchardAppContext * context, KW01_PKT * pkt);
extern int msgReceived (OrchardAppContext * context);

#endif /* _PROTO_H_ */
