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

/*
 * This module implements support for the Semtech SX1233 IMS radio module.
 * The SX1233 chip is available as a standalone device, and may also be
 * embedded as the on-board radio device in an SoC, such as the
 * Freescale/NXP KW01 processor.
 *
 * The SX1233 is an Industrial Scientific and Medical (ISM) radio modem
 * that covers the 433MHz, 868MHz and 915MHZ bands, supporting FSK, GMSK, MSK
 * and OOK modulation. It also includes built-in AES-128 encryption support
 * as well as a battery monitor and temperature sensor. The packet handler
 * also includes a 66-byte FIFO.
 *
 * The radio module is programmed using a SPI interface. A set of digital
 * I/O pins are provided for signalling interrupt conditions and a CLKOUT
 * pin to which an be optionally configured to provide an image of the
 * radio's reference clock. There is also a hard reset pin.
 *
 * The SX1233 radio module supports several tunable parameters,
 * including center carrier frequency, deviation, bitrate, and RX filter
 * bandwidth. Packet filtering can be done using a bit synchronizer that
 * can be programmed to recognize up to 8 bytes of synronization data.
 * (This effectively forms a radio network ID.) Additional filtering can
 * be performed using a node ID byte, which allows for 255 unique unicast
 * addresses and one broadcast address.
 *
 * On the Freescale/NXP KW01 chip, the radio module's SPI pins are wired
 * internally to the KW01's SPI0 interface. The DIO, reset and CLKOUT pins
 * must be wired up by the board designer. On the KW019032 reference board,
 * the DIO0 pin is wired to PORTC pin 3, the DIO1 pin is wired to PORTC
 * pin 4 and the reset pin is wired to PORTE pin 30.
 *
 * Note that by default the radioReset() function is disabled. We want
 * to be able to use the radio's 32MHz crystal as a reference clock for
 * the CPU (to keep the parts count low). This is possible because the
 * radio passes through its reference clock to the CLKOUT pin. However
 * at power up or reset, it sets a divisor of 32 on the CLKOUT signal
 * by default. We need to disable this to restore it to 32MHz again,
 * which we do in the __early_init() function in ChibisOS. The problem
 * is that if we hard reset the radio again later, the clock will go
 * back to 1MHz again and slow the whole system down until we can correct
 * it again. For now we rely on the reset during __early_init() to put
 * the radio into a known state and avoid asserting its reset pin again
 * while the system is running.
 *
 * SEE ALSO:
 * http://www.nxp.com/files/microcontrollers/doc/ref_manual/MKW01xxRM.pdf
 * http://www.semtech.com/images/datasheet/sx1233.pdf
 */

#include "ch.h"
#include "hal.h"
#include "spi.h"
#include "pal.h"
#include "chprintf.h"

#include "radio_reg.h"
#include "radio_lld.h"

#include "orchard.h"
#include "orchard-events.h"

#ifndef KW01_RADIO_HWFILTER
#include "userconfig.h"
#endif

#if KINETIS_MCG_MODE == KINETIS_MCG_MODE_FEI
#define KW01_HARD_RESET
#endif /* KINETIS_MCG_MODE_FEI */

/*
 * Statically allocate memory for a single radio handle structure.
 * This includes the device state and one packet structure.
 */

RADIODriver KRADIO1;

static int radioReceive (RADIODriver *);
static void radioIntrHandle (eventid_t);
static void radioSelect (RADIODriver *);
static void radioUnselect (RADIODriver *);
static void radioSpiWrite (RADIODriver *, uint8_t, uint8_t);
static uint8_t radioSpiRead (RADIODriver *, uint8_t);
static int radioModeSet (RADIODriver *, uint8_t);

/******************************************************************************
*
* radioInterrupt - SX1233 interrupt service routine
*
* This function is triggered when the radio module asserts its interrupt pin.
* On the Freescale Freedom KW019032 board, the DIO0 pin is tied to I/O
* PORT C pin 4. The ISR announces a ChibiOS event which causes a thread-level
* handler to execute. The thread-level handler then processes the device
* events.
*
* RETURNS: N/A
*/

void
radioInterrupt (EXTDriver *extp, expchannel_t channel)
{
	(void)extp;
	(void)channel;

	chSysLockFromISR ();
	chEvtBroadcastI (&rf_pkt_rdy);
	chSysUnlockFromISR ();

	return;
}

/******************************************************************************
*
* radioReceive - receive a packet and dispatch it
*
* This function is invoked when the thread-level interrupt handler detects
* a "payload ready" event. It reads the current packet from the FIFO into
* the kw01_pkt structure, along with the current signal strength reading.
* Each packet includes a protocol type, which is compared against a list
* of pre-set protocol handlers. If no matching protocol is found, a default
* handler is called which just prints the packet contents to the serial
* port.
*
* RETURNS: 0 if the packet was dispatched successfully, or -1 if the
*          frame size is larger than KW01_PKT_MAXLEN
*/

