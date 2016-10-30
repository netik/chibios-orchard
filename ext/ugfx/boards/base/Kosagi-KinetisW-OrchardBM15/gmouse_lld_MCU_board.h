/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef _LLD_GMOUSE_MCU_BOARD_H
#define _LLD_GMOUSE_MCU_BOARD_H

#include "xpt2046.h"
#include "xpt2046_reg.h"

// Resolution and Accuracy Settings
#define GMOUSE_MCU_PEN_CALIBRATE_ERROR		8
#define GMOUSE_MCU_PEN_CLICK_ERROR			6
#define GMOUSE_MCU_PEN_MOVE_ERROR			4
#define GMOUSE_MCU_FINGER_CALIBRATE_ERROR	14
#define GMOUSE_MCU_FINGER_CLICK_ERROR		18
#define GMOUSE_MCU_FINGER_MOVE_ERROR		14
#define GMOUSE_MCU_Z_MIN					0			// The minimum Z reading
#define GMOUSE_MCU_Z_MAX					100			// The maximum Z reading
#define GMOUSE_MCU_Z_TOUCHON				80			// Values between this and Z_MAX are definitely pressed
#define GMOUSE_MCU_Z_TOUCHOFF				70			// Values between this and Z_MIN are definitely not pressed

// How much extra data to allocate at the end of the GMouse structure for the board's use
#define GMOUSE_MCU_BOARD_DATA_SIZE			0

static bool_t init_board(GMouse *m, unsigned driverinstance) {
	(void)m;
	(void)driverinstance;
	return (TRUE);
}

static bool_t read_xyz(GMouse *m, GMouseReading *prd) {
	uint16_t z1;

	(void)m;

	prd->buttons = 0;
	prd->z = 0;

	z1 = xptGet (XPT_CHAN_Z1);

	if (z1 > 50) {
		prd->z = 1;
		prd->x = xptGet (XPT_CHAN_X);
		prd->y = xptGet (XPT_CHAN_Y);
		prd->buttons = GINPUT_TOUCH_PRESSED;
	}

	return (TRUE);
}

#endif /* _LLD_GMOUSE_MCU_BOARD_H */
