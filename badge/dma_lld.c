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

#include "ch.h"
#include "hal.h"
#include "osal.h"

#include "dma_lld.h"

thread_reference_t dma0Thread;
thread_reference_t dma1Thread;
thread_reference_t dma2Thread;

static void
dmaIntHandle (uint8_t chan, thread_reference_t * thd)
{
	if (DMA->ch[chan].DSR_BCR & DMA_DSR_BCRn_DONE) {

		/* Wake any thread waiting for this DMA transfer to finish */

		osalSysLockFromISR ();
		osalThreadResumeI (thd, MSG_OK);
		osalSysUnlockFromISR ();                          

		/* Ack the DMA completion interrupt */

		DMA->ch[chan].DSR_BCR |= DMA_DSR_BCRn_DONE;
	}

	return;
}

OSAL_IRQ_HANDLER(KINETIS_DMA0_IRQ_VECTOR)
{
	OSAL_IRQ_PROLOGUE();
	dmaIntHandle (0, &dma0Thread);
	OSAL_IRQ_EPILOGUE();
	return;
}

OSAL_IRQ_HANDLER(KINETIS_DMA1_IRQ_VECTOR)
{
	OSAL_IRQ_PROLOGUE();
	dmaIntHandle (1, &dma1Thread);
	OSAL_IRQ_EPILOGUE();
	return;
}

OSAL_IRQ_HANDLER(KINETIS_DMA2_IRQ_VECTOR)
{
	OSAL_IRQ_PROLOGUE();
	dmaIntHandle (2, &dma2Thread);
	OSAL_IRQ_EPILOGUE();
	return;
}

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

void
dmaSend8 (const void * src, uint32_t len)
{
	SPI1->C3 |= SPIx_C3_FIFOMODE;

	DMA->ch[0].DSR_BCR = len;
	DMA->ch[0].SAR = (uint32_t)src;

	osalSysLock ();

	SPI1->C2 |= SPIx_C2_TXDMAE;
	DMA->ch[0].DCR |= DMA_DCRn_ERQ;
	osalThreadSuspendS (&dma0Thread);

	osalSysUnlock ();
 
	SPI1->C2 &= ~SPIx_C2_TXDMAE;
	SPI1->C3 &= ~SPIx_C3_FIFOMODE;

	return;
}

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

	osalSysLock ();

	SPI1->C2 |= SPIx_C2_TXDMAE;
	DMA->ch[2].DCR |= DMA_DCRn_ERQ;
	osalThreadSuspendS (&dma2Thread);

	osalSysUnlock ();
 
	SPI1->C2 &= ~SPIx_C2_TXDMAE;

	return;
}