static int
radioReceive (RADIODriver * radio)
{
	uint8_t * p;
	KW01_PKT_HANDLER * ph;
	KW01_PKT * pkt;
	uint8_t reg;
	uint8_t len;
	uint8_t i;
#ifndef KW01_RADIO_HWFILTER
	userconfig * config;
#endif

	palClearPad (GREEN_LED_PORT, GREEN_LED_PIN);   /* Green */

	pkt = &radio->kw01_pkt;

        /* Get the signal strength reading for this frame. */

	pkt->kw01_rssi = radioSpiRead (radio, KW01_RSSIVAL);

	/* Start reading from the FIFO. */

	radioSelect (radio);

	reg = KW01_FIFO;
	spiSend (radio->kw01_spi, 1, &reg);

	/* The frame length is the first byte in the FIFO. */

	spiReceive (radio->kw01_spi, 1, &len);

	if (len > radio->kw01_maxlen ||
	    len < sizeof (KW01_PKT_HDR)) {
		palSetPad (GREEN_LED_PORT, GREEN_LED_PIN);   /* Green */
		radioUnselect (radio);
		return (-1);
	}

	pkt->kw01_length = len - sizeof (KW01_PKT_HDR);

	/* Read in the he frame. */

	p = (uint8_t *)&pkt->kw01_hdr;

	spiReceive (radio->kw01_spi, len, p);

	palSetPad (GREEN_LED_PORT, GREEN_LED_PIN);   /* Green */

	radioUnselect (radio);
	radioRelease (radio);

#ifndef KW01_RADIO_HWFILTER

	/* Perform address filtering */

	config = getConfig ();

	if (pkt->kw01_hdr.kw01_dst != RADIO_BROADCAST_ADDRESS &&
	    pkt->kw01_hdr.kw01_dst != config->netid)
		return (0);

#endif

	/* Set the payload length (don't include the header length) */
	for (i = 0; i < KW01_PKT_HANDLERS_MAX; i++) {
		ph = &radio->kw01_handlers[i];
		if (ph->kw01_handler != NULL &&
		    ph->kw01_prot == pkt->kw01_hdr.kw01_prot) {
                  ph->kw01_handler (pkt);
                  return(0);
		}
	}

	if (i == KW01_PKT_HANDLERS_MAX &&
	    radio->kw01_default_handler.kw01_handler != NULL)
		radio->kw01_default_handler.kw01_handler (pkt);

	return (0);
}

/******************************************************************************
*
* radioIntrHandle - thread-level interrupt handler
*
* This handler is executed in a thread context to process radio events.
* Currently the only event we check for is PAYLOADREADY, which indicates
* that a frame has been received by the radio which has a matching set
* of sync bytes and a valid CRC. Since address filtering is enabled, we
* should also only get a frame that matches either this node's address
* or the broadcast address. If a PAYLOADREADY event occurs, we call the
* receive handler to dispatch the frame.
*
* The other event we should be triggered for is PACKETSENT, but we don't
* care about that since we use a synchronous transmit scheme.
*
* RETURNS: N/A
*/

static void
radioIntrHandle (eventid_t id)
{
	uint8_t irq1;
	uint8_t irq2;
	RADIODriver * radio;
	int sts = -1;

	(void)id;

	/*
	 * This is a bit of a hack, but there's no way to specify
	 * a context handle to the event handler function.
	 */

	radio = radioDriver;

	radioAcquire (radio);

	irq1 = radioSpiRead (radio, KW01_IRQ1);
	irq2 = radioSpiRead (radio, KW01_IRQ2);

	/* Acknowledge interrupts */

	radioSpiWrite (radio, KW01_IRQ1, irq1);
	radioSpiWrite (radio, KW01_IRQ2, irq2);

	/* We received a packet -- read it and dispatch it. */

	if (irq2 & KW01_IRQ2_PAYLOADREADY)
		sts = radioReceive (radio);

	if (sts != 0)
		radioRelease (radio);

	return;
}

#ifdef KW01_HARD_RESET
/******************************************************************************
*
* radioReset - force a reset of the SX1233 chip
*
* The SX1233 module has a reset pin which can be asserted to force a complete
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
radioReset (RADIODriver * radio)
{
	(void)radio;

        /* Assert the reset pin for 1 millisecond */

        palSetPad (RADIO_RESET_PORT, RADIO_RESET_PIN);
        chThdSleepMilliseconds (1);
        palClearPad (RADIO_RESET_PORT, RADIO_RESET_PIN);

 	/* Now wait for the chip to get its brains in order. */

	chThdSleepMilliseconds (20);

	radioWrite (radio, KW01_DIOMAP2, KW01_CLKOUT_FXOSC);

	return;
}
#endif

