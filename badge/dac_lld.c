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
 * This module contains support for playing audio samples using the DAC in
 * the Freescale/NXP KW01. The DAC supports 12-bit sample values using a max
 * 3.3v reference voltage. Audio is played in conjunction with one of the
 * programmable interval timers.
 *
 * The KW01 has two programmable interval timers (PITs). To play audio, PIT1
 * is programmed to trigger at the audio sample rate. An audio sample buffer
 * is allocated to contain data read from the SD card. On each PIT interrupt,
 * the next sample in the buffer is copied to the DAC data register and the
 * buffer index is incremented. Using the PIT ensures the audio is played at
 * the correct rate without the need for manual delay loops. A thread running
 * at very low priority is responsible for keeping the sample buffer populated.
 * Other threads may cue up audio to play in the background while they perform
 * other tasks.
 *
 * Audio samples must be provided in 16-bit words containing raw 12-bit sample
 * data. Samples are generated on a host system using a small program to
 * convert from 16-bit to 12-bit raw values as most audio processing software
 * does not support 12-bit sampling mode.
 *
 * Audio files stored on the SD card have no special header or formatting.
 * The DAC thread will continue to play samples until it reaches the end of
 * a file.
 *
 * The current audio sample rate is 9216Hz. This was chosen to work well
 * in conjunction with video playback.
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

static THD_WORKING_AREA(waDacThread, 512);

/*
 * Note: dacpos and daxmax must be volatile to prevent the compiler
 * from optimizing accesses to them.
 */

uint16_t * dacBuf;

static volatile uint16_t * dacbuf = NULL;
static volatile int dacpos;
static volatile int dacmax;

static char * fname;
static thread_t * pThread = NULL;
static uint8_t play;
static uint8_t dacloop;

/******************************************************************************
*
* dacWrite - DAC sample interval interrupt handler
*
* This function is invoked from the PIT interrupt handler. The PIT1 timer is
* programmed to fire at the audio sample rate. Each time it triggers, the
* next available sample in the sample buffer is transfered to the DAC data
* register.
*
* If there are no samples available, tbis function returns with no effect.
*
* RETURNS: N/A
*/

static void
dacWrite (void)
{
	if (dacpos < dacmax) {
		DAC_WRITE_2(&DAC1, DAC0_DAT0L, dacbuf[dacpos]);
		dacpos++;
	}

	return;
}

/******************************************************************************
*
* dacThread - DAC audio player thread
*
* This function implements the DAC player thread loop. It runs continuously
* in the background and normally sleeps waiting for a request to play an
* andio file. When either dacLoopPlay() or dacPlay() is called, it will open
* the file and contiously load blocks of samples into a playback buffer which
* is consumed by the dacWrite() function above.
*
* A double-buffering scheme is used. The sample buffer is divided in half,
* and an effort is made to have the first half of the buffer playing while
* the second half is being filled with data from the SD card.
*
* RETURNS: N/A
*/

