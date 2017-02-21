/*-
 * Copyright (c) 2017
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
#include "chprintf.h"
#include "dac_lld.h"
#include "rand.h"

#include "ff.h"
#include "ffconf.h"

#include "src/gdisp/gdisp_driver.h"
extern LLDSPEC void gdisp_lld_write_start_ex(GDisplay *g);

#include <string.h>

#define FRAMES_PER_SECOND	8
#define FRAMERES_HORIZONTAL	128
#define FRAMERES_VERTICAL	96
#define FRAME_DELAY_TICKS	\
	(((CH_CFG_ST_FREQUENCY / FRAMES_PER_SECOND) / FRAMERES_VERTICAL) * 2)

static uint32_t
hackers_init(OrchardAppContext *context)
{
	(void)context;
	return (0);
}

static void
hackers_start(OrchardAppContext *context)
{
	uint32_t start_ticks;
	uint32_t frame_draw_ticks;
	uint32_t elapsed_ticks;
	uint32_t expected_ticks;
	GWidgetInit wi;
 	GHandle ghExitButton;
	GEvent * pe;
	GListener gl;
	char fname[64];
	char * track;
	pixel_t * buf;
	FIL f;
	UINT br;
	int p;
	int i;

	(void)context;

	chThdSetPriority (10);

	gdispClear (Blue);

	gwinWidgetClearInit (&wi);
	wi.g.show = TRUE;
	wi.g.width = gdispGetWidth();
	wi.g.height = gdispGetHeight();
	wi.g.y = 0;
	wi.g.x = 0;
	wi.text = "";
	wi.customDraw = noRender;

	ghExitButton = gwinButtonCreate (NULL, &wi);
	geventListenerInit (&gl);
	gwinAttachListener (&gl);

	i = rand () % 3;

	switch (i) {
		case 0:
			track = "hackers2";
			break;
		case 1:
			track = "bill";
			break;
		case 2:
		default:
			track = "hackers";
			break;
	}

	chsnprintf (fname, sizeof(fname), "video/%s/video.bin", track, i);
	if (f_open (&f, fname, FA_READ) != FR_OK)
		goto out;

	/* start the audio */

	chsnprintf (fname, sizeof(fname), "video/%s/sample.raw", track);
	dacPlay (fname);

	/* start the video */

	i = 0;
	elapsed_ticks = 0;
	expected_ticks = 0;

	/*
	 * This tells the graphics controller the dimentions of the
	 * drawing area. The drawing aperture is like a circular
	 * buffer: we can keep writing pixels to it sequentially and
	 * when we fill the area, the controller will automatically
	 * wrap back to the beginning. We tell the controller the
	 * drawing frame is twice the size of our actual video
	 * frame data.
	 */

	GDISP->p.x = 32;
	GDISP->p.y = 24;
	GDISP->p.cx = FRAMERES_HORIZONTAL * 2;
	GDISP->p.cy = FRAMERES_VERTICAL * 2;
	gdisp_lld_write_start (GDISP);
	gdisp_lld_write_stop (GDISP);

	buf = chHeapAlloc (NULL, FRAMERES_HORIZONTAL * 4);

	while (1) {
		start_ticks = chVTGetSystemTime ();

		/* Read two scan lines from the stream */
		f_read (&f, buf, FRAMERES_HORIZONTAL * 4, &br);
		if (br == 0)
			break;

		/*
		 * Now write them to the screen. Note: we write each
		 * pixel and line twice. This doubles the size of the
		 * displayed image, at the expenso of pixelating it.
		 * But hey, old school pixelated video is elite, right?
		 *
		 * Note: gdisp_lld_write_start_ex() is a custom version
		 * of gdisp_lld_write_start() that doesn't update the
		 * drawing area. We don't need to do that since all our
		 * frames will go to the same place. This means we
		 * only need to initialize it once. Sadly, we still need
		 * to call the start/stop routines every time we go
		 * to draw to the screen because the sceen and SD card
		 * are on the same SPI channel. For performance, we
		 * program the SPI controller to use 16-bit mode when
		 * talking to to the screen, but we use 8-bit mode when
		 * talking to the SD card. We need to use the start/stop
		 * function to switch the modes.
		 */

		gdisp_lld_write_start_ex (GDISP);

		/* Draw the first line twice. */

		for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
			GDISP->p.color = buf[p];
			gdisp_lld_write_color (GDISP);
			gdisp_lld_write_color (GDISP);
		}
		for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
			GDISP->p.color = buf[p];
			gdisp_lld_write_color (GDISP);
			gdisp_lld_write_color (GDISP);
		}

		/* Draw the second line twice. */

		for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
			GDISP->p.color = buf[p + FRAMERES_HORIZONTAL];
			gdisp_lld_write_color (GDISP);
			gdisp_lld_write_color (GDISP);
		}
		for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
			GDISP->p.color = buf[p + FRAMERES_HORIZONTAL];
			gdisp_lld_write_color (GDISP);
			gdisp_lld_write_color (GDISP);
		}

		gdisp_lld_write_stop (GDISP);

		i += 2;
		if (i == FRAMERES_VERTICAL)
			i = 0;

		/* Check for the user requesting exit */

		pe = geventEventWait (&gl, 0);
		if (pe != NULL && pe->type == GEVENT_GWIN_BUTTON)
			break;

		/*
		 * Synchronization.
		 * FRAME_DELAY_TICKS is the amount of ticks it should take
		 * to draw a scanline in a frame, which is what we just did.
		 * We keep track of the theoretical time it should take to
		 * draw frames in expected_ticks, and we count the actual
		 * amount of time it took in elapsed_ticks. We can't really
		 * do anything if we lag, but in theory we should always
		 * be just slightly faster than the amount of time required.
		 * If we detect that we've taken less time than needed,
		 * calculate how many ticks ahead we are and sleep for
		 * that amount of time. If we're not ahead, we only
		 * sleep for one tick.
		 *
		 * We must sleep at least one tick to let the audio thread
		 * run and play the soundtrack.
		 *
		 * The fact that this works amazes nobody more than me.
		 */

		frame_draw_ticks = chVTGetSystemTime () - start_ticks;
		elapsed_ticks += frame_draw_ticks;
		expected_ticks += FRAME_DELAY_TICKS;
		if (elapsed_ticks < expected_ticks) {
			chThdSleep (expected_ticks - elapsed_ticks);
			elapsed_ticks = expected_ticks;
		} else
			chThdSleep (1);
	}

	chHeapFree (buf);

	f_close (&f);
	dacPlay (NULL);

out:

	geventDetachSource (&gl, NULL);
	gwinDestroy (ghExitButton);

	orchardAppExit ();

	return;
}

static void
hackers_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	(void)context;
	(void)event;

	return;
}

static void
hackers_exit(OrchardAppContext *context)
{
	(void)context;
	return;
}

orchard_app("Hackers", /*APP_FLAG_HIDDEN|*/APP_FLAG_AUTOINIT,
	hackers_init, hackers_start, hackers_event, hackers_exit);