/******************************************************************************
*
* radioModeSet - set the radio mode
*
* This function selects the current radio mode. It must be one of STANDBY,
* SLEEP, RX or TX. The function checks that a MODEREADY event is signalled
* in the IRQ1 register before returning.
*
* RETURNS: 0 if the mode is set successfully or -1 if the requested mode
*          is invalid or if setting the mode times out
*/

static int
radioModeSet (RADIODriver * radio, uint8_t mode)
{
	unsigned int i;

	if (mode != KW01_MODE_STANDBY &&
	    mode != KW01_MODE_SLEEP &&
	    mode != KW01_MODE_RX &&
	    mode != KW01_MODE_TX)
		return (-1);

	radioSpiWrite (radio, KW01_OPMODE, mode);

	for (i = 0; i < KW01_DELAY; i++) {
		if (radioSpiRead (radio, KW01_IRQ1) & KW01_IRQ1_MODEREADY)
			break;
	}

	if (i == KW01_DELAY)
		return (-1);

	return (0);
}

/******************************************************************************
*
* radioFrequencySet - set the current radio carrier frequency
*
* This function sets the radio's current center carrier frequency. The
* frequency is specified in Hz, and is confined to a range of 430MHz to
* 920MHz. (The radio might actually support a wider range than this, but
* for now this range is suitable for the intended application.)
*
* The frequency range is specified as a 24-bit value which is a fraction
* of the radio's crystal frequency. The KW01 board uses a 32MHz crystal,
* which results in a minimum frequency setp size of approximately 61Hz.
* This constitutes the radio's minimum tuning resolution.
*
* RETURNS: 0 if the mode is set successfully or -1 if the requested
*          frequnecy is out of range
*/

int
radioFrequencySet (RADIODriver * radio, uint32_t freq)
{
	double val;
	uint32_t regval;

	if (freq < 430000000 || freq > 920000000)
		return (-1);

	val = KW01_FREQ(freq);
	regval = (uint32_t)val;

	radioAcquire (radio);
	radioSpiWrite (radio, KW01_FRFMSB, regval >> 16);
	radioSpiWrite (radio, KW01_FRFISB, regval >> 8);
	radioSpiWrite (radio, KW01_FRFLSB, regval & 0xFF);
	radioRelease (radio);

	return (0);
}

/******************************************************************************
*
* radioFrequencyGet - get the current radio carrier frequency
*
* This function reads the radio's current center carrier frequency and
* returns it to the caller.
*
* RETURNS: the current radio center frequency in Hz
*/

uint32_t
radioFrequencyGet (RADIODriver * radio)
{
	double val;
	uint32_t regval;

	radioAcquire (radio);
	regval = radioSpiRead (radio, KW01_FRFLSB);
	regval |= radioSpiRead (radio, KW01_FRFISB) << 8;
	regval |= radioSpiRead (radio, KW01_FRFMSB) << 16;
	radioRelease (radio);

	val = (double)regval * KW01_FSTEP;
	regval = (uint32_t)val;

	return (regval);
}

/******************************************************************************
*
* radioDeviationSet - set the current radio frequency deviation setting
*
* This function sets the radio's current deviation setting. The deviation is
* specified in Hz and must be within the range of 600 to 200000Hz.
*
* RETURNS: 0 if the bitrate was set successfully or -1 if the requested
*          bitrate value is out of range
*/

int
radioDeviationSet (RADIODriver * radio, uint32_t deviation)
{
	double val;
	uint32_t regval;

	if (deviation < 600 || deviation > 200000)
		return (-1);

	val = KW01_FREQ(deviation);
	regval = (uint32_t)val;

	radioAcquire (radio);
	radioSpiWrite (radio, KW01_FDEVMSB, regval >> 8);
	radioSpiWrite (radio, KW01_FDEVLSB, regval & 0xFF);
	radioRelease (radio);

	return (0);
}

/******************************************************************************
*
* radioDeviationGet - get the current radio deviation setting
*
* This function reads the radio's current deviation setting and returns
* it to the caller.
*
* RETURNS: the current radio deviation setting in Hz
*/

uint32_t
radioDeviationGet (RADIODriver * radio)
{
	double val;
	uint32_t regval;

	radioAcquire (radio);
	regval = radioSpiRead (radio, KW01_FDEVLSB);
	regval |= radioSpiRead (radio, KW01_FDEVMSB) << 8;
	radioRelease (radio);

	val = (double)regval * KW01_FSTEP;
	regval = (uint32_t)val;

	return (regval);
}

/******************************************************************************
*
* radioBitrateSet - set the current radio bitrate setting
*
* This function sets the radio's current bitrate setting. The bitrate is
* specified as a baud rate and must be within the range of 1200 to 200000
* baud.
*
* RETURNS: 0 if the bitrate was set successfully or -1 if the requested
*          bitrate value is out of range
*/

