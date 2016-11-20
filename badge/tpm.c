#include "ch.h"
#include "hal.h"
#include "chschd.h"
#include "chthreads.h"

#include "tpm.h"
#include "pwm_lld.h"

#include "chprintf.h"
extern void * stream;

static THD_WORKING_AREA(waPwmThread, 64);

static PWM_NOTE * pTune;
static thread_t * pThread;

static THD_FUNCTION(pwmThread, arg) {
	PWM_NOTE * p;
	(void)arg;

	chRegSetThreadName ("pwm");

	while (1) {
		chThdSleep (TIME_INFINITE);
		while (pTune != NULL) {
			p = pTune;
			while (1) {
				if (p->pwm_duration == PWM_DURATION_END) {
					pwmToneStop ();
					pTune = NULL;
					break;
				}
				if (p->pwm_duration == PWM_DURATION_LOOP) {
					pwmToneStop ();
					break;
				}
				if (p->pwm_note != PWM_NOTE_PAUSE)
					pwmToneStart (p->pwm_note);
				if (p->pwm_note == PWM_NOTE_OFF)
					pwmToneStop();
				chThdSleepMilliseconds (p->pwm_duration);
				p++;
			}
		}
			
	}

	/* NOTREACHED */
	return;
}

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

	/* Create PWM thrad */

	pThread = chThdCreateStatic(waPwmThread, sizeof(waPwmThread),
		TPM_THREAD_PRIO, pwmThread, NULL);
	/*while (pThread->p_state != CH_STATE_SLEEPING)
		chThdSleepMilliseconds (1);*/

	return;
}

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

void pwmToneStop (void)
	{
	/* Turn off timer and clear modulus and pulse width */
 
	TPM0->SC &= ~(TPM_SC_CMOD_LPTPM_CLK|TPM_SC_CMOD_LPTPM_EXTCLK);
	TPM0->C[TPM_CHANNEL].V = 0;
	TPM0->MOD = 0;

	return;
	}

void pwmThreadPlay (const PWM_NOTE * p)
{
	pTune = (PWM_NOTE *)p;
	chThdResume (&pThread, MSG_OK);

	return;
}
