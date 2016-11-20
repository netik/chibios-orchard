#ifndef _TPM_H_
#define _TPM_H_

/*
 * We use TPM0 channe 1, which is wired to pin PTC2
 */

#define TPM_CHANNEL	1

/*
 * We pre-scale the clock by a factor of 8
 */

#define TPM_PRESCALE	8

/*
 * Base TPM clock is the system clock (48MHz), which we divide by
 * the prescale factor when calculating the desired output
 * frequency.
 */

#define TPM_FREQ	(KINETIS_SYSCLK_FREQUENCY / TPM_PRESCALE)

/*
 * Structure for music array. Each entry in the array is a tone
 * and duration tuple that describes a single note. There are two
 * special cases for diration: PWM_DURATION_END, which indicates
 * the end of the tune to play, and PWM_DURATION_LOOP, which indicates
 * this tune should be played over and over again (until told to stop).
 * There are also two special cases for a note: PWM_NODE_PAUSE, which
 * that we should just pause for the duration time, and PWM_NODE_OFF,
 * which measn to turn off the tone generator completely.
 */

typedef struct pwm_note {
	uint16_t	pwm_note;
	uint16_t	pwm_duration;
} PWM_NOTE;

#define PWM_DURATION_END	0xFFFF
#define PWM_DURATION_LOOP	0xFFFE
#define PWM_NOTE_PAUSE		0xFFFF
#define PWM_NOTE_OFF		0xFFFE

/* TPM note player thread priority */

#define TPM_THREAD_PRIO	70

extern void pwmInit (void);

extern void pwmToneStart (uint32_t);
extern void pwmToneStop (void);

extern void pwmThreadPlay (const PWM_NOTE *);

#endif /* _TPM_H_ */
