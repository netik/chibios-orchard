/*-
 * Copyright (c) 2016-2017
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
 * This module implements support for the periodic interval timer in
 * the KW01. The PIT block has two timers tied to the same IRQ in the
 * NVIC. We both timers: one to provide delay timer support for the
 * FatFs SPI module and the other for playing audio samples through
 * the DAC.
 *
 * During early bootstrap, we also use the second PIT as an entropy
 * source to prime the random number generator. The amount of time
 * it takes the system to complete initialization varies slightly
 * due to the non-constant amount of time it takes to issue SPI
 * requests when resetting and initializing the radio, so we use
 * PIT1 to count the number of timer ticks it takes before we
 * reach the main() function. Later, PIT1 is re-initialized for
 * use by the DAC.
 */

#include "ch.h"
#include "hal.h"
#include "osal.h"

#include "pit_reg.h"
#include "pit_lld.h"

#include "dac_reg.h"
#include "dac_lld.h"

PITDriver PIT1;

#ifdef UPDATER
#include "updater.h"

extern isr vectors[];

static void pitIrq (void)
#else
OSAL_IRQ_HANDLER(KINETIS_PIT_IRQ_VECTOR)
#endif
{
	uint32_t status;

#ifndef UPDATER
	OSAL_IRQ_PROLOGUE();
#endif

	/* Acknowledge the interrupt */

	status = CSR_READ_4(&PIT1, PIT_TFLG0);

	/* Call the callout function if one is present */

	if (status & PIT_TFLG_TIF && PIT1.pit0_func != NULL) {
		(*PIT1.pit0_func)();
		CSR_WRITE_4(&PIT1, PIT_TFLG0, PIT_TFLG_TIF);
	}

#ifndef UPDATER
	status = CSR_READ_4(&PIT1, PIT_TFLG1);

	if (status & PIT_TFLG_TIF && PIT1.pit1_func != NULL) {
		(*PIT1.pit1_func)();
		CSR_WRITE_4(&PIT1, PIT_TFLG1, PIT_TFLG_TIF);
	}

	OSAL_IRQ_EPILOGUE();
#endif

	return;
}

void
pitStart (PITDriver * pit, PIT_FUNC func)
{
	pit->pit_base = (uint8_t *)PIT_BASE;
	pit->pit0_func = func;

#ifdef UPDATER
	/* The PIT may already be started, so halt it. */

	CSR_WRITE_4(pit, PIT_TCTRL0, 0);
	CSR_WRITE_4(pit, PIT_TCTRL1, 0);
	vectors[KINETIS_PIT_IRQ_VECTOR] = pitIrq;

#else
	/* Enable the clock gating for the PIT. */

	SIM->SCGC6 |= SIM_SCGC6_PIT;
#endif

	/* Enable the IRQ in the NVIC */

	nvicEnableVector (PIT_IRQn, 5);

	CSR_WRITE_4(pit, PIT_MCR, PIT_MCR_FRZ);

	/*
	 * The PIT is triggered from the bus clock, which is
	 * usually 24MHz. We want it to trigger every 10ms for
	 * the FatFs timer callback.
	 */

	CSR_WRITE_4(pit, PIT_LDVAL0, KINETIS_BUSCLK_FREQUENCY / 100);

	/* Enable timer and interrupt. */

	CSR_WRITE_4(pit, PIT_TFLG0, PIT_TFLG_TIF);
	CSR_WRITE_4(pit, PIT_TCTRL0, PIT_TCTRL_TIE|PIT_TCTRL_TEN);

	return;
}

#ifndef UPDATER
void
pit1Start (PITDriver * pit, PIT_FUNC func)
{
	pit->pit1_func = func;

	CSR_WRITE_4(pit, PIT_TCTRL1, 0);
	CSR_WRITE_4(pit, PIT_LDVAL1,
	    KINETIS_BUSCLK_FREQUENCY / DAC_SAMPLERATE);
	CSR_WRITE_4(pit, PIT_TFLG1, PIT_TFLG_TIF);
	CSR_WRITE_4(pit, PIT_TCTRL1, PIT_TCTRL_TIE|PIT_TCTRL_TEN);

	return;
}

void
pitEnable (PITDriver * pit, uint8_t pitidx)
{
	if (pitidx == 0)
		CSR_WRITE_4(pit, PIT_TCTRL0, PIT_TCTRL_TIE |
                    PIT_TCTRL_TEN);
	else if (pitidx == 1)
		CSR_WRITE_4(pit, PIT_TCTRL1, PIT_TCTRL_TIE |
                    PIT_TCTRL_TEN);

	return;
}

void
pitDisable (PITDriver * pit, uint8_t pitidx)
{
	if (pitidx == 0)
		CSR_WRITE_4(pit, PIT_TCTRL0, 0);
	else if (pitidx == 1)
		CSR_WRITE_4(pit, PIT_TCTRL1, 0);

	return;
}
#endif
