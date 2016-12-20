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
 * This module implements support for using the KW01 TPM module as a
 * tone generator. This simplifies the generation of music notes as
 * the hardware can be set to generate a square wave without having
 * jiggle GPIO pins in software. There are several pins available for TPM.
 * This module uses three: PTC2 form TPM0 channel 1 and PTB1 for TPM1
 * channel 1 and PTB2 for TPM2 channel 0. On the Freescale/NXP KW019032
 * board, PTB1 is wired to the green segment of the tri-color LED, but
 * this doesn't affect the pin usage for audio. Also PTB2 is documented
 * as being used for UART0 flow control, however it turns out that it's
 * not being for that purpose by ChibiOS. (It doesn't look like the
 * Kinetis UART driver in ChibiOS supports hardware flow control.)
 *
 * There is a ChibiOS PWM driver for the KW01, however its API is a little
 * clumsy to use for this application, so we use this code instead.
 *
 * We create a thread to play music in the background so that the game
 * code doesn't have to block. Music is formatted as a table of PWM_NOTE
 * structures which contain a 8-bit MIDI note value and a 8-bit duration
 * value. The duration is specified in milliseconds, but divided by 8
 * so that the value will more easily fit into a single byte.
 *
 * Three voices are supported using the three TPM channels. To play a
 * tune, the pwmChanThreadPlay() function is called with pointers to up
 * three tune tables, and the thread will play each note in the tables until
 * it reaches a note with a duration of PWM_DURATION_END. If the last
 * note has a duration of PWM_DURATION_LOOP, the same tune will play
 * over and over until pwmChanThreadPlay(NULL, NULL, NULL) is called.
 *
 * The TPM includes a pre-scaler feature which we make use of here. We
 * want to use the TPM to generate a pulse train at audio frequencies.
 * When using the 48MHz system clock as the TPM timer source, the divisor
 * values we need to use to generate audio frequencies can be fairly
 * large. This is a problem because the value field for a TPM channel,
 * which is used to set the mid-point of the pulse sycle, is only 16 bits
 * wide. To ensure the desired values will fit, we divide the master
 * clock using the pre-scaler. The default pre-scale factor is 32, which
 * allows for tones as low as 22Hz.
 */

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "tpm.h"
#include "kinetis_tpm.h"
#include "userconfig.h"

#include "ff.h"
#include "ffconf.h"

#include <string.h>

extern mutex_t enemies_mutex;

/*
 * This table represents the 127 available MIDI note frequencies.
 * Using this table allows us to specify nots using a single byte.
 * The note value is converted to a frequency in the tone start
 * routines.
 */

static const uint16_t notes[128] = {
	8,	9,	9,	10,	10,	11,	12,	12,
	13,	14,	15,	15,	16,	17,	18,	19,
	21,	22,	23,	24,	26,	28,	29,	31,
	33,	35,	37,	39,	41,	44,	46,	49,
	52,	55,	58,	62,	65,	69,	73,	78,
	82,	87,	92,	98,	104,	110,	117,	123,
	131,	139,	147,	156,	165,	175,	185,	196,
	208,	220,	233,	247,	262,	277,	294,	311,
	330,	349,	370,	392,	415,	440,	466,	494,
	523,	554,	587,	622,	659,	698,	740,	784,
	831,	880,	932,	988,	1047,	1109,	1175,	1245,
	1319,	1397,	1480,	1568,	1661,	1760,	1865,	1976,
	2093,	2217,	2349,	2489,	2637,	2794,	2960,	3136,
	3322,	3520,	3729,	3951,	4186,	4435,	4699,	4978,
	5274,	5588,	5920,	6272,	6645,	7040,	7459,	7902,
	8372,	8870,	9397,	9956,	10548,	11175,	11840,	12544
};

static THD_WORKING_AREA(waThread0, 144);

typedef struct thread_state {
	PWM_NOTE *		pTune0;
	PWM_NOTE *		pTune1;
	PWM_NOTE *		pTune2;
	thread_t *		pThread;
	char *			pName;
	uint8_t			play;
} THREAD_STATE;

static char fname[13];
static 	FIL f[3];

static THREAD_STATE pwmState0 = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0
};

static PWM_NOTE buf0[PWM_BUFSIZ];
static PWM_NOTE buf1[PWM_BUFSIZ];
static PWM_NOTE buf2[PWM_BUFSIZ];

static const PWM_NOTE dummy = { PWM_NOTE_OFF, PWM_DURATION_END };

