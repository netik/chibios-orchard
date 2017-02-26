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

#include "ch.h"
#include "hal.h"
#include "osal.h"

#include "pit_lld.h"
#include "dac_lld.h"
#include "dac_reg.h"
#include "pit_lld.h"
#include "pit_reg.h"

#include "ff.h"
#include "ffconf.h"

#include "gfx.h"
#include "orchard-ui.h"

#include "src/gdisp/gdisp_driver.h"
extern LLDSPEC void gdisp_lld_write_start_ex(GDisplay *g);

#define SAMPLE_CHUNKS		64
#define SAMPLES_PER_LINE	12

#define FRAMERES_HORIZONTAL	128
#define FRAMERES_VERTICAL	96
#define FRAMES_PER_SECOND	8
#define LINE_RATE		2

#define SAMPLE_BYTES_PER_LINE	(SAMPLES_PER_LINE * 2)
#define BYTES_HORIZONTAL	(FRAMERES_HORIZONTAL * 2)
#define SAMPLES_PER_PLAY	(SAMPLES_PER_LINE * SAMPLE_CHUNKS) / LINE_RATE

#define VID_BUFSZ		(BYTES_HORIZONTAL * LINE_RATE)
#define VID_EXTRA		(SAMPLE_BYTES_PER_LINE * 2)
#define AUD_BUFSZ		(SAMPLE_BYTES_PER_LINE * SAMPLE_CHUNKS)

#define SAMPLE_INTERVAL		((SAMPLE_CHUNKS / 4) - 1)

static void
write_pixel (pixel_t p1, int extra)
{
	volatile pixel_t * dst;

	dst = (pixel_t *)&SPI1->DL;

	dst[0] = p1;
	dst[0] = p1;
	if (extra)
		dst[0] = p1;

	while ((SPI1->S & SPIx_S_SPTEF) == 0)
		;

        return;
}

void
videoPlay (char * fname)
{
	GWidgetInit wi;
 	GHandle ghExitButton;
	GEvent * pe;
	GListener gl;
	uint16_t * samples;
	uint16_t * cur;
	uint16_t * ps;
	pixel_t * buf;
	FIL f;
	UINT br;
	int p;
	int i;
	int j;
	uint32_t rate;
	int lastwait;
	int curwait;
	int skipcnt;

	gwinWidgetClearInit (&wi);
	wi.g.show = TRUE;
	wi.g.width = gdispGetWidth ();
	wi.g.height = gdispGetHeight ();
	wi.g.y = 0;
	wi.g.x = 0;
	wi.text = "";
	wi.customDraw = noRender;

	ghExitButton = gwinButtonCreate (NULL, &wi);
	geventListenerInit (&gl);
	gwinAttachListener (&gl);

	if (f_open (&f, fname, FA_READ) != FR_OK)
		goto out;

	/* start the video */

	/*
	 * This tells the graphics controller the dimentions of the
	 * drawing area. The drawing aperture is like a circular
	 * buffer: we can keep writing pixels to it sequentially and
	 * when we fill the area, the controller will automatically
	 * wrap back to the beginning. We tell the controller the
	 * drawing frame is twice the size of our actual video
	 * frame data.
	 */

	GDISP->p.x = 0;
	GDISP->p.y = 0;
	GDISP->p.cx = gdispGetWidth ();
	GDISP->p.cy = gdispGetHeight ();

	gdisp_lld_write_start (GDISP);
	gdisp_lld_write_stop (GDISP);

	buf = chHeapAlloc (NULL, VID_BUFSZ + VID_EXTRA);
	samples = chHeapAlloc (NULL, AUD_BUFSZ);

	dacPlay (NULL);

	i = 0;
	j = 0;
	ps = samples;
	cur = samples;
	skipcnt = 0;
	curwait = 0;
	lastwait = 0;

	pitEnable (&PIT1, 1);

	while (1) {
		/* Read two scan lines from the stream */
		f_read (&f, buf, VID_BUFSZ + VID_EXTRA, &br);

		if (br == 0)
			break;

		for (p = 0; p < SAMPLES_PER_LINE * 2; p++)
			ps[p] = buf[(VID_BUFSZ / 2) + p];

		if ((i & SAMPLE_INTERVAL) == SAMPLE_INTERVAL) {
			curwait = dacWait ();
			if (curwait == 0 && lastwait == 0)
				skipcnt++;
			else
				skipcnt = 0;
			lastwait = curwait;
			dacSamplesPlay (cur, SAMPLES_PER_PLAY);
			if (cur == samples)
				cur = samples + SAMPLES_PER_PLAY; 
			else
				cur = samples;
		}

		ps += (SAMPLES_PER_LINE * 2);
		i++;
		if (i == (SAMPLE_CHUNKS / 2)) {
			ps = samples;
			i = 0;
		}

		if (skipcnt > 5) {
			skipcnt = 0;
			rate = CSR_READ_4(&PIT1, PIT_LDVAL1);
 			rate += 100;
			CSR_WRITE_4(&PIT1, PIT_LDVAL1, rate);
		}

		/*
		 * Now write them to the screen. Note: we write each
                 * pixel and line two and a half times. This scales up the
		 * size of the displayed image, at the expense of pixelating
		 * it But hey, old school pixelated video is elite, right?
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
			write_pixel (buf[p], p & 1);
		}

		for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
			write_pixel (buf[p], p & 1);
		}

		if (j & 1) {
			for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
				write_pixel (buf[p], p & 1);
			}
		}

		for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
			write_pixel (buf[p + FRAMERES_HORIZONTAL], p & 1);
		}

		for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
			write_pixel (buf[p + FRAMERES_HORIZONTAL], p & 1);
		}

		if (j & 1) {
			for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
				write_pixel (buf[p + FRAMERES_HORIZONTAL],
				    p & 1);
			}
		}

		gdisp_lld_write_stop (GDISP);

		j++;
		if (j == FRAMERES_VERTICAL)
			j = 0;

		/* Check for the user requesting exit */

		pe = geventEventWait (&gl, 0);
		if (pe != NULL && pe->type == GEVENT_GWIN_BUTTON)
			break;
	}

	chHeapFree (buf);
	chHeapFree (samples);

	f_close (&f);

	/* Re-initialize the DAC's PIT frequency, in case we changed it. */

	pitDisable (&PIT1, 1);
	CSR_WRITE_4(&PIT1, PIT_LDVAL1,
	    KINETIS_BUSCLK_FREQUENCY / DAC_SAMPLERATE);

out:

	geventDetachSource (&gl, NULL);
	gwinDestroy (ghExitButton);

	return;
}
