#include "orchard-app.h"
#include "orchard-ui.h"
#include "fontlist.h"

#include "ff.h"
#include "ffconf.h"

#define	UPDATER_BASE		0x20001400
#define UPDATER_NAME		"UPDATER.BIN"
#define UPDATER_SIZE		7168

typedef uint32_t (*pFwUpdate) (void);

pFwUpdate fwupdatefunc = (pFwUpdate)(UPDATER_BASE | 1);

static uint32_t update_init(OrchardAppContext *context)
{
	(void)context;

	return (0);
}

static void update_start(OrchardAppContext *context)
{
	FIL f;
	UINT br;
	GHandle ghConsole;
	GWidgetInit wi;
	font_t font;
	FILINFO finfo;

	(void)context;

	font = gdispOpenFont (FONT_KEYBOARD);
	gwinSetDefaultFont (font);

	gwinWidgetClearInit (&wi);
	wi.g.show = FALSE;
	wi.g.x = 0;
	wi.g.y = 0;
	wi.g.width = gdispGetWidth();
	wi.g.height = gdispGetHeight();
        ghConsole = gwinConsoleCreate (0, &wi.g);
	gwinSetColor (ghConsole, Yellow);
	gwinSetBgColor (ghConsole, Blue);
	gwinShow (ghConsole);
	gwinClear (ghConsole);

	gwinPrintf (ghConsole, "Searching for updater...\n");
	if (f_stat (UPDATER_NAME, &finfo) != FR_OK) {
		gwinPrintf (ghConsole, "Not found.\n");
		goto done;
	} else
		gwinPrintf (ghConsole, "Found! (%d bytes)\n", finfo.fsize);

	gwinPrintf (ghConsole, "Searching for firmware image...\n");
	if (f_stat ("BADGE.BIN", &finfo) != FR_OK) {
		gwinPrintf (ghConsole, "Not found.\n");
		goto done;
	} else
		gwinPrintf (ghConsole, "Found! (%d bytes)\n", finfo.fsize);

	gwinPrintf (ghConsole, "\nThe firmware will now be re-\n"
				"flashed from the SD card.\n\n"
				"PLEASE DO NOT POWER OFF THE\n"
				"BADGE OR REMOVE THE CARD UNTIL\n"
				"THE UPDATE COMPLETES!\n\n"
				"The system will reboot when\n"
				"flashing is done. \n");

	chThdSleepMilliseconds (2000);

	f_open (&f, UPDATER_NAME, FA_READ);
	f_read (&f, (char *)UPDATER_BASE, UPDATER_SIZE, &br);
	f_close (&f);

	/*
	 * If we reached this point, we've loaded an updater image into
	 * RAM and we're about to run it. Once it starts, it will
	 * completely take over the system, and either the firmware
	 * will be updated or the system will hang. Fingers crossed.
	 */

	fwupdatefunc ();

	/* NOTREACHED */

done:
	gwinPrintf (ghConsole, "\nAborting.\n");

	chThdSleepMilliseconds (2000);

	gdispCloseFont (font);
	gwinDestroy (ghConsole);
	orchardAppExit ();

	return;
}

void update_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	(void)context;
	(void)event;

	return;
}

static void update_exit(OrchardAppContext *context)
{
	(void)context;

	return;
}

orchard_app("Update firmware", 0, update_init, update_start,
		update_event, update_exit);
