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
 * This module uses two: PTC2 form TPM0 channel 1 and PTB1 for TPM1
 * channel 1. On the Freescale/NXP KW019032 board, PTB1 is wired to
 * the green segment of the tri-color LED, but this doesn't affect
 * the pin usage for audio. 
 *
 * There is a ChibiOS PWM driver for the KW01, however its API is a little
 * clumsy to use for this application, so we use this code instead.
 *
 * We create a thread to play music in the background so that the game
 * code doesn't have to block. Music is formatted as a table of PWM_NOTE
 * structures which contain a 16-bit frequency value and a 16-bit duration
 * value. To play a tune, the pwmChanThreadPlay() function is with a pointer
 * to a tune table, and the thread will play each note in the table until
 * it reaches a note with a duration of PWM_DURATION_END. If the last
 * note has a duration of PWM_DURATION_LOOP, the same tune will play
 * over and over until pwmChanThreadPlay(NULL) is called.
 *
 * The TPM includes a pre-scaler feature which we make use of here. We
 * want to use the TPM to generate a pulse train at audio frequencies.
 * When using the 48MHz system clock as the TPM timer source, the divisor
 * values we need to use to generate audio frequencies can be fairly
 * large. This is a problem because the value field for a TPM channel,
 * which is used to set the mid-point of the pulse sycle, is only 16 bits
 * wide. To ensure the desired values will fit, we divide the master
 * clock using the pre-scaler. The default pre-scale factor is 16, which
 * allows for tones as low as 45Hz.
 */

#include "ch.h"
#include "hal.h"

#include "tpm.h"
#include "kinetis_tpm.h"
#include "userconfig.h"

static THD_WORKING_AREA(waThread0, 0);
static THD_WORKING_AREA(waThread1, 0);

typedef void (*tonestop)(void);
typedef void (*tonestart)(uint32_t);

typedef struct thread_state {
	tonestop		toneStop;
	tonestart		toneStart;
	PWM_NOTE *		pTune;
	thread_t *		pThread;
	uint8_t			play;
} THREAD_STATE;

static THREAD_STATE	pwmState0 = {
	pwmChan0ToneStop,
	pwmChan0ToneStart,
	NULL,
	NULL,
	0
};

static THREAD_STATE	pwmState1 = {
	pwmChan1ToneStop,
	pwmChan1ToneStart,
	NULL,
	NULL,
	0
};

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
* the same tune will play over and over until pwmChanThreadPlay(NULL) is called.
*
* RETURNS: N/A
*/

