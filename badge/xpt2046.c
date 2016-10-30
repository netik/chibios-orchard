#include "ch.h"
#include "hal.h"
#include "osal.h"
#include "spi.h"

#include "xpt2046_reg.h"
#include "xpt2046.h"

uint16_t xptGet (uint8_t cmd)
{
	uint8_t		reg;
	uint8_t		v[2];
	uint16_t	val;

	reg = cmd | XPT_CTL_SDREF | XPT_CTL_START;
	spiAcquireBus (&SPID2);
	SPID2.spi->BR = 0x75;
	spiSelect (&SPID2);
	spiSend (&SPID2, 1, &reg);
	spiReceive (&SPID2, 2, &v);
	spiUnselect (&SPID2);
	SPID2.spi->BR = SPID2.config->br;
	spiReleaseBus (&SPID2);

	val = v[1] >> 3;
	val |= v[0] << 5;

	return (val);
}

#ifdef notdef
void xpd_test(void)
{
	uint16_t	v;
	uint16_t	z1;

	while (1) {
		z1 = xptGet (XPT_CHAN_Z1);
		if (z1 > 10) {
			chprintf (stream, "press: ");
			v = xptGet (XPT_CHAN_X);
			chprintf (stream, "X: %d ", v);
			v = xptGet (XPT_CHAN_Y);
			chprintf (stream, "Y: %d\r\n", v);
		}
		chThdSleepMilliseconds (100);
	}

	return;
}
#endif
