#include "orchard-app.h"
#include "orchard-ui.h"

#include "src/gdriver/gdriver.h"
#include "src/ginput/ginput_driver_mouse.h"

#include <string.h>

static uint32_t
calibrate_init (OrchardAppContext *context)
{
	(void)context;

	/*
	 * We don't want any extra stack space allocated for us.
	 * We'll use the heap.
	 */

	return (0);
}

static void
calibrate_start (OrchardAppContext *context)
{
	(void)context;

	(void)ginputCalibrateMouse (0);

	orchardAppExit ();

	return;
}

static void
calibrate_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{

	(void)event;
	(void)context;

	return;
}

static void
calibrate_exit (OrchardAppContext *context)
{
	userconfig * config;
	GMouse * m;

	(void)context;

	m = (GMouse*)gdriverGetInstance (GDRIVER_TYPE_MOUSE, 0);

	config = getConfig ();

	memcpy (&config->touch_data, &m->caldata,
	    sizeof(config->touch_data));
	config->touch_data_present = 1;

	configSave (config);

	return;
}

orchard_app("CalibrateTS", APP_FLAG_HIDDEN,
    calibrate_init, calibrate_start, calibrate_event, calibrate_exit);