static uint8_t pwmSmallest (uint8_t * d)
{
	uint8_t i;
	uint8_t smallest = PWM_DURATION_END;

	for (i = 0; i < 3; i++) {
		if (d[i] < smallest)
			smallest = d[i];
	}

	return (smallest);
}

/******************************************************************************
*
* pwmThread - background tone generator thread
*
* This function implements a background thread for playing notes through
* the pulse width modulator. When music is not playing, the thread sleeps.
* It can be woken up by pwmChanThreadPlay(). When awakened, pTune0 should point
* to a table of PWM_NOTE structures which indicate the frequency and duration
* for each note in the tune. The last note in the table should have a
* duration of PWM_DURATION_END. If the duration is PWM_DURATION_LOOP, then
* the same tune will play over and over until pwmChanThreadPlay(NULL) is
* called.
*
* RETURNS: N/A
*/

static THD_FUNCTION(pwmThread, arg) {
	PWM_NOTE * p[3];
	uint8_t d[3];
	uint8_t smallest;
	THREAD_STATE * pState;
	userconfig * config;

	pState = (THREAD_STATE *)arg;
	chRegSetThreadName ("pwm");
	config = getConfig();
	UINT br;

	buf0[PWM_BUFSIZ - 1].pwm_note = PWM_NOTE_BUFEND;
	buf1[PWM_BUFSIZ - 1].pwm_note = PWM_NOTE_BUFEND;
	buf2[PWM_BUFSIZ - 1].pwm_note = PWM_NOTE_BUFEND;

	while (1) {
		chThdSleep (TIME_INFINITE);
		
		if (config->sound_enabled == 0) {
			pState->play = 0;
			pState->pTune0 = NULL;
			pState->pTune1 = NULL;
			pState->pTune2 = NULL;
			pState->pName = NULL;
		}
		
		while (pState->play) {

			/*
			 * If we've been asked to play a file, try to
			 * open all the tracks.
			 */

			if (pState->pName != NULL) {
				chsnprintf (fname, sizeof(fname), "%s0.bin",
				    pState->pName);
				if (f_open (&f[0], fname, FA_READ) == FR_OK)
					pState->pTune0 = &buf0[PWM_BUFSIZ - 1];
				else
					pState->pTune0 = (PWM_NOTE *)&dummy;
				fname[strlen(pState->pName)] = '1';
				if (f_open (&f[1], fname, FA_READ) == FR_OK)
					pState->pTune1 = &buf1[PWM_BUFSIZ - 1];
				else
					pState->pTune1 = (PWM_NOTE *)&dummy;
				fname[strlen(pState->pName)] = '2';
				if (f_open (&f[2], fname, FA_READ) == FR_OK)
					pState->pTune2 = &buf2[PWM_BUFSIZ - 1];
				else
					pState->pTune2 = (PWM_NOTE *)&dummy;
			}

			p[0] = pState->pTune0;
			p[1] = pState->pTune1;
			p[2] = pState->pTune2;

			d[0] = p[0]->pwm_duration;
			d[1] = p[1]->pwm_duration;
			d[2] = p[2]->pwm_duration;

			while (1) {
				if (pState->play == 0) {
					pwmChan0ToneStop ();
					pwmChan1ToneStop ();
					pwmChan2ToneStop ();
					break;
				}

				if (p[0]->pwm_note == PWM_NOTE_BUFEND) {
					f_read (&f[0], buf0,
					    sizeof(PWM_NOTE) *
					    (PWM_BUFSIZ - 1), &br);
					p[0] = buf0;
					d[0] = p[0]->pwm_duration;
				}

				if (p[1]->pwm_note == PWM_NOTE_BUFEND) {
					f_read (&f[1], buf1,
					    sizeof(PWM_NOTE) *
					    (PWM_BUFSIZ - 1), &br);
					p[1] = buf1;
					d[1] = p[1]->pwm_duration;
				}

				if (p[2]->pwm_note == PWM_NOTE_BUFEND) {
					f_read (&f[2], buf2,
					    sizeof(PWM_NOTE) *
					    (PWM_BUFSIZ - 1), &br);
					p[2] = buf2;
					d[2] = p[2]->pwm_duration;
				}

				if (p[0]->pwm_duration == PWM_DURATION_END)
					pwmChan0ToneStop ();

				if (p[1]->pwm_duration == PWM_DURATION_END)
					pwmChan1ToneStop ();

				if (p[2]->pwm_duration == PWM_DURATION_END)
					pwmChan2ToneStop ();

				if (d[0] == PWM_DURATION_END &&
				    d[1] == PWM_DURATION_END &&
				    d[2] == PWM_DURATION_END) {
					if (pState->play)
						pState->play--;
					break;
				}

				if (p[0]->pwm_duration == PWM_DURATION_LOOP) {
					pwmChan0ToneStop ();
					break;
				}

				if (p[1]->pwm_duration == PWM_DURATION_LOOP) {
					pwmChan0ToneStop ();
					break;
				}

				if (p[2]->pwm_duration == PWM_DURATION_LOOP) {
					pwmChan0ToneStop ();
					break;
				}

				if (p[0]->pwm_note == PWM_NOTE_OFF) {
					pwmChan0ToneStop ();
				} else if (p[0]->pwm_note != PWM_NOTE_PAUSE) {
					pwmChan0ToneStart (p[0]->pwm_note);
				}

				if (p[1]->pwm_note == PWM_NOTE_OFF) {
					pwmChan1ToneStop ();
				} else if (p[1]->pwm_note != PWM_NOTE_PAUSE) {
					pwmChan1ToneStart (p[1]->pwm_note);
				}

				if (p[2]->pwm_note == PWM_NOTE_OFF) {
					pwmChan2ToneStop ();
				} else if (p[2]->pwm_note != PWM_NOTE_PAUSE) {
					pwmChan2ToneStart (p[2]->pwm_note);
				}

				smallest = pwmSmallest (d);
				if (d[0] != PWM_DURATION_END)
					d[0] -= smallest;
				if (d[1] != PWM_DURATION_END)
					d[1] -= smallest;
				if (d[2] != PWM_DURATION_END)
					d[2] -= smallest;

				chThdSleepMilliseconds (smallest * 8);

				if (d[0] == 0) {
					p[0]++;
					d[0] = p[0]->pwm_duration;
				}
				if (d[1] == 0) {
					p[1]++;
					d[1] = p[1]->pwm_duration;
				}
				if (d[2] == 0) {
					p[2]++;
					d[2] = p[2]->pwm_duration;
				}
			}

			/* Done playing song from a file, close the tracks. */

			if (pState->pTune0 != (PWM_NOTE *)&dummy)
				f_close (&f[0]);
			if (pState->pTune1 != (PWM_NOTE *)&dummy)
				f_close (&f[1]);
			if (pState->pTune2 != (PWM_NOTE *)&dummy)
				f_close (&f[2]);

			if (pState->play == 0) {
				pState->pTune0 = NULL;
				pState->pTune1 = NULL;
				pState->pTune2 = NULL;
				pState->pName = NULL;
			}
		}
	}

	/* NOTREACHED */
	return;
}

