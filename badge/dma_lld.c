/*-
 * Copyright (c) 2017
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
 * This module implements support for using the DMA controllers in the
 * Freescale/NXP KW01 in conjunction with SPI channel 1. This allows us to
 * perform high speed transfers over the SPI bus for the display and SD card
 * with minimal CPU overhead.
 *
 * The KW01 has four DMA controllers which may be configured to perform either
 * memory/memory transfers, or peripheral/memory transfers, for a number of
 * different peripherals. Our board design requires the use of 8-bit transfers
 * for the SD card and 16-bit transfers for the display. We use three of the
 * DMA channels, each configured for a different mode:
 *
 * Channel 0: SPI1 8-bit TX
 * Channel 1: SPI1 8-bit RX
 * Channel 2: SPI1 16-bit TX
 * Channel 3: unused
 *
 * We could re-configure the channels on the fly, however we prefer to keep
 * them statically configured to reduce the amount of instructions required
 * to perform each transfer.
 *
 * We provide interrupt handling support such that each channel generates an
 * interrupt when a transfer completes, allowing the calling thread to sleep
 * and other threads to execute while the transfer is in progress.
 *
 * Getting the DMA controller to work with the SPI controller depends on
 * one important configuration option: we must enable "cycle-steal" mode, by
 * setting the CS bit in the DCR register. This allows the SPI1 controller to
 * stop and restart the DMA channel as needed as data is received or
 * transmitted. Memory/memory transfers don't need "cycle-steal" mode enabled.
 */

#include "ch.h"
#include "hal.h"
#include "osal.h"

#include "dma_lld.h"

static thread_reference_t dma0Thread;
static thread_reference_t dma1Thread;
static thread_reference_t dma2Thread;

/******************************************************************************
*
* dmaIntHandle - DMA interrupt handler
*
* This function is called whenever one of the three DMA channel interrupts
* is triggered. It acknowledges the transfer completion interrupt and wakes
* up any thread that might be waiting for the transfer to finish. This code
* is broken out as a helper routine as it is common to all three interrupt
* handlers. The <chan> argument indicates the DMA controller channel (0-2)
* and the <thd> argument is a pointer to a thread reference. If thd is
* NULL, osalThreadResumeI() has no effect.
*
* RETURNS: N/A
*/

static void
dmaIntHandle (uint8_t chan, thread_reference_t * thd)
{
	if (DMA->ch[chan].DSR_BCR & DMA_DSR_BCRn_DONE) {

		/*
		 * Wake any thread waiting for this
		 * DMA transfer to finish
		 */

		osalSysLockFromISR ();
		osalThreadResumeI (thd, MSG_OK);
		osalSysUnlockFromISR ();                          

		/* Ack the DMA completion interrupt */

		DMA->ch[chan].DSR_BCR |= DMA_DSR_BCRn_DONE;
	}

	return;
}

/******************************************************************************
*
* Vector40 - interrupt service routine for DMA channel 0
*
* This function is invoked when DMA channel 0 triggers an interrupt. It uses
* the dmaIntHandle() halper routine to ack the interrupt.
*
* RETURNS: N/A
*/

OSAL_IRQ_HANDLER(KINETIS_DMA0_IRQ_VECTOR)
{
	OSAL_IRQ_PROLOGUE();
	dmaIntHandle (0, &dma0Thread);
	OSAL_IRQ_EPILOGUE();
	return;
}

/******************************************************************************
*
* Vector44 - interrupt service routine for DMA channel 1
*
* This function is invoked when DMA channel 1 triggers an interrupt. It uses
* the dmaIntHandle() halper routine to ack the interrupt.
*
* RETURNS: N/A
*/

OSAL_IRQ_HANDLER(KINETIS_DMA1_IRQ_VECTOR)
{
	OSAL_IRQ_PROLOGUE();
	dmaIntHandle (1, &dma1Thread);
	OSAL_IRQ_EPILOGUE();
	return;
}

/******************************************************************************
*
* Vector48 - interrupt service routine for DMA channel 2
*
* This function is invoked when DMA channel 2 triggers an interrupt. It uses
* the dmaIntHandle() halper routine to ack the interrupt.
*
* RETURNS: N/A
*/

OSAL_IRQ_HANDLER(KINETIS_DMA2_IRQ_VECTOR)
{
	OSAL_IRQ_PROLOGUE();
	dmaIntHandle (2, &dma2Thread);
	OSAL_IRQ_EPILOGUE();
	return;
}

/******************************************************************************
*
* dmaStart - enable and configure the DMA channels
*
* This function enables the DMA controller module in the KW01 and configures
* the first three channels for use with the SPI1 controller. The following
* channel setup is used:
*
* Channel0: SPI1 TX, 8-bit transfers, source increment
* Channel1: SPI1 RX, 8-bit transfers, destination increment
* Channel2: SPI1 TX, 16-bit transfers, source increment
*
* The NVIC is also programmed to enable the interrupts corresponding each
* channel.
*
* RETURNS: N/A
*/

