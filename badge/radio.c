/*
 * This module implements support for the Semtech SX123x IMS radio module.
 * The SX123x chips are available as standalone devices, are may also be
 * embedded as the on-board radio device in an SoC, such as the
 * Freescale/NXP KW01 processor.
 *
 * The SX123x series are Industrial Scientific and Medical (ISM) radio modems
 * that cover the 433MHz, 868MHz and 915MHZ bands, supporting FSK, GMSK, MSK
 * and OOK modulation. They also include built-in AES-128 encryption support
 * as well as a battery monitor and temperature sensor. The packet handler
 * also includes a 66-byte FIFO.
 *
 * The radio module is programmed using a SPI interface. A set of digital
 * I/O pins are provided for signalling interrupt conditions and a CLKOUT
 * pin to which an be optionally configured to provide an image of the
 * radio's reference clock. There is also a hard reset pin.
 *
 * The radio module radio module supports several tunable parameters,
 * including center carrier frequency, deviation, bitrate, and RX filter
 * bandwidth. Packet filtering can be done using a bit synchronizer that
 * can be programmed to recognize up to 8 bytes of synronization data.
 * (This effectively forms a radio network.) Additional filtering can be
 * performed using a node ID byte, which allows for 254 unique unicast
 * addresses and one broadcast address.
 *
 * On the Freescale/NXP KW01 chip, the radio module's SPI pins are wired
 * internally to the KW01's SPI0 interface. The DIO, reset and CLKOUT pins
 * must be wired up by the board designer. On the KW019032 reference board,
 * the DIO0 pin is wired to PORTC pin 3, the DIO1 pin is wired to PORTC
 * pin 4 and the reset pin is wired to PORTE pin 30.
 *
 * SEE ALSO:
 * http://www.nxp.com/files/microcontrollers/doc/ref_manual/MKW01xxRM.pdf
 * http://www.semtech.com/images/datasheet/sx1232.pdf
 * http://www.semtech.com/images/datasheet/sx1233.pdf
 */

#include <string.h>

#include "ch.h"
#include "hal.h"
#include "spi.h"
#include "pal.h"
#include "chprintf.h"

#include "radio_reg.h"
#include "radio.h"

#include "orchard.h"
#include "orchard-events.h"

/*
 * Statically allocate memory for a single packet.
 * We can only dispatch one frame at a time.
 */

static KW01_PKT pkt;
static KW01_PKT_HANDLER handlers[KW01_PKT_HANDLERS_MAX];
static KW01_PKT_HANDLER default_handler;

static int radioDispatch (void);
static void radioIntrHandle (eventid_t);
static void radio_select (SPIDriver *);
static void radio_unselect (SPIDriver *);

/******************************************************************************
*
* radioInterrupt - SX123x interrupt service routine
*
* This function is triggered when the radio module asserts its interrupt pin.
* On the Freescale Freedom KW019032 board, the DIO0 pin is tied to I/O
* PORT C pin 4.
*
* RETURNS: N/A
*/

void
radioInterrupt (EXTDriver *extp, expchannel_t channel)
{
	(void)extp;
	(void)channel;

	chSysLockFromISR();
	chEvtBroadcastI(&rf_pkt_rdy);
	chSysUnlockFromISR();

	return;
}