/******************************************************************************
*
* pwmInit - initialize the PWM module and thrad
*
* This function intializes the TPM hardware and starts the music player
* thread. We enable clock gating for TPM0 and set its clock source to
* the system clock. We also program the TPM prescaler to divide the
* system clock by a pre-defined factor (currently 8). This is done because
* the TPM channel value field is only 16 bits wide. With a system clock of
* 48MHz, the desired channel value for lower notes would be too large to
* fit in 16 bits. The timer is initialized for edge-aligned PWM mode,
* active low. The active low setting is used so that the output pin
* goes low when no tone is playing.
*
* RETURNS: N/A
*/
 
void pwmInit (void)
{
	int i;
	uint32_t psc;

	/*
         * Initialize the TPM clock source. We use the
	 * system clock, which is running at 48Mhz. We can
	 * use the TPM prescaler to adjust it later.
	 */

	SIM->SOPT2 &= ~(SIM_SOPT2_TPMSRC_MASK);
	SIM->SOPT2 |= SIM_SOPT2_TPMSRC(KINETIS_TPM_CLOCK_SRC);

	/* Enable clock gating for TPM0 */

	SIM->SCGC6 |= SIM_SCGC6_TPM0 | SIM_SCGC6_TPM1 | SIM_SCGC6_TPM2;

	/* Set prescaler factor. */

	psc = KINETIS_SYSCLK_FREQUENCY / TPM_FREQ;

	for (i = 0; i < 8; i++) {
		if (psc == (1UL << i))
			break;
	}

	/* Check for invalid prescaler factor */

	if (i == 8)
		return;

	/* Set prescaler field. */

	TPM0->SC = i;
	TPM1->SC = i;
	TPM2->SC = i;

	/*
	 * Initialize channel mode. We select edge-aligned PWM mode,
         * active low.
	 */

	TPM0->C[TPM0_CHANNEL].SC = TPM_CnSC_MSB | TPM_CnSC_ELSB;
	TPM1->C[TPM1_CHANNEL].SC = TPM_CnSC_MSB | TPM_CnSC_ELSB;
	TPM2->C[TPM2_CHANNEL].SC = TPM_CnSC_MSB | TPM_CnSC_ELSB;

	/* Create PWM thread */

	pwmState0.pThread = chThdCreateStatic(waThread0, sizeof(waThread0),
		TPM_THREAD_PRIO, pwmThread, &pwmState0);

	return;
}

