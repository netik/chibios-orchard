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
#include "dac_lld.h"
#include "pit_lld.h"

#include "ff.h"
#include "ffconf.h"

#include "fix_fft.h"

#include "src/gdisp/gdisp_driver.h"

#include "fontlist.h"

#include <string.h>
#include <stdlib.h>

#define BACKGROUND HTML2COLOR(0x470b67)

typedef struct _VideoHandles {
	char **			listitems;
	int			itemcnt;
	OrchardUiContext	uiCtx;
	short			in[128];
	short			out[128];
} VideoHandles;

static uint32_t
music_init(OrchardAppContext *context)
{
	(void)context;
	return (0);
}

static void
music_start (OrchardAppContext *context)
{
	VideoHandles * p;
	DIR d;
	FILINFO info;
	int i;

	f_opendir (&d, "\\");

	i = 0;

	while (1) {
		if (f_readdir (&d, &info) != FR_OK || info.fname[0] == 0)
			break;
		if (strstr (info.fname, ".RAW") != NULL)
			i++;
	}

	f_closedir (&d);

	if (i == 0) {
		orchardAppExit ();
		return;
	}

	i += 2;

	p = chHeapAlloc (NULL, sizeof(VideoHandles));
	memset (p, 0, sizeof(VideoHandles));
	p->listitems = chHeapAlloc (NULL, sizeof(char *) * i);
	p->itemcnt = i;

	p->listitems[0] = "Choose a song";
	p->listitems[1] = "Exit";

	f_opendir (&d, "\\");

	i = 2;

	while (1) {
		if (f_readdir (&d, &info) != FR_OK || info.fname[0] == 0)
			break;
		if (strstr (info.fname, ".RAW") != NULL) {
			p->listitems[i] =
			    chHeapAlloc (NULL, strlen (info.fname) + 1);
			memset (p->listitems[i], 0, strlen (info.fname) + 1);
			strcpy (p->listitems[i], info.fname);
			i++;
		}
	}

	p->uiCtx.itemlist = (const char **)p->listitems;
	p->uiCtx.total = p->itemcnt - 1;

	context->instance->ui = getUiByName ("list");
	context->instance->uicontext = &p->uiCtx;
	context->instance->ui->start (context);

	context->priv = p;
	return;
}

static void
columnDraw (int col, int amp, int erase)
{
	int x;
	int y;
	int level;

	if (amp > 128)
		amp = 128;

	GDISP->p.x = col * 5;
	GDISP->p.y = 240 - amp;
	GDISP->p.cx = 4;
	GDISP->p.cy = amp;
	gdisp_lld_write_start (GDISP);
	gdisp_lld_write_stop (GDISP);

        GDISP->p.x = -1;
       	GDISP->p.y = -1;

	gdisp_lld_write_start (GDISP);
	for (y = amp; y > 0; y--) {
		if (erase)
			GDISP->p.color = BACKGROUND;
		else {
			level = y;
			if (level >= 0 && level < 32)
				GDISP->p.color = Lime;
			if (level >= 32 && level < 64)
				GDISP->p.color = Yellow;
			if (level >= 64 && level < 128)
				GDISP->p.color = Red;
		}
		for (x = 0; x < 4; x++) {
			gdisp_lld_write_color (GDISP);
		}
	}
	gdisp_lld_write_stop (GDISP);

	return;
}

static int
musicPlay (VideoHandles * p, char * fname)
{
	FIL f;
	int i;
	uint16_t * buf;
	uint16_t * dacBuf;
	UINT br;
	float fl;
	uint32_t b;
	GEventMouse * me;
	GSourceHandle gs;
	GListener gl;

	dacPlay (NULL);

	if (f_open (&f, fname, FA_READ) != FR_OK)
		return (0);

	dacBuf = chHeapAlloc (NULL,
	    (DAC_SAMPLES * sizeof(uint16_t)) * 2);

	pitEnable (&PIT1, 1);

	buf = dacBuf;

	f_read (&f, buf, DAC_SAMPLES * sizeof(uint16_t), &br);

	gs = ginputGetMouse (0);
	geventListenerInit (&gl);
	geventAttachSource (&gl, gs, GLISTEN_MOUSEMETA);

	while (1) {

		dacSamplesPlay (buf, DAC_SAMPLES);

		for (i = 1; i < 63; i++) {
			b = p->in[i];
			columnDraw (i, b, 1);
		}

		chThdSleepMicroseconds (10);

		for (i = 0; i < 128; i++) {
			fl = (float)buf[i];
			fl *= 1.6;
			p->in[i] = fl;
		}

		fix_fft (p->in, p->out, 7, 0);

		for (i = 0; i < 63; i++) {
			b = abs(p->in[i]);
			b >>= 2;
			p->in[i] = b;
		}

		for (i = 1; i < 63; i++) {
			b = p->in[i];
			columnDraw (i, b, 0);
		}

		if (buf == dacBuf)
			buf += DAC_SAMPLES;
		else
			buf = dacBuf;

		f_read (&f, buf, DAC_SAMPLES * sizeof(uint16_t), &br);

		if (br == 0)
			break;

		me = (GEventMouse *)geventEventWait(&gl, 0);
		if (me != NULL && me->type == GEVENT_TOUCH)
			break;

		dacSamplesWait ();

	}

	f_close (&f);
	pitDisable (&PIT1, 1);
	chHeapFree (dacBuf);

	geventDetachSource (&gl, NULL);

	if (me != NULL && me->type == GEVENT_TOUCH)
		return (-1);

	return (0);
}

static void
music_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	OrchardUiContext * uiContext;
	const OrchardUi * ui;
	VideoHandles * p;
	char buf[32];
	font_t font;

	p = context->priv;
	ui = context->instance->ui;
	uiContext = context->instance->uicontext;

	if (event->type == appEvent && event->app.event == appTerminate) {
		if (ui != NULL)
 			ui->exit (context);
	}

	if (event->type == ugfxEvent)
		ui->event (context, event);

	if (event->type == uiEvent && event->ui.code == uiComplete &&
	    event->ui.flags == uiOK) {
		/*
		 * If this is the list ui exiting, it means we chose a
		 * song to play, or else the user selected exit.
		 */

		dacWait ();
 		ui->exit (context);
		context->instance->ui = NULL;

		/* User chose the "EXIT" selection, bail out. */

		if (uiContext->selected == 0) {
			orchardAppExit ();
			return;
		}
		gdispClear (BACKGROUND);

		font = gdispOpenFont (FONT_FIXED);
		chsnprintf (buf, sizeof(buf), "Now playing: %s",
		    p->listitems[uiContext->selected + 1]);
		gdispDrawStringBox (0,
		    gdispGetFontMetric(font, fontHeight) * 2,
		    gdispGetWidth(), gdispGetFontMetric(font, fontHeight),
		    buf, font, Cyan, justifyCenter);
		gdispCloseFont (font);

		if (musicPlay (p, p->listitems[uiContext->selected + 1]) != 0)
			dacPlay ("click.raw");

		orchardAppExit ();
	}

	return;
}

static void
music_exit(OrchardAppContext *context)
{
	VideoHandles * p;
	int i;

	p = context->priv;

	if (p == NULL)
            return;

	for (i = 2; i < p->itemcnt; i++)
		chHeapFree (p->listitems[i]);

	chHeapFree (p->listitems);
	chHeapFree (p);

	context->priv = NULL;

	return;
}

orchard_app("Play Music", "mplay.rgb", 0, music_init, music_start,
    music_event, music_exit);