static int
radioDispatch ()
{
	uint8_t * p;
	KW01_PKT_HANDLER * ph;
	uint8_t reg;
	uint8_t len;
	uint8_t i;

	/* Get the signal strength reading for this frame. */

	pkt.kw01_rssi = radioRead (KW01_RSSIVAL);

	/* Start reading from the FIFO. */

	radio_select (&SPID1);
	reg = KW01_FIFO;
	spiSend (&SPID1, 1, &reg);

	/* The frame length is the first byte in the FIFO. */

	spiReceive (&SPID1, 1, &len);

	if (len > KW01_PKT_MAXLEN) {
		radio_unselect (&SPID1);
		radioWrite (KW01_PKTCONF2, KW01_PKTCONF2_RESTARTRX);
		return (-1);
	}

	/*
	 * The first byte in the pkt structure is the RSSI value
	 * and the second is the length, both of which we will
	 * fill in manually, so start filling the packet buffer
	 * from the second byte in.
	 */

	p = (uint8_t *)&pkt;
	p += 2;

	/* Read the rest of the frame. */

	spiReceive (&SPID1, len - 1, p);
	radio_unselect (&SPID1);

	/* Set the payload length (don't include the header length) */

	pkt.kw01_hdr.kw01_length = len - sizeof (KW01_PKT_HDR);

	/* Restart the receiver */

	radioWrite (KW01_PKTCONF2, KW01_PKTCONF2_RESTARTRX);

	for (i = 0; i < KW01_PKT_HANDLERS_MAX; i++) {
		ph = &handlers[i];
		if (ph->kw01_handler != NULL &&
		    ph->kw01_prot == pkt.kw01_hdr.kw01_prot) {
			ph->kw01_handler (&pkt);
			break;
		}
	}

	if (i == KW01_PKT_HANDLERS_MAX)
		default_handler.kw01_handler (&pkt);

	return (0);
}

static void
radioIntrHandle (eventid_t id)
{
	uint8_t irq1;
	uint8_t irq2;

	(void)id;

	irq1 = radioRead (KW01_IRQ1);
	irq2 = radioRead (KW01_IRQ2);

	/* We received a packet -- read it and dispatch it. */
	if (irq2 & KW01_IRQ2_PAYLOADREADY)
		radioDispatch ();

	/* Acknowledge interrupts */

	radioWrite (KW01_IRQ1, irq1);
	radioWrite (KW01_IRQ2, irq2);

	return;
}

/******************************************************************************
*
* radioReset - force a reset of the SX1232 chip
*
* The SX1232 module has a reset pin which can be asserted to force a complete
* reset of the radio. This reloads the power-up default register contents and
* puts the radio into standby mode.
*
* On the Freescale Freedom KW019032 board, the reset pin is wired to I/O
* port E pin 30. This function asserts that pin for 1 millisecond and then
* then waits for 10 ms afterwards for the chip reset to complete.
*
* RETURNS: N/A
*/

static void
radioReset (void)
{
        /* Assert the reset pin for 1 millisecond */

        palSetPad (GPIOE, 30);
        chThdSleepMilliseconds(1);
        palClearPad (GPIOE, 30);

 	/* Now wait for the chip to get its brains in order. */

        chThdSleepMilliseconds(100);
	
	return;
}

int
radioModeSet (uint8_t mode)
{
	unsigned int i;

	if (mode != KW01_MODE_STANDBY &&
	    mode != KW01_MODE_SLEEP &&
	    mode != KW01_MODE_RX &&
	    mode != KW01_MODE_TX)
		return (-1);

	radioWrite (KW01_OPMODE, mode);

	for (i = 0; i < KW01_DELAY; i++) {
		if (radioRead (KW01_IRQ1) & KW01_IRQ1_MODEREADY)
			break;
	}

	if (i == KW01_DELAY)
		return (-1);

	return (0);
}

int
radioFrequencySet (uint32_t freq)
{
	double val;
	uint32_t regval;

	if (freq < 430000000 || freq > 920000000)
		return (-1);

	val = KW01_FREQ(freq);
	regval = (uint32_t)val;

	radioWrite (KW01_FRFMSB, regval >> 16);
	radioWrite (KW01_FRFISB, regval >> 8);
	radioWrite (KW01_FRFLSB, regval & 0xFF);

	return (0);
}

uint32_t
radioFrequencyGet(void)
{
	double val;
	uint32_t regval;

	regval = radioRead (KW01_FRFLSB);
	regval |= radioRead (KW01_FRFISB) << 8;
	regval |= radioRead (KW01_FRFMSB) << 16;

	val = (double)regval * KW01_FSTEP;
	regval = (uint32_t)val;

	return (regval);
}

