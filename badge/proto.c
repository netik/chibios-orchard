#include "orchard-app.h"
#include "orchard-ui.h"
#include "radio_lld.h"
#include "rand.h"
#include "proto.h"

#include <stdlib.h>
#include <string.h>

#include "app-fight.h"
#include "ringbuf.h"

/*  
 * A naive implementation of the Stop and Wait protocol with timeout,
 * ARQ, and a txqueue.
 */

void
protoInit (OrchardAppContext *context)
{
  ProtoHandles * p;
  
  p = (((FightHandles *)context->priv)->proto);

  p->state = PROTO_STATE_IDLE;
  p->rxseq = p->txseq = rand();
  p->interval = PROTO_TICK_US + (rand () & 0xFFFF);

  p->cb_ack = p->cb_recv = p->cb_timeout = NULL;

  ringInit(&p->txring);
  
  return;
}

void
tickHandle (OrchardAppContext *context)
{
  ProtoHandles * p;
  p = (((FightHandles *)context->priv)->proto);

  // reset the interval

  if (p->interval > 0) {
    // no work to do yet
    // regardless of frame rate, we will work this out.
    p->interval = p->interval - ST2US(chVTGetSystemTime() - p->last_tick_time);
    p->last_tick_time = chVTGetSystemTime();
    return;
  }
  
  p->interval = PROTO_TICK_US + (rand () & 0xFFFF);
  
  if (p->state != PROTO_STATE_IDLE) {
    p->intervals_since_last_contact++;
    if (p->intervals_since_last_contact > 10) {
#ifdef DEBUG_FIGHT_NETWORK
      chprintf (stream, "request timed out!!\r\n");
#endif
      p->intervals_since_last_contact = 0;
      p->state = PROTO_STATE_IDLE;
      p->rxseq = p->txseq = rand();
    }
  }
  
  /*
   * Keep retransmitting until our packet is
   * acknowleged or we time out.
   */
  if (p->state == PROTO_STATE_WAITACK) {
    ringGetItem(&p->txring, false, &p->wpkt);
#ifdef DEBUG_FIGHT_NETWORK
    chprintf (stream, "resend packet %d\r\n", p->wpkt.prot_seq);
#endif
    radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
               sizeof(PACKET), &p->wpkt);
  }
  
  return;
}

int
msgSend (OrchardAppContext *context, void * payload)
{
  ProtoHandles * p;
  int8_t res;

  res = 0;
  p = (((FightHandles *)context->priv)->proto);

  p->state = PROTO_STATE_WAITACK;
  p->wpkt.prot_msg = PROTO_SYN;
  p->wpkt.prot_seq = p->txseq;

  memcpy(&p->wpkt.prot_payload, payload, p->mtu);
  
  if (ringIsEmpty(&p->txring)) { 
    /* we're up to date and can immediately send. */
    res = radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
                     sizeof(PACKET), &p->wpkt);
#ifdef DEBUG_FIGHT_NETWORK
    chprintf(stream, "Send message to peer %x txseq %d\r\n", p->netid, p->txseq);
#endif
    p->txseq++; 
#ifdef DEBUG_FIGHT_NETWORK
  } else {
    chprintf(stream, "packet queued (still busy)\r\n");
  }
#else
  }
#endif
  
  // always inject to queue.
  ringPutItem(&p->txring, &p->wpkt);    

  return res;
}

int
msgReceived (OrchardAppContext *context)
{
  ProtoHandles * p;
  
  p = (((FightHandles *)context->priv)->proto);
  if (p->rxseq == p->txseq)
    return (0);
  
  return (-1);
}

#ifdef notdef
void
rstSend (OrchardAppContext *context)
{
  ProtoHandles * p;
  PACKET * proto;
  
  p = (((FightHandles *)context->priv)->proto);
  
  proto = (PACKET *)&p->txring[0];
  
  p->state = PROTO_STATE_WAITACK;
  
  proto->prot_msg = PROTO_RST;
  proto->prot_seq = p->txseq;
  
  radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
             sizeof(PACKET), proto);
  
  p->txseq++;
  return;
}
#endif