int
radioBitrateSet (RADIODriver * radio, uint32_t bitrate)
{
	double val;
	uint32_t regval;

	if (bitrate < 1200 || bitrate > 200000)
		return (-1);

	val = KW01_BITRATE(bitrate);
	regval = (uint32_t)val;

	radioAcquire (radio);
	radioSpiWrite (radio, KW01_BITRATEMSB, regval >> 8);
	radioSpiWrite (radio, KW01_BITRATELSB, regval & 0xFF);
	radioRelease (radio);

	return (0);
}

/******************************************************************************
*
* radioBitrateGet - get the current radio bitrate setting
*
* This function reads the radio's current bitrate setting and returns
* it to the caller.
*
* RETURNS: the current radio bitrate setting in bps
*/

uint32_t
radioBitrateGet (RADIODriver * radio)
{
	double val;
	uint32_t regval;

	radioAcquire (radio);
	regval = radioSpiRead (radio, KW01_BITRATELSB);
	regval |= radioSpiRead (radio, KW01_BITRATEMSB) << 8;
	radioRelease (radio);

	val = (double)KW01_XTAL_FREQ / (double)regval;
	regval = (uint32_t)val;

	return (regval);
}

/******************************************************************************
*
* radioStart - initialize and start the radio
*
* This function resets and initializes the SX1233 radio. The radio module's
* reset pin is toggled, and the radio is programmed for FSK modulation
* with a deviation of 170000Hz and a bitrate of 50K baud. The default
* carrier frequency is 902.5MHz. Address filtering is enabled and the
* bit synchronizer is enabled with a 6 byte sync value. (Note: all radios
* must use the same sync bytes to communicate.) An RSSITHRESH value is
* also set, which indicates the radio's squelch setting.
*
* Once initialization is complete, the radio is put into the receive state.
*
* RETURNS: N/A
*/

void
radioStart (SPIDriver * sp)
{
	uint8_t version;
	RADIODriver * radio;
	uint8_t syncbytes[6] = { 0x90, 0x4e, 0xde, 0xad, 0xbe, 0xef };
	uint8_t key[16] = { 0x01, 0x02, 0x03, 0x4,
			    0x01, 0x02, 0x04, 0x4,
			    0x01, 0x02, 0x04, 0x4,
			    0x01, 0x02, 0x04, 0x4 };
#ifdef KW01_RSSI_CALIBRATE
	unsigned int i;
	uint8_t rssi;
#endif

	radio = radioDriver;

	osalMutexObjectInit (&radio->kw01_mutex);

	radio->kw01_spi = sp;

#ifdef KW01_HARD_RESET
	/* Reset radio hardware to known state. */

	radioReset (radio);
#endif

	/* Put radio in standby mode */

	radioAcquire (radio);
	radioModeSet (radio, KW01_MODE_STANDBY);
	radioRelease (radio);

	/* Select variable length packet mode */

	radioWrite (radio, KW01_DATMOD, KW01_DATAMODE_PACKET |
		    KW01_MODULATION_FSK | KW01_MODSHAPE_GFBT03);

	/*
	 * Enable whitening and CRC insertion/checking,
	 * select unicast/broadcast address filtering mode.
	 */

	radioWrite (radio, KW01_PKTCONF1, KW01_FORMAT_VARIABLE |
		    KW01_DCFREE_WHITENING | KW01_PKTCONF1_CRCON |
#ifdef KW01_RADIO_HWFILTER
		    KW01_AFILT_UCASTBCAST);
#else
		    KW01_AFILT_NONE);
#endif

	radio->kw01_flags = 0;
	radio->kw01_maxlen = KW01_PKT_MAXLEN;

	radioWrite (radio, KW01_PKTCONF2, KW01_PKTCONF2_IPKTDELAY |
	    KW01_PKTCONF2_AUTORRX);
	radioWrite (radio, KW01_PAYLEN, KW01_PKT_MAXLEN);
	radioWrite (radio, KW01_DIOMAP1, 0x40);

	radioWrite (radio, KW01_PREAMBLEMSB, 0);
	radioWrite (radio, KW01_PREAMBLELSB, 0xF);

	/* Select frequency, deviation and bitrate */

	radioFrequencySet (radio, 902500000);
	radioDeviationSet (radio, 170000);
	radioBitrateSet (radio, 50000);

	/* Set power output mode and power level to max */

	radioWrite (radio, KW01_PALEVEL, KW01_PALEVEL_PA0 | KW01_TXPWR(31));

#ifdef KW01_RADIO_HWFILTER
	/* Set node and broadcast address */

	radioAddressSet (radio, 1);
	radioWrite (radio, KW01_BCASTADDR, 0xFF);