/******************************************************************************
*
* pwmChan0ToneStart - start playing a tone on voice 0
*
* This function programs the TPM channel for a desired tone frequency and
* enables the timer. This produces a square wave at the desired tone on
* the output pin, The <tone> argument is the tone frequency in Hz. The
* tone may be changed by calling pwmChan0ToneStart() again with a different
* frequency. The tone will continue to play until pwmChan0ToneStop() is called
* to turn off the timer.
*
* RETURNS: N/A
*/

void pwmChan0ToneStart (uint8_t tone)
{
	uint16_t		f;

	f = notes[tone];

	/* Disable timer */

	TPM0->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);

	/* Set modulus and pulse width */

	TPM0->MOD = TPM_FREQ / f;
	TPM0->C[TPM0_CHANNEL].V = (TPM_FREQ / f) / 2;
	TPM0->CNT = 0;

	/* Turn on timer */

	TPM0->SC |= TPM_SC_CMOD_LPTPM_CLK;

	return;
}

/******************************************************************************
*
* pwmChan1ToneStart - start playing a tone on voice 1
*
* This function is the same as pwmChan0ToneStart(), except it works on
* the second voice channel. This voice 1 channel be set to play a different
* tone, completely independent of the others.
*
* RETURNS: N/A
*/

void pwmChan1ToneStart (uint8_t tone)
{
	uint16_t		f;

	f = notes[tone];

	/* Disable timer */

	TPM1->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);

	/* Set modulus and pulse width */

	TPM1->MOD = TPM_FREQ / f;
	TPM1->C[TPM1_CHANNEL].V = (TPM_FREQ / f) / 2;
	TPM1->CNT = 0;

	/* Turn on timer */

	TPM1->SC |= TPM_SC_CMOD_LPTPM_CLK;

	return;
}

/******************************************************************************
*
* pwmChan2ToneStart - start playing a tone on voice 2
*
* This function is the same as pwmChan0ToneStart(), except it works on
* the third voice channel. The voice 2 channel be set to play a different
* tone, completely independent the others.
*
* RETURNS: N/A
*/

void pwmChan2ToneStart (uint8_t tone)
{
	uint16_t		f;

	f = notes[tone];

	/* Disable timer */

	TPM2->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);

	/* Set modulus and pulse width */

	TPM2->MOD = TPM_FREQ / f;
	TPM2->C[TPM2_CHANNEL].V = (TPM_FREQ / f) / 2;
	TPM2->CNT = 0;

	/* Turn on timer */

	TPM2->SC |= TPM_SC_CMOD_LPTPM_CLK;

	return;
}

/******************************************************************************
*
* pwmChan0ToneStop - Turn off TPM voice 0
*
* This function disables the TPM channel. If a tone is currently playing,
* calling pwmChan0ToneStop() will cause it to stop. This function may be called
* even if the TPM is already stopped, thought his has no effect.
*
* RETURNS: N/A
*/

void pwmChan0ToneStop (void)
{
	/* Turn off timer and clear count */
 
	TPM0->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);
	TPM0->CNT = 0;

	return;
}

/******************************************************************************
*
* pwmChan1ToneStop - Turn off TPM voice 1
*
* This function is the same as pwmChan0ToneStop(), except it works on
* the second voice channel.
*
* RETURNS: N/A
*/

void pwmChan1ToneStop (void)
{
	/* Turn off timer and clear count */

	TPM1->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);
	TPM1->CNT = 0;

	return;
}

/******************************************************************************
*
* pwmChan2ToneStop - Turn off TPM voice 2
*
* This function is the same as pwmChan0ToneStop(), except it works on
* the third voice channel.
*
* RETURNS: N/A
*/

void pwmChan2ToneStop (void)
{
	/* Turn off timer and clear count */
 
	TPM2->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);
	TPM2->CNT = 0;

	return;
}