void
dmaStart (void)
{
	/* Enable clocks for DMA and DMAMUX modules. */

        SIM->SCGC6 |= SIM_SCGC6_DMAMUX;
        SIM->SCGC7 |= SIM_SCGC7_DMA;

	/* Initialize channel 0 for SPI TX */

	DMAMUX->CHCFG[0] = DMAMUX_CHCFGn_ENBL |
	    DMAMUX_CHCFGn_SOURCE(DMA_CHAN_SPI1_TX);

	DMA->ch[0].DCR = DMA_DCRn_EINT | DMA_DCRn_D_REQ |
	    DMA_DCRn_SSIZE(1) | DMA_DCRn_DSIZE(1) |
	    DMA_DCRn_SINC | DMA_DCRn_CS | DMA_DCRn_EADREQ;
	DMA->ch[0].DAR = (uint32_t)&(SPI1->DL);

	/* Initialize channel 1 for SPI RX */

	DMAMUX->CHCFG[1] = DMAMUX_CHCFGn_ENBL |
	    DMAMUX_CHCFGn_SOURCE(DMA_CHAN_SPI1_RX);

	DMA->ch[1].DCR = DMA_DCRn_EINT | DMA_DCRn_D_REQ |
	    DMA_DCRn_SSIZE(1) | DMA_DCRn_DSIZE(1) |
	    DMA_DCRn_DINC | DMA_DCRn_CS | DMA_DCRn_EADREQ;
	DMA->ch[1].SAR = (uint32_t)&(SPI1->DL);

	/* Initialize channel 2 for SPI TX with 16-bit transfers */

	DMAMUX->CHCFG[2] = DMAMUX_CHCFGn_ENBL |
	    DMAMUX_CHCFGn_SOURCE(DMA_CHAN_SPI1_TX);

	DMA->ch[2].DCR = DMA_DCRn_EINT | DMA_DCRn_D_REQ |
	    DMA_DCRn_SSIZE(2) | DMA_DCRn_DSIZE(2) |
	    DMA_DCRn_SINC | DMA_DCRn_CS | DMA_DCRn_EADREQ;
	DMA->ch[2].DAR = (uint32_t)&(SPI1->DL);

	/* Enable IRQ vectors */

	nvicEnableVector (DMA0_IRQn, 5);
	nvicEnableVector (DMA1_IRQn, 5);
	nvicEnableVector (DMA2_IRQn, 5);

	return;
}

/******************************************************************************
*
* dmaSend8 - perform an 8-bit DMA transmit operation on SPI controller 1
*
* This function uses DMA channel 0 to transmit a block of data to a slave
* connected to the SPI1 controller. The data is transmitted a byte at a
* a time. The <src> argument is a pointer to the memory buffer to be sent
* and <len> indicates the amount of data to transfer in bytes. The <src>
* address can be anywhere in RAM or flash.
*
* The calling thread will be suspended while the transfer is in progress.
* It will be woken up when the DMA completion interrupt fires. Only a
* single channel is needed for this operation. Any data returned by the
* slave is ignored.
*
* RETURNS: N/A
*/

void
dmaSend8 (const void * src, uint32_t len)
{
	SPI1->C3 |= SPIx_C3_FIFOMODE;

	DMA->ch[0].DSR_BCR = len;
	DMA->ch[0].SAR = (uint32_t)src;

	/* Initiate the transfer */

	osalSysLock ();

	SPI1->C2 |= SPIx_C2_TXDMAE;
	DMA->ch[0].DCR |= DMA_DCRn_ERQ;
	osalThreadSuspendS (&dma0Thread);

	osalSysUnlock ();
 
	SPI1->C2 &= ~SPIx_C2_TXDMAE;
	SPI1->C3 &= ~SPIx_C3_FIFOMODE;

	return;
}

/******************************************************************************
*
* dmaRecv8 - perform an 8-bit DMA receive operation on SPI controller 1
*
* This function uses DMA channel 0 and DMA channel 1 to receive a block of
* data from a slave connected to the SPI1 controller. The data is received
* a byte at a time. The <dst> argument is a pointer to a memory buffer
* where the received data should be deposited and <len> indicates the amount
* of data to transfer in bytes. The <dst> address can be anywhere in RAM.
*
* The calling thread will be suspended while the transfer is in progress.
* It will be woken up when the DMA completion interrupt fires. Two DMA
* channels are needed for this operation because the SPI controller only
* pulses the clock when data is transmitted, thus we must send data in
* order to prompt the slave to reply to us.
*
* In most cases, the caller is responsible for providing a buffer filled
* with 0xFF bytes which are transmitted as part of the exchange. (Usually
* the same buffer used to receive the data is re-used for this purpose.)
* To avoid having to do this, this function temporarily reprograms the
* MOSI pin as a GPIO and pulls it high. This makes the slave think the SPI
* controller is always sending 1 bits regardless of what is actually in
* the buffer. The pin is reset to MOSI mode before this function exits.
*
* RETURNS: N/A
*/