#endif

	/* Set antenna impedance, enable RX AGC, */

	radioWrite (radio, KW01_LNA, KW01_ZIN_200OHM|KW01_GAIN_AGC);

	/* Initialize synchronization bytes */

	radioWrite (radio, KW01_SYNCCONF, KW01_SYNCCONF_SYNCON);
	radioNetworkSet (radio, syncbytes, 6);

	/* Set TX FIFO threshold -- send immediately. */

	radioWrite (radio, KW01_FIFOTHRESH, 1);

	radioWrite (radio, KW01_AFC_FEI, KW01_AFC_FEI_AFCACLR);

	radioWrite (radio, KW01_RXBW, KW01_DCCFREQ_4 | KWW01_RXBW_250000);
	radioWrite (radio, KW01_RXBWAFC, KW01_DCCFREQ_4 | KWW01_RXBW_250000);

	/* Enable AES and set the key. */

	radioAesEnable (radio, key, 16);

	/* Set up interrupt event handler */

	evtTableHook (orchard_events, rf_pkt_rdy, radioIntrHandle);

	/* Display radio info. */

	version = radioRead (radio, KW01_VERSION);

	chprintf (stream, "Radio chip version: %d Mask version: %d\r\n",
		  KW01_CHIPREV(version), KW01_MASKREV(version));
	chprintf (stream, "Radio channel: %dHz ", radioFrequencyGet (radio));
	chprintf (stream, "Deviation: +/- %dKHz ", radioDeviationGet (radio));
	chprintf (stream, "Bitrate: %dbps\r\n", radioBitrateGet (radio));

	/* Go into receive mode. */

	radioAcquire (radio);
	radioModeSet (radio, KW01_MODE_RX);
	radioRelease (radio);

#ifdef KW01_RSSI_CALIBRATE
	/* Calibrate the squelch level. Set squelch to full open, */

	radioWrite (radio, KW01_RSSITHRESH, 0xFF);

	/* Initiate an RSSI reading */

	radioWrite (radio, KW01_RSSICONF, KW01_RSSICONF_START);

	/* Wait until it finishes. */

	for (i = 0; i < KW01_DELAY; i++) {
		version = radioRead (radio, KW01_RSSICONF);
		if (version & KW01_RSSICONF_DONE)
			break;
        	chThdSleepMilliseconds (1);
	}

	/* Get the result. */

	rssi = radioRead (radio, KW01_RSSIVAL);

	chprintf (stream, "Background noise level: -%ddBm\r\n", rssi / 2);

	/*
	 * Program the squelch threshold to be a few dBm below the
	 * current noise level.
	 */

	rssi -= 16;
	radioWrite (radio, KW01_RSSITHRESH, rssi);

	chprintf (stream, "Auto-calibrated squelch level: -%ddBm\r\n",
		  rssi / 2);
#else
	radioWrite (radio, KW01_RSSITHRESH, 0xFF);
#endif
	return;
}

/******************************************************************************
*
* radioStop - halt the radio
*
* This function stops all radio activity by forcing a hard reset of the
* SX1233 module via its reset pin.
*
* RETURNS: N/A
*/

void
radioStop (RADIODriver * radio)
{
#ifdef KW01_HARD_RESET
	radioReset (radio);
#endif
	radioModeSet (radio, KW01_MODE_SLEEP);
	return;
}

/******************************************************************************
*
* radioAcquire - acquire access to the radio's SPI interface
*
* This initializes SPI communication to the radio module. This must be done
* prior to every SPI read/write transaction.
*
* This function must not be called twice in a row.
*
* RETURNS: N/A
*/

void
radioAcquire (RADIODriver * radio)
{
	osalMutexLock (&radio->kw01_mutex);
	return;
}

/******************************************************************************
*
* radioRelesse - release access to the radio's SPI interface
*
* This relinquishes access to the radio's SPI interface. This must be done
* at the completion of every SPI read/write transaction.
*
* This function must not be called twice in a row.
*
* RETURNS: N/A
*/

void
radioRelease (RADIODriver * radio)
{
	osalMutexUnlock (&radio->kw01_mutex);
	return;
}

/******************************************************************************
*
* radioSelect - select the radio's SPI interface
*
* This functions acquires access to the radio's SPI channel and prepares
* it for a SPI transaction. This must be done before every SPI read/write.
*
* RETURNS: N/A
*/

static void
radioSelect (RADIODriver * radio)
{
	spiAcquireBus (radio->kw01_spi);
	spiSelect (radio->kw01_spi);
	return;
}

/******************************************************************************
*
* radioUnselect - deselect the radio's SPI interface
*
* This functions relinquishes access to the radio's SPI channel after a
* SPI transaction. This must be done after every SPI read/write.
*
* RETURNS: N/A
*/

static void
radioUnselect (RADIODriver * radio)
{
	spiUnselect (radio->kw01_spi);
	spiReleaseBus (radio->kw01_spi);
	return;
}

/******************************************************************************
*
* radioSpiWrite - write a value to a radio register
*
* This function writes a value to one of the radio's registers via the SPI
* interface. The write transaction is in the form of a two byte request
* containing the register offset and the value to write.
*
* This function does not acquire exclusive access to the radio.
*
* RETURNS: N/A
*/

