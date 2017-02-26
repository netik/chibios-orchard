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

#include "ch.h"
#include "hal.h"
#include "osal.h"

#include "userconfig.h"

#include "dac_lld.h"
#include "dac_reg.h"

#include "pit_lld.h"
#include "pit_reg.h"

#include "ffconf.h"
#include "ff.h"

DACDriver DAC1;

static THD_WORKING_AREA(waDacThread, 512 /*1024*/);

/*
 * Note: dacpos and daxmax must be volatile to prevent the compiler
 * from optimizing accesses to them.
 */

static volatile uint16_t * dacbuf = NULL;
static volatile int dacpos;
static volatile int dacmax;

static char * fname;
static thread_t * pThread = NULL;
static uint8_t play;

static void
dacWrite (void)
{
	if (dacpos < dacmax) {
		DAC_WRITE_2(&DAC1, DAC0_DAT0L, dacbuf[dacpos]);
		dacpos++;
	}

	return;
}

static
THD_FUNCTION(dacThread, arg)
{
	FIL f;
        UINT br;
	uint16_t * p;
	uint16_t * buf = NULL;
	userconfig *config;
	thread_t * th;

        (void)arg;

	chRegSetThreadName ("dac");
	config = getConfig();

	buf = chHeapAlloc (NULL, (DAC_SAMPLES * sizeof(uint16_t)) * 2);

	while (1) {
		th = chMsgWait ();
		chMsgRelease (th, MSG_OK);
 
		if (config->sound_enabled == 0)
			continue;

		if (f_open (&f, fname, FA_READ) != FR_OK)
			continue;

		/* Load the first block of samples. */

		p = buf;
		if (f_read (&f, p, DAC_BYTES, &br) != FR_OK) {
			f_close (&f);
			continue;
		}

		play = 1;
		pitEnable (&PIT1, 1);

		while (1) {

			/* Start the samples playing */

			dacbuf = p;
			dacpos = 0;
			dacmax = br / 2;

			/* Swap buffers and load the next block of samples */

			if (p == buf)
				p += DAC_SAMPLES;
			else
				p = buf;

			if (f_read (&f, p, DAC_BYTES, &br) != FR_OK)
				break;

			/*
			 * Wait until the current block of samples
			 * finishes playing before playing the next
			 * block.
			 */

			while (dacpos != dacmax)
				;

			/* If we read 0 bytes, we reached end of file. */

			if (br == 0)
				break;

			/* If told to stop, exit the loop. */

			if (play == 0)
				break;
		}

		/* We're done, close the file. */

		play = 0;
		f_close (&f);
		pitDisable (&PIT1, 1);
	}

	/* NOTREACHED */
	return;
}

void
dacStart (DACDriver * dac)
{
	dac->dac_base = (uint8_t *)DAC_BASE;

	SIM->SCGC6 |= SIM_SCGC6_DAC0;

	DAC_WRITE_1(dac, DAC0_C0, DAC0_C0_EN | DAC0_C0_RFS);
	DAC_WRITE_2(dac, DAC0_DAT0L, 0);

	pit1Start (&PIT1, dacWrite);

	if (pThread == NULL) {
		pThread = chThdCreateStatic (waDacThread, sizeof(waDacThread),
			DAC_THREAD_PRIO, dacThread, NULL);
	}

	return;
}

void
dacPlay (char * file)
{
	play = 0;
	fname = file;
	chMsgSend (pThread, MSG_OK);

	return;
}

void
dacSamplesPlay (uint16_t * p, int cnt)
{
	dacbuf = p;
	dacpos = 0;
	dacmax = cnt;

	return;
}

int
dacWait (void)
{
	int waits = 0;

	while (dacpos != dacmax) {
		chThdSleep (1);
		waits++;
	}

	return (waits);
}