static THD_FUNCTION(pwmThread, arg) {
	PWM_NOTE * p;
	THREAD_STATE * pState;
	userconfig * config;

	pState = (THREAD_STATE *)arg;
	chRegSetThreadName ("pwm");
	config = getConfig();

	while (1) {
		chThdSleep (TIME_INFINITE);
		
		if (config->sound_enabled == 0) {
			pState->play = 0;
			pState->pTune = NULL;
		}
		
		while (pState->play) {
			p = pState->pTune;
			while (1) {
				if (pState->play == 0) {
					pState->toneStop ();
					pState->pTune = NULL;
					break;
				}
				if (p->pwm_duration == PWM_DURATION_END) {
					pState->toneStop ();
					if (pState->play)
						pState->play--;
					if (pState->play == 0)
						pState->pTune = NULL;
					break;
				}
				if (p->pwm_duration == PWM_DURATION_LOOP) {
					pState->toneStop ();
					break;
				}
				if (p->pwm_note == PWM_NOTE_OFF) {
					pState->toneStop ();
				} else if (p->pwm_note != PWM_NOTE_PAUSE) {
					pState->toneStart (p->pwm_note);
				}
				chThdSleepMilliseconds (p->pwm_duration);
				p++;
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

	SIM->SCGC6 |= SIM_SCGC6_TPM0 | SIM_SCGC6_TPM1;

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

	/*
	 * Initialize channel mode. We select edge-aligned PWM mode,
         * active low.
	 */

	TPM0->C[TPM_CHANNEL].SC = TPM_CnSC_MSB | TPM_CnSC_ELSB;
	TPM1->C[TPM_CHANNEL].SC = TPM_CnSC_MSB | TPM_CnSC_ELSB;

	/* Create PWM threads */

	pwmState0.pThread = chThdCreateStatic(waThread0, sizeof(waThread0),
		TPM_THREAD_PRIO, pwmThread, &pwmState0);
	pwmState1.pThread = chThdCreateStatic(waThread1, sizeof(waThread1),
		TPM_THREAD_PRIO, pwmThread, &pwmState1);

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

void pwmChan0ToneStart (uint32_t tone)
{
	/* Disable timer */

	TPM0->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);

	/* Set modulus and pulse width */

	TPM0->MOD = TPM_FREQ / tone;
	TPM0->C[TPM_CHANNEL].V = (TPM_FREQ / tone) / 2;

	/* Turn on timer */

	TPM0->SC |= TPM_SC_CMOD_LPTPM_CLK;

	return;
}

/******************************************************************************
*
* pwmChan1ToneStart - start playing a tone on voice 1
*
* This function is the same as pwmChan0ToneStart(), except it works on
* the second voice channel. This voice 1 chan be set to play a different
* tone, completely independent of voice 0.
*
* RETURNS: N/A
*/

void pwmChan1ToneStart (uint32_t tone)
{
	/* Disable timer */

	TPM1->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);

	/* Set modulus and pulse width */

	TPM1->MOD = TPM_FREQ / tone;
	TPM1->C[TPM_CHANNEL].V = (TPM_FREQ / tone) / 2;

	/* Turn on timer */

	TPM1->SC |= TPM_SC_CMOD_LPTPM_CLK;

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
	/* Turn off timer and clear modulus and pulse width */
 
	TPM0->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);
	TPM0->C[TPM_CHANNEL].V = 0;
	TPM0->MOD = 0;

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
	/* Turn off timer and clear modulus and pulse width */
 
	TPM1->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);
	TPM1->C[TPM_CHANNEL].V = 0;
	TPM1->MOD = 0;

	return;
}

/******************************************************************************
*
* pwmChanThreadPlay - Play a tune using the background music thread.
*
* This function wakes up the background music thread to play an array of
* notes that form a tune. The tune is formatted as an array of PWM_NOTE
* structures. The <p> argument is a pointer to the array. Each PWM_NOTE
* contains a frequency (pwm_note) and duration (pwm_duration). There
* are two available channels and two music playing threads. A channel
* can be specified using the <chan> argument which can be either PWM_CHAN_0
* or PWM_CHAN_1. The music thread will program the PWM to play the note and
* then pause for the duration period before moving on to the next note.
* The following special cases are handled:
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
* Calling pwmChanThreadPlay() with a <p> pointer of NULL has no effect,
* except to cease the playing of a tune that is looping.
*
* Calling pwmChanThreadPlay() with a new tune pointer while a tune is already
* playing, the new tune will be played after the current tune completes
* (unless the current tune is set to loop, in which case
* pwmChanThreadPlay (NULL, <chan>) must be called to cancel it first.
*
* If pwmChanThreadPlay() is called more than once with a non-NULL tune pointer
* while a tune is playing, only the tune specified in the last call will
* be played (tunes are not queued).
*
* RETURNS: N/A
*/

void pwmChanThreadPlay (const PWM_NOTE * p, uint8_t chan)
{
	THREAD_STATE * pState;
	thread_t * t;

	if (chan == PWM_CHAN_0)
		pState = &pwmState0;
	else if (chan == PWM_CHAN_1)
		pState = &pwmState1;

	/*
	 * Cancel any currently playing tune and wait for the
	 * player thread to go idle. We have to do this otherwise
	 * our next command to play a tune might be ignored.
	 */

	if (p == NULL) {
		pState->play = 0;
		while (pState->pTune != NULL)
			chThdSleepMicroseconds (10);
		return;
	}

	pState->pTune = (PWM_NOTE *)p;
	if (pState->play == 0) {
		pState->play = 1;
		t = pState->pThread;
		chThdResume (&t, MSG_OK);
	} else
		pState->play = 2;

	return;
}
