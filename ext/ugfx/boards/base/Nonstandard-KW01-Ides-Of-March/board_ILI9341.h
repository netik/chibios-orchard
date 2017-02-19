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
	/*
	 * Enable the slave select function, enable FIFO
	 * mode, disable interrupts.
	 */
	SPI1->C1 |= SPIx_C1_SSOE;
	SPI1->C2 |= SPIx_C2_MODFEN;
	SPI1->C3 |= SPIx_C3_FIFOMODE|SPIx_C3_TNEAREF_MARK;
	SPI1->C1 &= ~SPIx_C1_SPIE;
	SPI1->BR = 0;
	return;
}

static inline void release_bus(GDisplay *g) {
	(void) g;
	/*
	 * Disable the slave select function, disable FIFO
	 * mode, re-enable interrupts.
	 */
	SPI1->C1 &= ~SPIx_C1_SSOE;
	SPI1->C2 &= ~SPIx_C2_MODFEN;
	SPI1->C3 &= ~(SPIx_C3_FIFOMODE|SPIx_C3_TNEAREF_MARK);
	SPI1->C1 |= SPIx_C1_SPIE;
	SPI1->BR = SPID2.config->br;
	spiReleaseBus (&SPID2);
	return;
}

static inline void write_index(GDisplay *g, uint16_t index) {
	(void) g;
	palClearPad (SCREEN_CMDDATA_PORT, SCREEN_CMDDATA_PIN);
	SPI1->DL = index & 0xFF;
	while ((SPI1->S & SPIx_S_SPTEF) == 0)
		;
	(void)SPI1->DL;
	return;
}

static inline void write_data(GDisplay *g, uint16_t data) {
	(void) g;
	palSetPad (SCREEN_CMDDATA_PORT, SCREEN_CMDDATA_PIN);
	SPI1->DL = data & 0xFF;
	while ((SPI1->S & SPIx_S_SPTEF) == 0)
		;
	(void)SPI1->DL;
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
