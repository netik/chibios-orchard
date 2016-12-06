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

#ifdef KW01_RADIO_HWFILTER
#define KW01_PKT_MAXLEN 66	/* Max packet size (FIFO size) */
#define KW01_PKT_HDRLEN 3
#define KW01_PKT_AES_MAXLEN 48	/* Max packet size with AES and filtering on */
#else
#define KW01_PKT_MAXLEN 64	/* Max packet size (FIFO size) */
#define KW01_PKT_HDRLEN 9
#define KW01_PKT_AES_MAXLEN 64	/* Max packet size with AES and filtering off */
#endif
#define KW01_PKT_HANDLERS_MAX 4

#ifdef KW01_RADIO_HWFILTER
#define RADIO_BROADCAST_ADDRESS 0xFF
#else
#define RADIO_BROADCAST_ADDRESS 0xFFFFFFFF
#endif

/* Radio message types */
#define RADIO_PROTOCOL_CHAT	1	/* Send message to 1 badge */
#define RADIO_PROTOCOL_SHOUT	2	/* Broadcast message to all badges */
#define RADIO_PROTOCOL_PING	3	/* Solicit ping ID from badges */
#define RADIO_PROTOCOL_BATTLE	4	/* Fight! */

/*
 * Only the first byte of the packet header definition is defined
 * by the hardware. The length field is not actually transmitted but must
 * be written to the FIFO to indicate the packet size. The destination
 * field is used when address filtering is enabled. The radio also
 * appends a two byte CRC when hardware CRC calculation/checking is
 * enabled. Everything else is defined by the user.
 */

#pragma pack(1)
typedef struct kw01_pkt_hdr {
#ifdef KW01_RADIO_HWFILTER
	uint8_t		kw01_dst;	/* Destination node */
	uint8_t		kw01_src;	/* Source node */
#else
	uint32_t	kw01_dst;	/* Destination node */
	uint32_t	kw01_src;	/* Source node */
#endif
	uint8_t		kw01_prot;	/* Protocol type */
} KW01_PKT_HDR;
#pragma pack()

#pragma pack(1)
typedef struct kw01_pkt {
	uint8_t		kw01_rssi;	/* Signal strength reading */
	uint8_t		kw01_length;	/* Total frame length */
	KW01_PKT_HDR	kw01_hdr;
	uint8_t		kw01_payload[KW01_PKT_MAXLEN - KW01_PKT_HDRLEN];
} KW01_PKT;
#pragma pack()

typedef void (*KW01_PKT_FUNC)(KW01_PKT *);

typedef struct kw01_pkt_handler {
	KW01_PKT_FUNC	kw01_handler;
	uint8_t		kw01_prot;
} KW01_PKT_HANDLER;

#define KW01_FLAG_AES		0x01	/* AES enabled */

typedef struct radio_driver {
	SPIDriver *	kw01_spi;
	KW01_PKT	kw01_pkt;
	uint8_t		kw01_flags;
	uint8_t		kw01_maxlen;
	mutex_t		kw01_mutex;
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
#ifdef KW01_RADIO_HWFILTER
extern int radioSend(RADIODriver *, uint8_t dest, uint8_t prot,
			uint8_t len, const void *payload);
#else
extern int radioSend(RADIODriver *, uint32_t dest, uint8_t prot,
			uint8_t len, const void *payload);
#endif

extern int radioFrequencySet (RADIODriver *, uint32_t freq);
extern int radioDeviationSet (RADIODriver *, uint32_t freq);
extern int radioBitrateSet (RADIODriver *, uint32_t freq);
extern uint32_t radioFrequencyGet (RADIODriver *);
extern uint32_t radioDeviationGet (RADIODriver *);
extern uint32_t radioBitrateGet (RADIODriver *);
#ifdef KW01_RADIO_HWFILTER
extern uint8_t radioAddressGet (RADIODriver *);
extern void radioAddressSet (RADIODriver *, uint8_t);
#endif /* KW01_RADIO_HWFILTER */
extern int radioNetworkSet (RADIODriver *, const uint8_t *, uint8_t);
extern int radioNetworkGet (RADIODriver *, uint8_t *, uint8_t *);
extern int radioTemperatureGet (RADIODriver *);
extern int radioAesEnable (RADIODriver *, const uint8_t *, uint8_t);
extern void radioAesDisable (RADIODriver *);
extern void radioDefaultHandlerSet (RADIODriver *, KW01_PKT_FUNC);
extern void radioHandlerSet (RADIODriver *, uint8_t, KW01_PKT_FUNC);

#endif /* _RADIO_H_ */