int
radioDeviationSet (uint32_t deviation)
{
	double val;
	uint32_t regval;

	if (deviation < 600 || deviation > 200000)
		return (-1);

	val = KW01_FREQ(deviation);
	regval = (uint32_t)val;

	radioWrite (KW01_FDEVMSB, regval >> 8);
	radioWrite (KW01_FDEVLSB, regval & 0xFF);

	return (0);
}

uint32_t
radioDeviationGet(void)
{
	double val;
	uint32_t regval;

	regval = radioRead (KW01_FDEVLSB);
	regval |= radioRead (KW01_FDEVMSB) << 8;

	val = (double)regval * KW01_FSTEP;
	regval = (uint32_t)val;

	return (regval);
}

int
radioBitrateSet (uint32_t bitrate)
{
	double val;
	uint32_t regval;

	if (bitrate < 1200 || bitrate > 200000)
		return (-1);

	val = KW01_BITRATE(bitrate);
	regval = (uint32_t)val;

	radioWrite (KW01_BITRATEMSB, regval >> 8);
	radioWrite (KW01_BITRATELSB, regval & 0xFF);

	return (0);
}

uint32_t
radioBitrateGet(void)
{
	double val;
	uint32_t regval;

	regval = radioRead (KW01_BITRATELSB);
	regval |= radioRead (KW01_BITRATEMSB) << 8;

	val = (double)KW01_XTAL_FREQ / (double)regval;
	regval = (uint32_t)val;

	return (regval);
}

void
radioStart (SPIDriver * sp)
{
	uint8_t version;

	(void)sp;

	/* Reset radio hardware to known state. */

	radioReset ();

	/* Put radio in standby mode */

	radioModeSet (KW01_MODE_STANDBY);

	/* Select variable length packet mode */

	radioWrite (KW01_DATMOD, KW01_DATAMODE_PACKET|
		    KW01_MODULATION_FSK|KW01_MODSHAPE_GFBT03);

	/*
	 * Enable whitening and CRC insertion/checking,
	 * select unicast/broadcast address filtering mode.
	 */

	radioWrite (KW01_PKTCONF1, KW01_FORMAT_VARIABLE|KW01_DCFREE_WHITENING|
			KW01_PKTCONF1_CRCON|KW01_AFILT_UCASTBCAST);

	radioWrite (KW01_PKTCONF2, 0);
	radioWrite (KW01_PAYLEN, 0xFF);

	radioWrite (KW01_PREAMBLEMSB, 0);
	radioWrite (KW01_PREAMBLELSB, 3);

	/* Select frequency, deviation and bitrate */

	radioFrequencySet (902500000);
	radioDeviationSet (170000);
	radioBitrateSet (50000);

	/* Set power output mode */

	radioWrite (KW01_PALEVEL, KW01_PALEVEL_PA0);

	/* Set node and broadcas taddress */

	radioAddressSet (1);
	radioWrite (KW01_BCASTADDR, 0xFF);

	/* Set antenna impedance, enable rX AGC,  */

	radioWrite (KW01_LNA, KW01_ZIN_200OHM|KW01_GAIN_AGC);

	/* Initialize synchronization bytes */

	radioWrite (KW01_SYNCCONF, KW01_SYNCCONF_SYNCON | KW01_SYNCSIZE_4);
	radioWrite (KW01_SYNCVAL0, 0x90);
	radioWrite (KW01_SYNCVAL1, 0x4E);
	radioWrite (KW01_SYNCVAL2, 0xDE);
	radioWrite (KW01_SYNCVAL3, 0xAD);

	/* Set squelch level. */

	radioWrite (KW01_RSSITHRESH, 0xD0);

	/* Set TX FIFO threshold -- send immediately. */

	radioWrite (KW01_FIFOTHRESH, 1);

	radioWrite (KW01_AFC_FEI, KW01_AFC_FEI_AFCACLR);

	radioWrite (KW01_RXBW, KW01_DCCFREQ_4 | KWW01_RXBW_250000);
	radioWrite (KW01_RXBWAFC, KW01_DCCFREQ_4 | KWW01_RXBW_250000);

	/* Set up interrupt event handler */

	evtTableHook (orchard_events, rf_pkt_rdy, radioIntrHandle);

	/* Display radio info. */

	version = radioRead (KW01_VERSION);

	chprintf (stream, "Radio chip version: %d Mask version: %d\r\n",
		KW01_CHIPREV(version), KW01_MASKREV(version));
	chprintf (stream, "Radio channel: %dMHz ", radioFrequencyGet());
	chprintf (stream, "Deviation: +/- %dKHz ", radioDeviationGet());
	chprintf (stream, "Bitrate: %dbps\r\n", radioBitrateGet());

	radioModeSet (KW01_MODE_RX);

	return;
}

