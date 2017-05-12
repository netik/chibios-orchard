#include "orchard-app.h"
#include "orchard-ui.h"
#include "radio_lld.h"
#include "rand.h"
#include "proto.h"

#include <stdlib.h>
#include <string.h>

#include "app-fight.h"

/*  
 * A naive implementation of the Stop and Wait protocol with timeout and ARQ and a short txring.
 */

void
protoInit (OrchardAppContext *context)
{
  ProtoHandles * p;
  
  p = (((FightHandles *)context->priv)->proto);
  
  p->state = PROTO_STATE_IDLE;
  p->rxseq = p->txseq = rand();
  p->interval = 500000;
  p->txhead = p->txtail = 0;
  return;
}

void
tickHandle (OrchardAppContext *context)
{
  ProtoHandles * p;
  PACKET * proto;
  
  p = (((FightHandles *)context->priv)->proto);
  
  p->interval = 500000 + (rand () & 0xFFFF);
  
  chprintf (stream, "tick... %d\r\n",
	    p->intervals_since_last_contact);
  orchardAppTimer (context, p->interval, FALSE);
  
  if (p->state != PROTO_STATE_IDLE) {
    p->intervals_since_last_contact++;
    if (p->intervals_since_last_contact > 10) {
      chprintf (stream, "request timed out!!\r\n");
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
    proto = (PACKET *)&p->txring[p->txtail];
    radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
               sizeof(PACKET), proto);
  }
  
  return;
}

int
msgSend (OrchardAppContext *context, void * payload)
{
  ProtoHandles * p;
  PACKET * proto;
  int res;

  res = 0;
  p = (((FightHandles *)context->priv)->proto);

  /* a packet is a raw collection of bytes, we do not care about the struct. */
  if (p->txhead == p->txtail) {
    /* we're at the head of the queue and can immediately send. */
    proto = (PACKET *)&p->txring[p->txtail]; // always send the tail. 
    
    p->state = PROTO_STATE_WAITACK;
    proto->prot_msg = PROTO_SYN;
    proto->prot_seq = p->txseq;
    
    memcpy((char *)proto->prot_payload, payload, p->mtu);
    
    chprintf(stream, "Send message to peer %x txseq %d\r\n", p->netid, p->txseq);
    res = radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
                     sizeof(PACKET), proto);
    p->txseq++;
    
    // we have a new pending packet, so increment the head.
    p->txhead++;
    if (p->txhead > PROTO_RING_SIZE) {
      p->txhead = 0;
    }
} else {
    /* enqueue the packet */
    if (p->txhead + 1 > PROTO_RING_SIZE) {
      chprintf(stream, "txring full\r\n");
      return(-1);
    } else {
      chprintf(stream, "packet queued\r\n");
      memcpy((char *)p->txring[p->txhead+1], payload, p->mtu);
      p->txhead++;
      if (p->txhead > PROTO_RING_SIZE) {
        p->txhead = 0;
      }
      return(0);
    }
  }
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
  p->txhead--;
  return;
}

int
rxHandle (OrchardAppContext *context, KW01_PKT * pkt)
{
  ProtoHandles * p;
  PACKET * proto;
  int sts = -1;
  int result;
  
  p = (((FightHandles *)context->priv)->proto);
  proto = (PACKET *)&pkt->kw01_payload;
  
  chprintf(stream,"in rxhandle, packet from %x expecting from %x protocol %x flags %d\r\n", pkt->kw01_hdr.kw01_src, p->netid, pkt->kw01_hdr.kw01_prot, proto->prot_msg);
  
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
    chprintf (stream, "out of txsequence message!!!\r\n");
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
    chprintf (stream, "message %d was acknowledged\r\n",
              proto->prot_seq);
    p->rxseq++;
    p->state = PROTO_STATE_CONNECTED;
    sts = 0;
    p->txtail++;
    if (p->txtail > PROTO_RING_SIZE) {
      p->txtail = 0;
    }

    if (p->txtail != p->txhead) {
      /* now that we've got an acknowledgement, if we have more work to
         do, send the next packet */
      p->state = PROTO_STATE_WAITACK;
      proto->prot_msg = PROTO_SYN;
      proto->prot_seq = p->txseq;
      proto = (PACKET *)&p->txring[p->txtail];
      radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
                 sizeof(PACKET), proto);

      p->txseq++;
    }      
    
    break;
    
  case PROTO_SYN:
    proto->prot_msg = PROTO_ACK;
    chprintf (stream, "message %d was received\r\n", proto->prot_seq);
    /* send ACK */
    result = radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
                        sizeof(PACKET), proto);
    
    chprintf(stream, "ack sent = %d\r\n", result);
    
    /* fire the recv callback with the full packet */
    if (p->cb_recv != NULL)
      p->cb_recv(pkt);
    break;
    
  case PROTO_RST:
    chprintf (stream, "peer disconnected\r\n", proto->prot_seq);
    proto->prot_msg = PROTO_ACK;
    radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_FIGHT,
               sizeof(PACKET), proto);
    p->state = PROTO_STATE_IDLE;
    p->intervals_since_last_contact = 0;
    p->txhead = p->txtail = 0;
    /* fire the timeout callback */
    if (p->cb_timeout != NULL)
      p->cb_timeout(pkt);
    break;
  default:
    chprintf(stream, "bogus packet\r\n");
    break;
  }
  
  proto->prot_msg = 0;
  
  return (sts);
}