static void
radioSpiWrite (RADIODriver * radio, uint8_t addr, uint8_t val)
{
	uint8_t buf[2];

	buf[0] = addr | 0x80;
	buf[1] = val;

	radioSelect (radio);
	spiSend (radio->kw01_spi, 2, buf);
	radioUnselect (radio);

	return;
}

/******************************************************************************
*
* radioWrite - write a value to a radio register, with mutual exclusion
*
* This function writes a value to one of the radio's registers via the SPI
* interface. The write transaction is in the form of a two byte request
* containing the register offset and the value to write.
*
* This function acquires exclusive access to the radio.
*
* RETURNS: N/A
*/

void
radioWrite (RADIODriver * radio, uint8_t addr, uint8_t val)
{
	radioAcquire (radio);
	radioSpiWrite (radio, addr, val);
	radioRelease (radio);

	return;
}

/******************************************************************************
*
* radioSpiRead - read a value from a radio register
*
* This function reads a value from one of the radio's registers via the SPI
* interface. The read transaction is in the form of a one byte request
* containing the register offset and a one byte response containing the
* result.
*
* This function does not acquire exclusive access to the radio.
*
* RETURNS: the requested 8-bit register value
*/

static uint8_t
radioSpiRead (RADIODriver * radio, uint8_t addr)
{
	uint8_t val;

	radioSelect (radio);
	spiSend (radio->kw01_spi, 1, &addr);
	spiReceive (radio->kw01_spi, 1, &val);
	radioUnselect (radio);

	return (val);
}

/******************************************************************************
*
* radioRead - read a value from a radio register, with mutual exclusion
*
* This function reads a value from one of the radio's registers via the SPI
* interface. The read transaction is in the form of a one byte request
* containing the register offset and a one byte response containing the
* result.
*
* This function acquires exclusive access to the radio.
*
* RETURNS: the requested 8-bit register value
*/

uint8_t
radioRead (RADIODriver * radio, uint8_t addr)
{
	uint8_t val;

	radioAcquire (radio);
	val = radioSpiRead (radio, addr);
	radioRelease (radio);

	return (val);
}

#ifdef KW01_RADIO_HWFILTER
/******************************************************************************
*
* radioAddressGet - get the radio node address
*
* This function obtains the radio's current node address. This indicates the
* destination address that must be specified when sending a packet in order
* for this radio to receive it. (A packet sent to the broadcast address will
* also be received.)
*
* RETURNS: the current radio node address
*/

uint8_t
radioAddressGet (RADIODriver * radio)
{
	uint8_t addr;

	addr = radioRead (radio, KW01_NODEADDR);
	return (addr);
}

/******************************************************************************
*
* radioAddressSet - set the radio node address
*
* This function sets the radio's current node address. This indicates the
* destination address that must be specified when sending a packet in order
* for this radio to receive it. (A packet sent to the broadcast address will
* also be received.)
*
* RETURNS: N/A
*/

void
radioAddressSet (RADIODriver * radio, uint8_t addr)
{
	radioWrite (radio, KW01_NODEADDR, addr);
	return;
}
#endif

/******************************************************************************
*
* radioDefaultHandlerSet - set default received packet handler
*
* This function initializes the default handler which will be invoked if
* no matching protocol handler for a received frame is found. Usually
* a handler is used that just dumps the packet contents to the serial port
* for debugging purposes.
*
* RETURNS: N/A
*/

void
radioDefaultHandlerSet (RADIODriver * radio, KW01_PKT_FUNC handler)
{
	radio->kw01_default_handler.kw01_handler = handler;
	return;
}

/******************************************************************************
*
* radioHandlerSet - install a handler for a given protocol type
*
* This function adds a packet protocol handler to the handler list. The
* first free entry in the list will be used to hold the handler. If the
* specified protocol value matches an existing entry, then that entry
* will be overwritten.
*
* RETURNS: N/A
*/

void
radioHandlerSet (RADIODriver * radio, uint8_t prot, KW01_PKT_FUNC handler)
{
	uint8_t i;
	KW01_PKT_HANDLER * p;

	for (i = 0; i < KW01_PKT_HANDLERS_MAX; i++) {
		p = &radio->kw01_handlers[i];
		if (p->kw01_handler == NULL || p->kw01_prot == prot) {
			p->kw01_handler = handler;
			p->kw01_prot = prot;
			break;
		}
	}

	return;
}

/******************************************************************************
*
* radioSend - transmit a packet
*
* This function transmits a frame over the air. The frame data must be
* less than 61 bytes in size. The radio is put into the standby mode
* and then the packet length, destination address and payload are loaded
* into the FIFO. Once the data is loaded, the radio is set to the transmit
* state. The routine then polls for the PACKETSET status to be indicated
* before putting the radio back into receive mode again.
*
* RETURNS: 0 if transmission was successful, or -1 if the frame was too
*          large or initiating transmission failed
*/

