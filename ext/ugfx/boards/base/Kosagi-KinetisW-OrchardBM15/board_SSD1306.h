/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef _GDISP_LLD_BOARD_H
#define _GDISP_LLD_BOARD_H

#include "orchard.h"
#include "gpiox.h"
#include "oled.h"
//#define SSD1306_PAGE_PREFIX		0x40

static inline void init_board(GDisplay *g) {
	(void) g;
}

static inline void post_init_board(GDisplay *g) {
	(void) g;
}

static inline void setpin_reset(GDisplay *g, bool_t state) {
	(void) g;

	if(state) {
	  // reset LCD
	  gpioxSetPadMode(GPIOX, oledResPad, GPIOX_OUT_PUSHPULL | GPIOX_VAL_LOW);
	} else {
	  gpioxSetPadMode(GPIOX, oledResPad, GPIOX_OUT_PUSHPULL | GPIOX_VAL_HIGH);
	}
}

static inline void acquire_bus(GDisplay *g) {
	(void) g;

	oledAcquireBus();
}

static inline void release_bus(GDisplay *g) {
	(void) g;

	oledReleaseBus();
}

static inline void write_cmd(GDisplay *g, uint8_t cmd) {
	(void) g;

	oledCmd(cmd);
}

static inline void write_data(GDisplay *g, uint8_t* data, uint16_t length) {
	(void) g;

	oledData(data, length);
}

#endif /* _GDISP_LLD_BOARD_H */
