#include "rand.h"

#include "ch.h"
#include "hal.h"
#include "xpt2046.h"
#include "xpt2046_reg.h"
#include "radio.h"
#include "radio_reg.h"
#include "pit.h"
#include "pit_reg.h"

#include "chprintf.h"
extern void * stream;

static unsigned long int next = 1;

/*
 * Try to get a little bit of entropy by reading some of the sensors
 * and one of the interval timers.
 */

void randInit (void)
{
	int i;
	unsigned long seed;

	seed = CSR_READ_4(&PIT1, PIT_CVAL1) & 0xFFFF;
	for (i = 0; i < 100; i++) {
		seed += xptGet (XPT_CHAN_TEMP0 | XPT_CTL_SDREF);
		seed += xptGet (XPT_CHAN_TEMP1 | XPT_CTL_SDREF);
		seed += xptGet (XPT_CHAN_AUX | XPT_CTL_SDREF);
	}
	seed += radioRead (&KRADIO1, KW01_TEMPVAL);
	seed += radioRead (&KRADIO1, KW01_RSSIVAL);
	seed += CSR_READ_4(&PIT1, PIT_CVAL1) & 0xFFFF;
	srand (seed);

	return;
}

int rand(void)
{
	next = next * 1103515245 + 12345;
	return (unsigned int)(next/65536) % 32768;
}

void srand(unsigned int seed)
{
	next = seed;
}