/******************************************************************************
*
* pwmChanThreadPlay - Play a tune using the background music thread.
*
* This function wakes up the background music thread to play an array of
* notes that form a tune. The tune is formatted as an array of PWM_NOTE
* structures. Each PWM_NOTE contains a MIDI note (pwm_note) and duration
* (pwm_duration).
*
* There are three available sound channels. Each channel can be set to
* play a different tune simultaneously. The tracks are specified using
* the <p0>, <p1> and <p2> arguments, which are pointers to tune arrays.
* If any of the three pointers are NULL, then that channel will remain
* silent during playback. This allows tunes with 1, 2 or 3 voices to
* be played.
*
* A currently playing tune may be halted by calling pwmChanThreadPlay()
* with all three tune pointers set to NULL.
*
* - If pwm_duration is PWM_DURATION_END, this indicates the end of the
*   tune. The thread will go to sleep when it reaches the end of the tune
*   and wait for pwmChanThreadPlay() to be called again. The value of pwm_note
*   is ignored.
*
* - If pwm_duration is PWM_DURATION_LOOP, then the thread will re-play
*   the tune over and over again. To stop the loop from playing,
*   pwmChanThreadPlay() must be called with a NULL pointer.
*
* - If pwm_note is PWM_NOTE_PAUSE, then the TPM will be left in its
*   current state and simply pause for the duration period.
*
* - If pwm_note is PWM_NOTE_NONE, then the TPM will be turned off and
*   the thread will pause for the duration period. This can be used to
*   create a silent pause period.
*
* Calling pwmChanThreadPlay() with a new tune pointer while a tune is already
* playing, the new tune will be played after the current tune completes
* (unless the current tune is set to loop, in which case
* pwmChanThreadPlay (NULL, NULL, NULL) must be called to cancel it first.
*
* If pwmChanThreadPlay() is called more than once with a non-NULL tune pointer
* while a tune is playing, only the tune specified in the last call will
* be played (tunes are not queued).
*
* RETURNS: N/A
*/

void pwmChanThreadPlay (const PWM_NOTE * p0, const PWM_NOTE * p1,
	const PWM_NOTE * p2)
{
	THREAD_STATE * pState;
	thread_t * t;

	pState = &pwmState0;

	/*
	 * Cancel any currently playing tune and wait for the
	 * player thread to go idle. We have to do this otherwise
	 * our next command to play a tune might be ignored.
	 */

	if (p0 == NULL && p1 == NULL && p2 == NULL) {
		pState->play = 0;
		while (pState->pTune0 != NULL &&
		    pState->pTune1 != NULL &&
		    pState->pTune2 != NULL)
			chThdSleepMicroseconds (10);
		return;
	}

	if (p0 == NULL)
		pState->pTune0 = (PWM_NOTE *)&dummy;
	else
		pState->pTune0 = (PWM_NOTE *)p0;
	if (p1 == NULL)
		pState->pTune1 = (PWM_NOTE *)&dummy;
	else
		pState->pTune1 = (PWM_NOTE *)p1;
	if (p2 == NULL)
		pState->pTune2 = (PWM_NOTE *)&dummy;
	else
		pState->pTune2 = (PWM_NOTE *)p2;
	if (pState->play == 0) {
		pState->play = 1;
		t = pState->pThread;
		chThdResume (&t, MSG_OK);
	} else
		pState->play = 2;

	return;
}

/******************************************************************************
*
* pwmFileThreadPlay - Play a tune from the filesystem
*
* This function is similar to pwmChanThreadPlay(), except it plays music from
* fimes stored in the SD card. The <pName> argument is the base name of a
* set of MIDI track files stored on the SD card. For example, if there are
* three tracks for a song called song0.bin, song1.bin and song2.bin, then
* <pName> should just be "song" and the player thread will fill in the
* rest of the file names automatically.
*
* The advantate to using this function is that allows for tunes of any length
* without needing any additional space in flash.
*
* As with pwmChanThreadPlay(), songs played with pwmFileThreadPlay() can be
* cancelled by calling pwmFileThreadPlay() with <pName> set to NULL.
*
* RETURNS: N/A
*/

void pwmFileThreadPlay (char * pName)
{
	THREAD_STATE * pState;
	thread_t * t;

	pState = &pwmState0;

	/*
	 * Cancel any currently playing tune and wait for the
	 * player thread to go idle. We have to do this otherwise
	 * our next command to play a tune might be ignored.
	 */

	if (pName == NULL) {
		pState->play = 0;
		while (pState->pName != NULL)
			chThdSleepMicroseconds (10);
		return;
	}

	pState->pName = pName;
	if (pState->play == 0) {
		pState->play = 1;
		t = pState->pThread;
		chThdResume (&t, MSG_OK);
	} else
		pState->play = 2;

	return;
}