static
THD_FUNCTION(dacThread, arg)
{
	FIL f;
	UINT br;
	uint16_t * p;
	userconfig *config;
	thread_t * th;

        (void)arg;

	chRegSetThreadName ("dac");
	config = getConfig();

	while (1) {
		if (play == 0) {
			th = chMsgWait ();
			chMsgRelease (th, MSG_OK);
		}
 
		if (config->sound_enabled == 0)
			continue;

		/*
		 * If the video app is running, then it's already using
		 * the DAC, and we shouldn't try to use it again here
		 * because can't play from two sources at once.
		 */
 	
		if (dacBuf != NULL)
			continue;

		if (f_open (&f, fname, FA_READ) != FR_OK)
			continue;

		dacBuf = chHeapAlloc (NULL,
		    (DAC_SAMPLES * sizeof(uint16_t)) * 2);

		/* Load the first block of samples. */

		p = dacBuf;
		if (f_read (&f, p, DAC_BYTES, &br) != FR_OK) {
			f_close (&f);
			chHeapFree (dacBuf);
			dacBuf = NULL;
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

			if (p == dacBuf)
				p += DAC_SAMPLES;
			else
				p = dacBuf;

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

		dacpos = 0;
		dacmax = 0;
		dacbuf = NULL;
		if (play && dacloop == DAC_PLAY_ONCE) {
			fname = NULL;
			play = 0;
		}
		chHeapFree (dacBuf);
		dacBuf = NULL;
		f_close (&f);
		pitDisable (&PIT1, 1);
	}

	/* NOTREACHED */
	return;
}

/******************************************************************************
*
* dacStart - initialize the DAC module
*
* This function enables the DAC module in the KW01, allocates the sample buffer
* memory and starts the DAC player thread. It also connects the dacWrite()
* function to the PIT1 interrupt handler. PIT1 is not actually enabled until
* the DAC thread begins playing samples.
*
* RETURNS: N/A
*/

void
dacStart (DACDriver * dac)
{
	dac->dac_base = (uint8_t *)DAC_BASE;

	SIM->SCGC6 |= SIM_SCGC6_DAC0;

	DAC_WRITE_1(dac, DAC0_C0, DAC0_C0_EN | DAC0_C0_RFS);
	DAC_WRITE_2(dac, DAC0_DAT0L, 0);

	pit1Start (&PIT1, dacWrite);

	pThread = chThdCreateStatic (waDacThread, sizeof(waDacThread),
		DAC_THREAD_PRIO, dacThread, NULL);

	return;
}

/******************************************************************************
*
* dacLoopPlay - play an audio file
*
* This function cues up an audio file to be played by the background DAC
* thread. The file name is specified by <file>. If <loop> is DAC_PLAY_ONCE,
* the sample file is played once and then the player thread will go idle
* again. If <loop> is DAC_PLAY_LOOP, the same file will be played over and
* over again.
*
* Playback of any sample file (including one that is looping) can be halted
* at any time by calling dacLoopPlay() with <file> set to NULL.
*
* RETURNS: N/A
*/

void
dacLoopPlay (char * file, uint8_t loop)
{
	play = 0;
	dacloop = loop;
	fname = file;
	chMsgSend (pThread, MSG_OK);

	return;
}

/******************************************************************************
*
* dacPlay - play an audio file
*
* This is a shortcut version of dacLoopPlay() that always plays a file just
* once. As with dacLoopPlay(), playback can be halted by calling dacPlay()
* with <file> set to NULL.
*
* RETURNS: N/A
*/

void
dacPlay (char * file)
{
	dacLoopPlay (file, DAC_PLAY_ONCE);
	return;
}

/******************************************************************************
*
* dacSamplesPlay - play specific sample buffer
*
* This function can be used to play an arbitrary buffer of samples provided
* by the saller via the pointer argument <p>. The <cnt> argument indicates
* the number of samples (which should be equal to the total number of bytes
* in the buffer divided by 2, since samples are stored as 16-bit quantities).
* The PIT1 timer must first be enabled with pitEnable() in order for the
* samples to play. The timer should be disabled with pitDisable() when
* playback is complete.
*
* This function is used primarily by the video player module.
*
* RETURNS: N/A
*/

void
dacSamplesPlay (uint16_t * p, int cnt)
{
	dacbuf = p;
	dacpos = 0;
	dacmax = cnt;

	return;
}

/******************************************************************************
*
* dacWait - wait for audio sample buffer to finish playing
*
* This function can be used to test when the current block of samples has
* finished playing. It is used primarily by callers of dacSamplesPlay() to
* keep pace with the audio playback. The function continously tests the
* sample buffer position until it reaches the end of the buffer, pausing to
* sleep for one system tick each time through the loop. If the playback
* is already completed by the time this function is called, it returns
* immediately, otherwise it will block until playback completes.
*
* RETURNS: The number of ticks we had to wait until the playback finished.
*/

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