int
#ifdef KW01_RADIO_HWFILTER
radioSend (RADIODriver * radio, uint8_t dest, uint8_t prot,
           uint8_t len, const void *payload)
#else
radioSend (RADIODriver * radio, uint32_t dest, uint8_t prot,
           uint8_t len, const void *payload)
#endif
{
	KW01_PKT_HDR hdr;
	uint8_t reg;
	unsigned int i;
#ifndef KW01_RADIO_HWFILTER
	userconfig * config;
#endif

	/* Don't send more data than will fit in the FIFO. */

	if (len > (radio->kw01_maxlen - KW01_PKT_HDRLEN))
		return (-1);

	radioAcquire (radio);

	palClearPad (RED_LED_PORT, RED_LED_PIN);  /* Red */

	if (radioModeSet (radio, KW01_MODE_STANDBY) != 0) {
		palSetPad (RED_LED_PORT, RED_LED_PIN);  /* Red */
		radioRelease (radio);
		return (-1);
	}

#ifdef KW01_RADIO_HWFILTER
	hdr.kw01_src = radioSpiRead (radio, KW01_NODEADDR);
#else
	config = getConfig ();
	hdr.kw01_src = config->netid;
#endif
	hdr.kw01_dst = dest;
	hdr.kw01_prot = prot;

	/* Start a SPI write transaction. */

	radioSelect (radio);

	reg = KW01_FIFO | 0x80;
	spiSend (radio->kw01_spi, 1, &reg);

	/* Write the frame length -- not really part of the payload. */

	reg = len + sizeof (hdr);

	spiSend (radio->kw01_spi, 1, &reg);

	/* Load the header into the FIFO */

	spiSend (radio->kw01_spi, sizeof(hdr), &hdr);

	/* Load the payload into the FIFO */

	spiSend (radio->kw01_spi, len, payload);

	radioUnselect (radio);

	palSetPad (RED_LED_PORT, RED_LED_PIN);  /* Red */

	if (radioModeSet (radio, KW01_MODE_TX) != 0) {
		radioRelease (radio);
		return (-1);
	}

	for (i = 0; i < KW01_DELAY * 100; i++) {
		reg = radioSpiRead (radio, KW01_IRQ2);
		if (reg & KW01_IRQ2_PACKETSENT)
			break;
	}

	radioModeSet (radio, KW01_MODE_RX);
	radioRelease (radio);

	if (i == KW01_DELAY)
		return (-1);

	return (0);
}

/******************************************************************************
*
* radioNetworkGet - get the current network ID
*
* This function reads the current network ID in the form of the syncbytes
* configuration in the radio. There can be up to 8 sync bytes, so the
* supplied buffer must be large enought to hold however many bytes are
* currently configured. The number of bytes is returned via the len
* parameter.
*
* RETURNS: 0 if the sync bytes can be returned, or -1 if the supplied
*          buffer is too small
*/

int
radioNetworkGet (RADIODriver * radio, uint8_t * net, uint8_t * len)
{
	uint8_t i;

	i = radioRead (radio, KW01_SYNCCONF);
	i = (i & KW01_SYNCCONF_SYNCSIZE) >> 3;

	if (*len < i)
		return (-1);

	*len = i;

	for (i = 0; i < *len; i++)
		net[i] = radioRead (radio, KW01_SYNCCONF + i);

	return (0);
}

/******************************************************************************
*
* radioNetworkSet - set the current network ID
*
* This function sets the current network ID in the form of the syncbytes
* configuration in the radio. There can be up to 8 sync bytes, so the
* supplied buffer should contain no more than 8 bytes. The sync bytes may
* be any valey except 0.
*
* RETURNS: 0 if the sync bytes can be set, or -1 if the supplied
*          network ID is too large
*/

int
radioNetworkSet (RADIODriver * radio, const uint8_t * net, uint8_t len)
{
	uint8_t i;

	if (len > 8)
		return (-1);

	/*
	 * Check that the requested network ID doesn't contain any
	 * 0 bytes as these are not allowed by the hardware.
	 */

	for (i = 0; i < len; i++) {
		if (net[i] == 0)
			return (-1);
	}

	/* Write the sync bytes */

	for (i = 0; i < len; i++)
		radioWrite (radio, KW01_SYNCVAL0 + i, net[i]);

	/* Update the sync size */

	i = radioRead (radio, KW01_SYNCCONF);
	i &= ~(KW01_SYNCCONF_SYNCSIZE);
	i |= KW01_SYNCSIZE(len);
	radioWrite(radio, KW01_SYNCCONF, i);

	return (0);
}

