/*-
 * Copyright (c) 2016
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "orchard-app.h"
#include "orchard-ui.h"
#include "fontlist.h"

#include "ff.h"
#include "ffconf.h"

#include "led.h"
#include "radio_lld.h"

#include "../updater/updater.h"

typedef uint32_t (*pFwUpdate) (void);

pFwUpdate fwupdatefunc = (pFwUpdate)(UPDATER_BASE | 1);

static void
gtimerCallback (void * arg)
{
	(void)arg;

	chThdExitS (MSG_OK);

	return;
}

static uint32_t
update_init(OrchardAppContext *context)
{
	(void)context;

	return (0);
}

static void
update_start(OrchardAppContext *context)
{
	FIL f;
	UINT br;
	GHandle ghConsole;
	GWidgetInit wi;
	font_t font;
	FILINFO finfo;
	GTimer * t;

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

	/*
	 * We need to take over the system and we're about to clobber
	 * the heap, so it's important that no other threads preempt
	 * us. The major culprits are the main thread, which might
	 * wake up when a radio event occurs, the LED thread and the
	 * gtimer thread. We can stop the radio and LED thread
	 * easily enough. For the gtimer thread, we schedule a
	 * timer event that calls chThdExitS().
	 */

	chSysLock ();
	chThdSetPriority (ABSPRIO);
	chSysUnlock ();

	/* Stop the radio */

	radioStop (&KRADIO1);

	/* Stop the LED thread */

	effectsStop ();

	/* Stop the gtimer thread */

	t = chHeapAlloc (NULL, sizeof(GTimer));
	gtimerInit (t);
	gtimerStart (t, gtimerCallback, NULL, FALSE, 1);

	/* Wait for the LED and gtimer threads to exit */

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

static void
update_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	(void)context;
	(void)event;

	return;
}

static
void update_exit(OrchardAppContext *context)
{
	(void)context;

	return;
}

orchard_app("UFW2", "update.rgb", APP_FLAG_HIDDEN,
            update_init, update_start, update_event, update_exit, 9999);