void
dmaRecv8 (const void * dst, uint32_t len)
{
	/*
	 * DMA transfers can cause the slave to overrun the receiver
	 * and result in corrupted reads unless we enable the FIFO.
	 */

	SPI1->C3 |= SPIx_C3_FIFOMODE;

	/* Set up the DMA source and destination addresses. */

	DMA->ch[0].DSR_BCR = len;
	DMA->ch[0].SAR = (uint32_t)dst;
	DMA->ch[1].DSR_BCR = len;
	DMA->ch[1].DAR = (uint32_t)dst;

	/*
	 * When performing a block read, we need to have the
	 * MOSI pin pulled high in order for the slave to
	 * send us data. This is usually accomplished by transmitting
	 * a buffer of all 1s at the same time we're receiving. But
	 * to do that, we need to do a memset() to populate the
	 * transfer buffer with 1 bits, and that wastes CPU cycles.
	 * To avoid this, we temporarily turn the MOSI pin back into
	 * a GPIO and force it high. We still need to perform a TX
	 * DMA transfer (otherwise the SPI controller won't toggle
	 * the clock pin), but the contents of the buffer don't
	 * matter: to the SD card it appears we're always sending
	 * 1 bits.
	 */

	palSetPadMode (GPIOE, 1, PAL_MODE_OUTPUT_PUSHPULL);
	palSetPad (GPIOE, 1);

	/*
	 * Start the transfer and wait for it to complete.
	 * We will be woken up by the DMA interrupt handler when
	 * the transfer completion interrupt triggers.
	 *
	 * Note: the whole block below is a critical section and
	 * must be guarded with syslock/sysunlock. We must ensure
	 * the interrupt service routine doesn't fire until
	 * osalThreadSuspendS() has saved a reference to the
	 * current thread to the dmaThread1 pointer, but we have
	 * to call osalThreadSuspendS()  after initiating the DMA
	 * transfer. There is a chance the DMA completion interrupt
	 * might trigger before dmaThread1 is updated, in which
	 * case the interrupt service routine will fail to wake
	 * us up. Initiating the transfer after calling osalSysLock()
	 * closes this race condition window: it masks off interrupts
	 * until osalThreadSuspendS() atomically puts this thread to
	 * sleep and then unmasks them again.
	 */

	osalSysLock ();

	SPI1->C2 |= SPIx_C2_RXDMAE;
	DMA->ch[1].DCR |= DMA_DCRn_ERQ;
	SPI1->C2 |= SPIx_C2_TXDMAE;
	DMA->ch[0].DCR |= DMA_DCRn_ERQ;

	osalThreadSuspendS (&dma1Thread);

	osalSysUnlock ();

	/* Release the MOSI pin. */

	palSetPadMode (GPIOE, 1, PAL_MODE_ALTERNATIVE_2);

	/* Disable SPI DMA mode and turn off the FIFO. */

	SPI1->C2 &= ~(SPIx_C2_TXDMAE|SPIx_C2_RXDMAE);
	SPI1->C3 &= ~SPIx_C3_FIFOMODE;

	return;
}

/******************************************************************************
*
* dmaSend16 - perform an 16-bit DMA transmit operation on SPI controller 1
*
* This function uses DMA channel 2 to transmit a block of data to a slave
* connected to the SPI1 controller. The data is transmitted two bytes at a
* a time. The <src> argument is a pointer to the memory buffer to be sent
* and <len> indicates the amount of data to transfer in bytes. The <src>
* address can be anywhere in RAM or flash.
*
* This function is used primarily for the display controller, which uses
* 16-bit pixel data. Note that the <len> argument must still specify the
* buffer size in bytes, not words.
*
* The calling thread will be suspended while the transfer is in progress.
* It will be woken up when the DMA completion interrupt fires. Only a
* single channel is needed for this operation. Any data returned by the
* slave is ignored.
*
* RETURNS: N/A
*/

void
dmaSend16 (const void * src, uint32_t len)
{
	/*
	 * Note: the graphics code has already enabled the
	 * FIFO by the time this function is called, so we
	 * don't need to do it here.
	 */

	DMA->ch[2].DSR_BCR = len;
	DMA->ch[2].SAR = (uint32_t)src;

	/* Initiate the transfer */

	osalSysLock ();

	SPI1->C2 |= SPIx_C2_TXDMAE;
	DMA->ch[2].DCR |= DMA_DCRn_ERQ;
	osalThreadSuspendS (&dma2Thread);

	osalSysUnlock ();
 
	SPI1->C2 &= ~SPIx_C2_TXDMAE;

	return;
}