/******************************************************************************
*
* radioAesEnable - enable AES encryption/decryption and set the key
*
* This function enables hardware AES encryption/decryption of radio payloads
* and programs the AES key. The SX1233 uses AES-128, meaning that the key
* can be up to 16 bytes in size.
*
* Once the key is programmed, the AES enable bit is set in the PacketConfig2
* register. Once this is done, all outbound payloads will be encrypted and
* only payloads encrypted with the same key will be accepted from other
* radios.
*
* Note that the AES key registers are write-only.
*
* The radioAesDisable() function should be called before this routine in
* order to ensure that unused key bytes are zeroed.
*
* RETURNS: 0 if AES is successfully enabled, or -1 if the key is too large
*          or if AES is already turned on
*/

int
radioAesEnable (RADIODriver * radio, const uint8_t * key, uint8_t len)
{
	uint8_t i;
	uint8_t pktconf;

	/* Check that the key lenght is valid. */

	if (len > 16)
		return (-1);

	/* Check that AES isn't already enabled. */

	radioAcquire (radio);

	pktconf = radioSpiRead (radio, KW01_PKTCONF2);
	if (pktconf & KW01_PKTCONF2_AESON) {
		radioRelease (radio);
		return (-1);
	}

	/* Set the key. */

	for (i = 0; i < len; i++)
		radioSpiWrite (radio, KW01_AESKEY0 + i, key[i]);

	/* Enable AES encryption/decryption. */

	pktconf |= KW01_PKTCONF2_AESON;
	radioSpiWrite (radio, KW01_PKTCONF2, pktconf);
	radio->kw01_flags |= KW01_FLAG_AES;

	/*
	 * When AES encryption is turned on, packets may be up to 64
	 * bytes in size when address filtering is off, or up to 48
	 * bytes in size when it's turned on. So if address filtering
	 * is enabled (which it usually is), then we have to adjust
	 * the packet size limit accordingly.
	 */

	radio->kw01_maxlen = KW01_PKT_AES_MAXLEN;

	radioRelease (radio);

	return (0);
}

/******************************************************************************
*
* radioAesDisable - disable AES encryption/decryption and clear the key
*
* This function forces off AES encryption/decryption of packet payloads
* by clearing the AES enable bit in the PacketConfig2 register and zeroes
* out the AES key bytes.
*
* This function should be called before using radioAesEnable() to turn AES
* on and set a new key.
*
* RETURNS: N/A
*/

void
radioAesDisable (RADIODriver * radio)
{
	uint8_t i;

	/* Turn off AES encryption/decryption. */

	radioAcquire (radio);

	i = radioSpiRead (radio, KW01_PKTCONF2);
	i &= ~(KW01_PKTCONF2_AESON);
	radioSpiWrite (radio, KW01_PKTCONF2, i);

	/* Clear the key. */

	for (i = 0; i < 16; i++)
		radioSpiWrite (radio, KW01_AESKEY0 + i, 0);

	radio->kw01_flags &= ~KW01_FLAG_AES;
	radio->kw01_maxlen = KW01_PKT_MAXLEN;

	radioRelease (radio);

	return;
}

/******************************************************************************
*
* radioTemperatureGet - read the radio temperature sensor
*
* This function performs a temperature measurement using the radio's on-board
* sensor and returns the result. The radio's internal ADC is used to obtain
* the measurement, so the radio must be either in standby or frequency
* synthesis mode. This means the radio has to be be briefly turned off
* in order to make the measurement. The receiver is re-enabled once the
* temperature reading is complete.
*
* RETURNS: temperature reading in degrees celsius or -1 if reading the
*          sensor fails
*/

int
radioTemperatureGet (RADIODriver * radio)
{
	int temp;
	unsigned int i;

	radioAcquire (radio);

	if (radioModeSet (radio, KW01_MODE_STANDBY) != 0) {
		radioRelease (radio);
		return (-1);
	}

	radioSpiWrite (radio, KW01_TEMPCTL, KW01_TEMPCTL_START);

	for (i = 0; i < KW01_DELAY; i++) {
		temp = radioSpiRead (radio, KW01_TEMPCTL);
		if ((temp & KW01_TEMPCTL_BUSY) == 0)
			break;
	}

	if (radioModeSet (radio, KW01_MODE_RX) != 0) {
		radioRelease (radio);
		return (-1);
	}

	if (i == KW01_DELAY) {
		radioRelease (radio);
		return (-1);
	}

	temp = radioSpiRead (radio, KW01_TEMPVAL);

	radioRelease (radio);

	/* Convert sign. */

	temp = 255 - temp;

	/*
	 * Adjust to degrees celsius based on an approximate
	 * calibration value.
	 */

	temp -= 99;

	return (temp);
}

/******************************************************************************
*
* radioDump - dump the radio registers
*
* This function dumps a range of radio registers and places them into
* the buffer supplied by the caller. This is used for debugging purposes
* to examine the radio state.
*
* RETURNS: N/A
*/

void
radioDump (RADIODriver * radio, uint8_t addr, void *bfr, int count)
{
	uint8_t i;
	uint8_t * p;

	p = bfr;
	for (i = 0; i < count; i++)
		p[i] = radioRead (radio, addr + i);

	return;
}
