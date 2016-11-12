#include "ch.h"
#include "hal.h"
#include "osal.h"
#include "spi.h"
#include "pal.h"

#include "xpt2046_reg.h"
#include "xpt2046.h"

#define XPT_CHIP_SELECT_PORT	GPIOE
#define XPT_CHIP_SELECT_PIN	19

uint16_t xptGet (uint8_t cmd)
{
	uint8_t		reg;
	uint8_t		v[2];
	uint16_t	val;
 	uint8_t		br;

	reg = cmd | XPT_CTL_SDREF | XPT_CTL_START;

	spiAcquireBus (&SPID2);
	br = SPID2.spi->BR;
	SPID2.spi->BR = 0x75;
	palClearPad (XPT_CHIP_SELECT_PORT, XPT_CHIP_SELECT_PIN);
	spiSend (&SPID2, 1, &reg);
	SPI1->C1 &= ~SPIx_C1_SPIE;
	SPI1->DL = 0xFF;
        while ((SPI1->S & SPIx_S_SPRF) == 0)
		;
	v[0] = SPI1->DL;
	SPI1->DL = 0xFF;
        while ((SPI1->S & SPIx_S_SPRF) == 0)
                ;
	v[1] = SPI1->DL;
	SPI1->C1 |= SPIx_C1_SPIE;
	palSetPad (XPT_CHIP_SELECT_PORT, XPT_CHIP_SELECT_PIN);	
	SPID2.spi->BR = br;
	spiReleaseBus (&SPID2);

	val = v[1] >> 3;
	val |= v[0] << 5;

	return (val);
}

#ifdef notdef
#include "chprintf.h"
extern void * stream;
void xpt_test(void)
{
	uint16_t	v;
	uint16_t	z1;

	while (1) {
		z1 = xptGet (XPT_CHAN_Z1);
		if (z1 > 10) {
			chprintf (stream, "press(%d): ", z1);
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
