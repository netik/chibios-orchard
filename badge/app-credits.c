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
#include "ff.h"
#include "ffconf.h"
#include "board_ILI9341.h"

#include "tpm.h"

extern mutex_t enemies_mutex;

#define ILI9341_VSDEF	0x33
#define ILI9341_VSADD	0x37

typedef struct gdisp_image {
        uint8_t                 gdi_id1;
        uint8_t                 gdi_id2;
        uint8_t                 gdi_width_hi;
        uint8_t                 gdi_width_lo;
        uint8_t                 gdi_height_hi;
        uint8_t                 gdi_height_lo;
        uint8_t                 gdi_fmt_hi;
        uint8_t                 gdi_fmt_lo;
} GDISP_IMAGE;

typedef struct _CreditsHandles {
	GHandle		ghExitButton;
	GListener	gl;
	uint8_t		stop;
} CreditsHandles;

static void scrollAreaSet (uint16_t TFA, uint16_t BFA)
{
	acquire_bus (NULL);
	write_index (NULL, ILI9341_VSDEF);
	write_data (NULL, TFA >> 8);
	write_data (NULL, (uint8_t)TFA);
	write_data (NULL, (320 - TFA - BFA) >> 8);
	write_data (NULL, (uint8_t)(320 - TFA - BFA));
	write_data (NULL, BFA >> 8);
	write_data (NULL, (uint8_t)BFA);
	release_bus (NULL);

	return;
}

static void scroll (int lines)
{
	acquire_bus (NULL);
	write_index (NULL, ILI9341_VSADD);
	write_data (NULL, lines >> 8);
	write_data (NULL, (uint8_t)lines);
	release_bus (NULL);

	return;
}

static uint32_t credits_init(OrchardAppContext *context)
{
	(void)context;

	return (0);
}

static void credits_start(OrchardAppContext *context)
{
	int i;
	int pos;
	FIL f;
 	UINT br;
	GDISP_IMAGE hdr;
	uint16_t h;
	pixel_t * buf;
	CreditsHandles * p;
	GWidgetInit wi;
	orientation_t o;


	if (f_open (&f, "credits.rgb", FA_READ) != FR_OK) {
		orchardAppExit ();
		return;
	}

	p = chHeapAlloc (NULL, sizeof(CreditsHandles));
	context->priv = p;

	pwmFileThreadPlay ("mario");

	gwinWidgetClearInit (&wi);
	wi.g.show = TRUE;
	wi.g.width = gdispGetWidth();
	wi.g.height = gdispGetHeight();
  	wi.g.y = 0;
  	wi.g.x = 0;
  	wi.text = "";
 	wi.customDraw = noRender;

	p->ghExitButton = gwinButtonCreate (NULL, &wi);
	geventListenerInit (&p->gl);
	gwinAttachListener (&p->gl);
	geventRegisterCallback (&p->gl, orchardAppUgfxCallback, &p->gl);

	p->stop = 0;

	f_read (&f, &hdr, sizeof(hdr), &br);
	h = hdr.gdi_height_hi << 8 | hdr.gdi_height_lo;

	buf = chHeapAlloc (NULL, sizeof(pixel_t) * gdispGetHeight ());

	o = gdispGetOrientation();

	scrollAreaSet (0, 0);
	pos = gdispGetWidth() - 1;
	for (i = 0; i < h; i++) {
		if (p->stop)
			break;
		f_read (&f, buf, sizeof(pixel_t) * gdispGetHeight (), &br);
		gdispBlitAreaEx (pos, 0, 1, gdispGetHeight (), 0, 0, 1, buf);
		pos++;
		if (pos == gdispGetWidth ())
			pos = 0;
		scroll (o == GDISP_ROTATE_90 ?
			(gdispGetWidth() - 1) - pos : pos);
		chThdSleepMilliseconds (15);
	}

	scroll (0);
	chHeapFree (buf);
	f_close (&f);

	if (p->stop == 0)
		chThdSleepMilliseconds (800);

	pwmFileThreadPlay (NULL);

	orchardAppExit ();

	return;
}

void credits_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	CreditsHandles * p;

        if (event->type == ugfxEvent) {
		p = context->priv;
		p->stop = 1;
	}

	return;
}

static void credits_exit(OrchardAppContext *context)
{
	CreditsHandles * p;

	p = context->priv;

	if (p != NULL) {
		geventRegisterCallback (&p->gl, NULL, NULL);
		geventDetachSource (&p->gl, NULL);
		gwinDestroy (p->ghExitButton);

		chHeapFree (p);
		context->priv = NULL;
	}

	return;
}

orchard_app("Credits", 0, credits_init, credits_start,
		credits_event, credits_exit);
