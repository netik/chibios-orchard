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
 * jiggle GPIO pins in software. There are two pins available for TPM:
 * output: PTC1 for TPM0 channel 0 and PTC2 for TPM0 channel 1. On the
 * Freescale/NXP KW019032 board, PTC1 is already used for the RTC crystal,
 * we use use TPM0 channel 1 and PTC2 here.
 *
 * There is a ChibiOS PWM driver for the KW01, however its API is a little
 * clumsy to use for this application, so we use this code instead.
 *
 * We create a thread to play music in the background so that the game
 * code doesn't have to block. Music is formatted as a table of PWM_NOTE
 * structures which contain a 16-bit frequency value and a 16-bit duration
 * value. To play a tune, the pwmThreadPlay() function is with a pointer
 * to a tune table, and the thread will play each note in the table until
 * it reaches a note with a duration of PWM_DURATION_END. If the last
 * note has a duration of PWM_DURATION_LOOP, the same tune will play
 * over and over until pwmThreadPlay(NULL) is called.
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

static THD_WORKING_AREA(waPwmThread, 0);

static PWM_NOTE * pTune;
static uint8_t play;
static thread_t * pThread;

/******************************************************************************
*
* pwmThread - background tone generator thread
*
* This function implements a background thread for playing notes through
* the pulse width modulator. When music is not playing, the thread sleeps.
* It can be woken up by pwmThreadPlay(). When awakened, pTune should point
* to a table of PWM_NOTE structures which indicate the frequency and duration
* for each note in the tune. The last note in the table should have a
* duration of PWM_DURATION_END. If the duration is PWM_DURATION_LOOP, then
* the same tune will play over and over until pwmThreadPlay(NULL) is called.
*
* RETURNS: N/A
*/

static THD_FUNCTION(pwmThread, arg) {
	PWM_NOTE * p;
	(void)arg;

	chRegSetThreadName ("pwm");

	while (1) {
		chThdSleep (TIME_INFINITE);
		userconfig *config = getConfig();
		
		if (config->sound_enabled == 0) {
		  play = 0;
		  pTune = NULL;
		}
		
		while (play) {
			p = pTune;
			while (1) {
				if (play == 0) {
					pwmToneStop ();
					pTune = NULL;
					break;
				}
				if (p->pwm_duration == PWM_DURATION_END) {
					pwmToneStop ();
					if (play)
						play--;
					if (play == 0)
						pTune = NULL;
					break;
				}
				if (p->pwm_duration == PWM_DURATION_LOOP) {
					pwmToneStop ();
					break;
				}
				if (p->pwm_note == PWM_NOTE_OFF) {
					pwmToneStop();
				} else if (p->pwm_note != PWM_NOTE_PAUSE) {
					pwmToneStart (p->pwm_note);
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

	SIM->SCGC6 |= SIM_SCGC6_TPM0;

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

	/*
	 * Initialize channel mode. We select edge-aligned PWM mode,
         * active low.
	 */

	TPM0->C[TPM_CHANNEL].SC = TPM_CnSC_MSB | TPM_CnSC_ELSB;

	/* Create PWM thread */

	pThread = chThdCreateStatic(waPwmThread, sizeof(waPwmThread),
		TPM_THREAD_PRIO, pwmThread, NULL);

	return;
}

/******************************************************************************
*
* pwmToneStart - start playing a tone
*
* This function programs the TPM channel for a desired tone frequency and
* enables the timer. This produces a square wave at the desired tone on
* the output pin, The <tone> argument is the tone frequency in Hz. The
* tone may be changed by calling pwmToneStart() again with a different
* frequency. The tone will continue to play until pwmToneStop() is called
* to turn off the timer.
*
* RETURNS: N/A
*/

void pwmToneStart (uint32_t tone)
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
* pwmToneStop - Turn off the TPM channel
*
* This function disables the TPM channel. If a tone is currently playing,
* calling pwmToneStop() will cause it to stop. This function may be called
* even if the TPM is already stopped, thought his has no effect.
*
* RETURNS: N/A
*/

void pwmToneStop (void)
	{
	/* Turn off timer and clear modulus and pulse width */
 
	TPM0->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);
	TPM0->C[TPM_CHANNEL].V = 0;
	TPM0->MOD = 0;

	return;
	}

/******************************************************************************
*
* pwmThreadPlay - Play a tune using the background music thread.
*
* This function wakes up the background music thread to play an array of
* notes that form a tune. The tune is formatted as an array of PWM_NOTE
* structures. The <p> argument is a pointer to the array. Each PWM_NOTE
* contains a frequency (pwm_note) and duration (pwm_duration). The music
* thread will program the PWM to play the note and then pause for the
* duration period before moving on to the next note. The following special
* cases are handled:
*
* - If pwm_duration is PWM_DURATION_END, this indicates the end of the
*   tune. The thread will go to sleep when it reaches the end of the tune
*   and wait for pwmThreadPlay() to be called again. The value of pwm_note
*   is ignored.
*
* - If pwm_duration is PWM_DURATION_LOOP, then the thread will re-play
*   the tune over and over again. To stop the loop from playing,
*   pwmThreadPlay() must be called with a NULL pointer.
*
* - If pwm_note is PWM_NOTE_PAUSE, then the TPM will be left in its
*   current state and simply pause for the duration period.
*
* - If pwm_note is PWM_NOTE_NONE, then the TPM will be turned off and
*   the thread will pause for the duration period. This can be used to
*   create a silent pause period.
*
* Calling pwmThreadPlay() with a <p> pointer of NULL has no effect,
* except to cease the playing of a tune that is looping.
*
* Calling pwmThreadPlay() with a new tune pointer while a tune is already
* playing, the new tune will be played after the current tune completes
* (unless the current tune is set to loop, in which case playThreadPlay(NULL)
* must be called to cancel it first).
*
* If pwmThreadPlay() is called more than once with a non-NULL tune pointer
* while a tune is playing, only the tune specified in the last call will
* be played (tunes are not queued).
*
* RETURNS: N/A
*/

void pwmThreadPlay (const PWM_NOTE * p)
{
	thread_t * t;

	/*
	 * Cancel any currently playing tune and wait for the
	 * player thread to go idle. We have to do this otherwise
	 * our next command to play a tune might be ignored.
	 */

	if (p == NULL) {
		play = 0;
		while (pTune != NULL)
			chThdSleepMicroseconds (10);
		return;
	}

	pTune = (PWM_NOTE *)p;
	if (play == 0) {
		play = 1;
		t = pThread;
		chThdResume (&t, MSG_OK);
	} else
		play = 2;

	return;
}