int
rxHandle (OrchardAppContext *context, KW01_PKT * pkt)
{
  ProtoHandles * p;
  PACKET * proto;  // inbound
  int sts = -1;

#ifdef DEBUG_FIGHT_NETWORK
  int result;
#endif
  
  p = (((FightHandles *)context->priv)->proto);
  proto = (PACKET *)&pkt->kw01_payload;
#ifdef DEBUG_FIGHT_NETWORK  
  chprintf(stream,"in rxhandle, packet from %x expecting from %x protocol %x flags %d\r\n", pkt->kw01_hdr.kw01_src, p->netid, pkt->kw01_hdr.kw01_prot, proto->prot_msg);
#endif
  
  /*
   * Note: we may end getting here either because we received
   * a message specifically for our protocol, or because
   * we received a ping. Either way, we know our peer is
   * still in range, so we can keep the connection alive.
   */
  
  p->intervals_since_last_contact = 0;
  
  /* If this is for our protocol, then process the packet. */
  if (pkt->kw01_hdr.kw01_prot != RADIO_PROTOCOL_FIGHT)
    return (sts);
  
  if (pkt->kw01_hdr.kw01_src != p->netid)
    return (sts);
  
  /*
   * Sequence number must match, or else this is
   * a bogus packet.
   */
  if (proto->prot_seq == p->txseq) {
#ifdef DEBUG_FIGHT_NETWORK
    chprintf (stream, "out of txsequence message!!!\r\n");
#endif
    p->state = PROTO_STATE_IDLE;
    p->intervals_since_last_contact = 0;

    /* TODO: even if we have an out of sequence message we must ACK them,
     * because if we do not ACK they will resend
     */

    /* TODO: how about handling delayed ACK -- must have a number of the
     * acknowledgement? Ack numbers must verified... 
     */
    return (-1);
  }
  
  switch (proto->prot_msg) {
  case PROTO_ACK:
#ifdef DEBUG_FIGHT_NETWORK
    chprintf (stream, "message %d was acknowledged\r\n",
              proto->prot_seq);
#endif
    p->rxseq++;
    p->state = PROTO_STATE_CONNECTED;
    sts = 0;

    // we're gonna reuse the proto memory because reasons
    ringGetItem(&p->txring, true, proto);

    // handle the ack callback
    if (p->cb_ack != NULL)
      p->cb_ack(pkt);

    if (! ringIsEmpty(&p->txring) ){
      ringGetItem(&p->txring, false, proto); // no dequeue, we're not done.
      p->state = PROTO_STATE_WAITACK;
      proto->prot_msg = PROTO_SYN;
      proto->prot_seq = p->txseq;
      
      radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
                 sizeof(PACKET), proto);

      p->txseq++;
    }      
    
    break;
    
  case PROTO_SYN:
    proto->prot_msg = PROTO_ACK;
#ifdef DEBUG_FIGHT_NETWORK
    chprintf (stream, "message %d was received\r\n", proto->prot_seq);
#endif
#ifdef DEBUG_FIGHT_NETWORK
    /* send ACK */
    result = radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
                        sizeof(PACKET), proto);

    chprintf(stream, "ack sent = %d\r\n", result);
#else
    /* send ACK */
    radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
                        sizeof(PACKET), proto);
#endif
    
    /* fire the recv callback with the full packet */
    if (p->cb_recv != NULL)
      p->cb_recv(pkt);
    break;
    
  case PROTO_RST:
#ifdef DEBUG_FIGHT_NETWORK
    chprintf (stream, "peer disconnected\r\n", proto->prot_seq);
#endif
    proto->prot_msg = PROTO_ACK;
    radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
               sizeof(PACKET), proto);
    p->state = PROTO_STATE_IDLE;
    p->intervals_since_last_contact = 0;

    /* fire the timeout callback */
    if (p->cb_timeout != NULL)
      p->cb_timeout(pkt);
    break;
  default:
#ifdef DEBUG_FIGHT_NETWORK
    chprintf(stream, "bogus packet\r\n");
#endif
    break;
  }
  
  proto->prot_msg = 0;
  
  return (sts);
}
