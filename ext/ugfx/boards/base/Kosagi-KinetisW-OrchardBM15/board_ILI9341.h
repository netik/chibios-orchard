/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */


#ifndef _GDISP_LLD_BOARD_H
#define _GDISP_LLD_BOARD_H

#include "ch.h"
#include "hal.h"
#include "spi.h"
#include "pal.h"

static inline void init_board(GDisplay *g) {
	(void) g;
	return;
}

static inline void post_init_board(GDisplay *g) {
	(void) g;
}

static inline void setpin_reset(GDisplay *g, bool_t state) {
	(void) g;
	(void) state;
}

static inline void set_backlight(GDisplay *g, uint8_t percent) {
	(void) g;
	(void) percent;
}

static inline void acquire_bus(GDisplay *g) {
	(void) g;
	spiAcquireBus (&SPID2);
	/* Enable the slave select function. */
	SPID2.spi->C1 |= SPIx_C1_SSOE;
	SPID2.spi->C2 |= SPIx_C2_MODFEN;
        /* Disable transmit interrupt */
	SPID2.spi->C1 &= ~SPIx_C1_SPTIE;
	return;
}

static inline void release_bus(GDisplay *g) {
	(void) g;
	/* Disable the slave select function. */
	SPID2.spi->C1 &= ~SPIx_C1_SSOE;
	SPID2.spi->C2 &= ~SPIx_C2_MODFEN;
        /* Enable transmit interrupt */
	SPID2.spi->C1 |= SPIx_C1_SPTIE;
	spiReleaseBus (&SPID2);
	return;
}

static inline void write_index(GDisplay *g, uint16_t index) {
	(void) g;
	palClearPad (GPIOE, 18);
 	while ((SPID2.spi->S & SPIx_S_SPTEF) == 0)
		;
	SPID2.spi->DL = index & 0xFF;
	return;
}

static inline void write_data(GDisplay *g, uint16_t data) {
	(void) g;
	palSetPad (GPIOE, 18);
 	while ((SPID2.spi->S & SPIx_S_SPTEF) == 0)
		;
	SPID2.spi->DL = data & 0xFF;
	return;
}

static inline void setreadmode(GDisplay *g) {
	(void) g;
}

static inline void setwritemode(GDisplay *g) {
	(void) g;
}

static inline uint16_t read_data(GDisplay *g) {
	(void) g;
	return 0;
}

#endif /* _GDISP_LLD_BOARD_H */
