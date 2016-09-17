/*-
 * Copyright (c) 2016
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RADIO_H_
#define _RADIO_H_

#define KW01_DELAY 1000
#define KW01_PKT_MAXLEN 64
#define KW01_PKT_HDRLEN 3
#define KW01_PKT_HANDLERS_MAX 10

/*
 * Only the first two bytes of the packet header definition are defined
 * by the hardware. The length field is not actually transmitted but must
 * be written to the FIFO to indicate the packet size. The destination
 * field is used when address filtering is enabled. The radio also
 * appends a two byte CRC when hardware CRC calculation/checking is
 * enabled. Everything else is defined by the user.
 */

typedef struct kw01_pkt_hdr {
	uint8_t		kw01_length;	/* Total frame length */
	uint8_t		kw01_dst;	/* Destination node */
	uint8_t		kw01_src;	/* Source node */
	uint8_t		kw01_prot;	/* Protocol type */
} KW01_PKT_HDR;

typedef struct kw01_pkt {
	uint8_t		kw01_rssi;	/* Signal strength reading */
	KW01_PKT_HDR	kw01_hdr;
	uint8_t		kw01_payload[KW01_PKT_MAXLEN - KW01_PKT_HDRLEN];
} KW01_PKT;

typedef void (*KW01_PKT_FUNC)(KW01_PKT *);

typedef struct kw01_pkt_handler {
	KW01_PKT_FUNC	kw01_handler;
	uint8_t		kw01_prot;
} KW01_PKT_HANDLER;

typedef struct radio_driver {
	SPIDriver *	kw01_spi;
	KW01_PKT	kw01_pkt;
	KW01_PKT_HANDLER kw01_handlers[KW01_PKT_HANDLERS_MAX];
	KW01_PKT_HANDLER kw01_default_handler;
} RADIODriver;

extern RADIODriver KRADIO1;

extern void radioStart (SPIDriver *);
extern void radioStop (RADIODriver *);
extern void radioInterrupt (EXTDriver *, expchannel_t);
extern uint8_t radioRead (RADIODriver *, uint8_t);
extern void radioWrite (RADIODriver *, uint8_t, uint8_t);
extern void radioDump (RADIODriver *, uint8_t addr, void *bfr, int count);
extern void radioAcquire (RADIODriver *);
extern void radioRelease (RADIODriver *);
extern int radioSend(RADIODriver *, uint8_t dest, uint8_t prot,
			size_t len, const void *payload);

extern int radioModeSet (RADIODriver *, uint8_t mode);
extern int radioFrequencySet (RADIODriver *, uint32_t freq);
extern int radioDeviationSet (RADIODriver *, uint32_t freq);
extern int radioBitrateSet (RADIODriver *, uint32_t freq);
extern uint32_t radioFrequencyGet (RADIODriver *);
extern uint32_t radioDeviationGet (RADIODriver *);
extern uint32_t radioBitrateGet (RADIODriver *);
extern uint8_t radioAddressGet (RADIODriver *);
extern void radioAddressSet (RADIODriver *, uint8_t);

extern void radioDefaultHandlerSet (RADIODriver *, KW01_PKT_FUNC);
extern void radioHandlerSet (RADIODriver *, uint8_t, KW01_PKT_FUNC);

#endif /* _RADIO_H_ */
