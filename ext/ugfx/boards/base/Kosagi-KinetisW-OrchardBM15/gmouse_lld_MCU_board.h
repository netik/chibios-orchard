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

#include <string.h>

static const float calibrationData[] = {
	0.09198,		// ax
	-0.00305,		// bx
	-14.45468,		// cx
	0.00118,		// ay
	0.06849,		// by
	-27.25699		// cy
};
 
bool_t LoadMouseCalibration(unsigned instance, void *data, size_t sz)
{
	(void)data;
	if (sz != sizeof(calibrationData) || instance != 0) {
		return FALSE;
	}
	memcpy (data, (void*)&calibrationData, sz);

	return (TRUE);
}

// Resolution and Accuracy Settings
#define GMOUSE_MCU_PEN_CALIBRATE_ERROR		16
#define GMOUSE_MCU_PEN_CLICK_ERROR		30
#define GMOUSE_MCU_PEN_MOVE_ERROR		16
#define GMOUSE_MCU_FINGER_CALIBRATE_ERROR	14
#define GMOUSE_MCU_FINGER_CLICK_ERROR		18
#define GMOUSE_MCU_FINGER_MOVE_ERROR		14
#define GMOUSE_MCU_Z_MIN		0	/* The minimum Z reading */
#define GMOUSE_MCU_Z_MAX		4096	/* The maximum Z reading */

#define GMOUSE_MCU_Z_TOUCHON		150	/*
						 * Values between this and
						 * Z_MAX are definitely pressed
						 */

#define GMOUSE_MCU_Z_TOUCHOFF		100	/*
						 * Values between this and
						 * Z_MIN are definitely not
						 * pressed
						 */

/*
 * How much extra data to allocate at the end of the GMouse
 * structure for the board's use
 */

#define GMOUSE_MCU_BOARD_DATA_SIZE	0

static bool_t init_board(GMouse *m, unsigned driverinstance) {
	(void)m;
	(void)driverinstance;
	return (TRUE);
}

static bool_t read_xyz(GMouse *m, GMouseReading *prd) {
	(void)m;

	/*
	 * Note: we flip the X and Y axis readings because the
	 * screen is being used in landscape mode and the touch
	 * controller assumes portrait mode.
	 */

	prd->y = xptGet (XPT_CHAN_X);
	prd->x = xptGet (XPT_CHAN_Y);
	prd->z = xptGet (XPT_CHAN_Z1);

	return (TRUE);
}

#endif /* _LLD_GMOUSE_MCU_BOARD_H */
