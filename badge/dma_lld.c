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

OSAL_IRQ_HANDLER(KINETIS_DMA0_IRQ_VECTOR)
{
	OSAL_IRQ_PROLOGUE();

	if (DMA->ch[0].DSR_BCR & DMA_DSR_BCRn_DONE) {

		/* Wake any thread waiting for this DMA transfer to finish */

		osalSysLockFromISR ();
		osalThreadResumeI (&dma0Thread, MSG_OK);
		osalSysUnlockFromISR ();                          

		/* Ack the DMA completion interrupt */

		DMA->ch[0].DSR_BCR |= DMA_DSR_BCRn_DONE;
	}

	OSAL_IRQ_EPILOGUE();

	return;
}

OSAL_IRQ_HANDLER(KINETIS_DMA1_IRQ_VECTOR)
{
	OSAL_IRQ_PROLOGUE();

	if (DMA->ch[1].DSR_BCR & DMA_DSR_BCRn_DONE) {

		/* Wake any thread waiting for this DMA transfer to finish */

		osalSysLockFromISR ();
		osalThreadResumeI (&dma1Thread, MSG_OK);
		osalSysUnlockFromISR ();                          

		/* Ack the DMA completion interrupt */

		DMA->ch[1].DSR_BCR |= DMA_DSR_BCRn_DONE;
	}

	OSAL_IRQ_EPILOGUE();

	return;
}

OSAL_IRQ_HANDLER(KINETIS_DMA2_IRQ_VECTOR)
{
	OSAL_IRQ_PROLOGUE();

	if (DMA->ch[2].DSR_BCR & DMA_DSR_BCRn_DONE) {

		/* Wake any thread waiting for this DMA transfer to finish */

		osalSysLockFromISR ();
		osalThreadResumeI (&dma2Thread, MSG_OK);
		osalSysUnlockFromISR ();                          

		/* Ack the DMA completion interrupt */

		DMA->ch[2].DSR_BCR |= DMA_DSR_BCRn_DONE;
	}

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
	    DMA_DCRn_SINC | DMA_DCRn_CS;
	DMA->ch[0].DAR = (uint32_t)&(SPI1->DL);

	/* Initialize channel 1 for SPI RX */

	DMAMUX->CHCFG[1] = DMAMUX_CHCFGn_ENBL |
	    DMAMUX_CHCFGn_SOURCE(DMA_CHAN_SPI1_RX);

	DMA->ch[1].DCR = DMA_DCRn_EINT | DMA_DCRn_D_REQ |
	    DMA_DCRn_SSIZE(1) | DMA_DCRn_DSIZE(1) |
	    DMA_DCRn_DINC | DMA_DCRn_CS;
	DMA->ch[1].SAR = (uint32_t)&(SPI1->DL);

	/* Initialize channel 2 for SPI TX with 16-bit transfers */

	DMAMUX->CHCFG[2] = DMAMUX_CHCFGn_ENBL |
	    DMAMUX_CHCFGn_SOURCE(DMA_CHAN_SPI1_TX);

	DMA->ch[2].DCR = DMA_DCRn_EINT | DMA_DCRn_D_REQ |
	    DMA_DCRn_SSIZE(2) | DMA_DCRn_DSIZE(2) |
	    DMA_DCRn_SINC | DMA_DCRn_CS;
	DMA->ch[2].DAR = (uint32_t)&(SPI1->DL);

	/* Enable IRQ vectors */

	nvicEnableVector (DMA0_IRQn, 5);
	nvicEnableVector (DMA1_IRQn, 5);
	nvicEnableVector (DMA2_IRQn, 5);

	return;
}
