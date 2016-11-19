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

extern void pwmInit (void);

extern void pwmToneStart (uint32_t);
extern void pwmToneStop (void);

#endif /* _TPM_H_ */
