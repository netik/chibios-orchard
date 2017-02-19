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

#include <string.h>

#define FRAMES_PER_SECOND	8
#define FRAMERES_HORIZONTAL	150
#define FRAMERES_VERTICAL	112
#define FRAME_DELAY_TICKS	\
	((CH_CFG_ST_FREQUENCY / FRAMES_PER_SECOND) / FRAMERES_VERTICAL)

static int delayTicks = 0;

static uint32_t hackers_init(OrchardAppContext *context)
{
	volatile uint32_t stime;
	volatile uint32_t etime;
	uint32_t cum;
	uint32_t diff;
	int samplecnt;
	pixel_t buf[FRAMERES_HORIZONTAL];
	FIL f;
	UINT br;
	int i;

	(void)context;

	if (delayTicks != 0)
		return (0);

	f_open (&f, "drwho.raw", FA_READ);

	cum = 0;
	samplecnt = 0;

	for (i = 0; i < 500; i++) {
		stime = chVTGetSystemTime ();
		f_read (&f, buf, sizeof(buf), &br);
		etime = chVTGetSystemTime ();
		diff = etime - stime;
		if (diff > 10 && diff < 100) {
			cum += (etime - stime);
			samplecnt++;
		}
	}

	f_close (&f);

	cum /= samplecnt;

	delayTicks = 1;

	if (cum < FRAME_DELAY_TICKS)
		delayTicks += FRAME_DELAY_TICKS - cum;

	return (0);
}

static void hackers_start(OrchardAppContext *context)
{
	GWidgetInit wi;
 	GHandle ghExitButton;
	GEvent * pe;
	GListener gl;
	char fname[64];
	char * track;
	pixel_t buf[FRAMERES_HORIZONTAL];
	FIL f;
	UINT br;
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
			track = "hackers";
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

	/* start the video (and hope they stay in sync) */

	i = 0;
	while (1) {
		f_read (&f, buf, sizeof(buf), &br);
		if (br == 0)
			break;
		gdispBlitAreaEx (85, 64 + i,	/* Screen position */
		    FRAMERES_HORIZONTAL, 1,	/* Drawing dimentions */
		    0, 0,			/* Position in pixel buffer */
		    FRAMERES_HORIZONTAL,	/* Width of pixel buffer */
		    buf);
		i++;
		if (i == FRAMERES_VERTICAL)
			i = 0;
		pe = geventEventWait (&gl, 0);
		if (pe != NULL && pe->type == GEVENT_GWIN_BUTTON)
			break;
		chThdSleep (delayTicks);
	}

	f_close (&f);
	dacPlay (NULL);

out:

	geventDetachSource (&gl, NULL);
	gwinDestroy (ghExitButton);

	orchardAppExit ();

	return;
}

void hackers_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	(void)context;
	(void)event;

	return;
}

static void hackers_exit(OrchardAppContext *context)
{
	(void)context;
	return;
}

orchard_app("Hackers", /*APP_FLAG_HIDDEN|*/APP_FLAG_AUTOINIT,
	hackers_init, hackers_start, hackers_event, hackers_exit);