void
radioStop (SPIDriver * sp)
{
	(void)sp;
	radioReset();
	return;
}

static void
radio_select(SPIDriver * sp)
{
	spiAcquireBus (sp);
	spiSelect (sp);
	return;
}

static void
radio_unselect(SPIDriver * sp)
{
	spiUnselect (sp);
	spiReleaseBus (sp);
}

void
radioAcquire (void)
{
	/*radio_select (&SPID1);*/
	return;
}
void
radioRelease (void)
{
	/*radio_select (&SPID1);*/
	return;
}

void
radioWrite (uint8_t addr, uint8_t val)
{
	uint8_t buf[2];
	SPIDriver * sp;

	sp = &SPID1;

	buf[0] = addr | 0x80;
	buf[1] = val;

	radio_select (sp);
	spiSend (sp, 2, buf);
	radio_unselect (sp);

	return;
}

uint8_t
radioRead (uint8_t addr)
{
	uint8_t val;
	SPIDriver * sp;

	sp = &SPID1;

	radio_select (sp);
	spiSend (sp, 1, &addr);
	spiReceive (sp, 1, &val);
	radio_unselect (sp);

	return (val);
}

uint8_t
radioAddressGet (void)
{
	uint8_t addr;

	addr = radioRead (KW01_NODEADDR);
	return (addr);
}

void
radioAddressSet (uint8_t addr)
{
	radioWrite (KW01_NODEADDR, addr);
	return;
}

void
radioDefaultHandlerSet (KW01_PKT_FUNC handler)
{
	default_handler.kw01_handler = handler;
	return;
}

void
radioHandlerSet (uint8_t prot, KW01_PKT_FUNC handler)
{
	uint8_t i;
	KW01_PKT_HANDLER * p;

	for (i = 0; i < KW01_PKT_HANDLERS_MAX; i++) {
		p = &handlers[i];
		if (p->kw01_prot == 0 || p->kw01_prot == prot) {
			p->kw01_handler = handler;
			p->kw01_prot = prot;
			break;
		}
	}

	return;
}

void
radioSend(uint8_t dest, uint8_t prot,
          size_t len, const void *payload)
{
	KW01_PKT_HDR hdr;
	uint8_t reg;
	unsigned int i;

	/* Don't send more data than will fit in the FIFO. */

	if (len > KW01_PKT_MAXLEN - KW01_PKT_HDRLEN)
		return;

	radioModeSet (KW01_MODE_STANDBY);

	hdr.kw01_length = len + sizeof(hdr);
	hdr.kw01_src = radioAddressGet();
	hdr.kw01_dst = dest;
	hdr.kw01_prot = prot;

	radio_select(&SPID1);

	reg = KW01_FIFO | 0x80;
	spiSend (&SPID1, 1, &reg);

	/* Load the header into the FIFO */
	spiSend(&SPID1, sizeof(hdr), &hdr);

	/* Load the payload into the FIFO */
	spiSend(&SPID1, len, payload);

	radio_unselect (&SPID1);

	radioModeSet (KW01_MODE_TX);

	for (i = 0; i < KW01_DELAY; i++) {
		reg = radioRead (KW01_IRQ2);
		if (reg & KW01_IRQ2_PACKETSENT)
			break;
	}

	radioModeSet (KW01_MODE_RX);

	return;
}

int
radioDump (uint8_t addr, void *bfr, int count)
{
	uint8_t i;
	uint8_t * p;

	p = bfr;
	for (i = 0; i < count; i++)
		p[i] = radioRead (addr + i);

	return (0);
}
