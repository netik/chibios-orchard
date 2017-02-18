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

#include <string.h>

#define FRAMES_PER_SECOND	5
#define FRAME_DELAY_TICKS	(CH_CFG_ST_FREQUENCY / FRAMES_PER_SECOND)

static uint32_t hackers_init(OrchardAppContext *context)
{
	(void)context;
	return (0);
}

static void hackers_start(OrchardAppContext *context)
{
	GWidgetInit wi;
	gdispImage myImage;
 	GHandle ghExitButton;
	GEvent * pe;
	GListener gl;
	char vname[32];
	char aname[32];
	char * track = "hackers";
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
			track = "fbi";
			break;
	}

	/* start the audio */

	chsnprintf (aname, sizeof(aname), "%s/sample.raw", track);
	dacPlay (aname);

	if (strcmp (track, "fbi") == 0) {
		gdispImageOpenFile (&myImage, "fbi.rgb");
		gdispImageDraw (&myImage, 0, 0,
		    myImage.width, myImage.height, 0, 0);
		gdispImageClose (&myImage);
		chThdSleepMilliseconds (3400);
		goto out;
	}

	/* start the video (and hope they stay in sync) */

	i = 1;
	while (1) {
		chsnprintf (vname, sizeof(vname), "%s/out%05d.rgb", track, i);
		if (gdispImageOpenFile (&myImage, vname) !=
		    GDISP_IMAGE_ERR_OK)
			break;
		gdispImageDraw (&myImage, 85, 64,
		    myImage.width, myImage.height, 0, 0);
		gdispImageClose (&myImage);
		chThdSleepMilliseconds (50);
		pe = geventEventWait(&gl, 0);
		if (pe != NULL && pe->type == GEVENT_GWIN_BUTTON)
			break;
		i++;
        }

out:

	dacPlay (NULL);

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

orchard_app("Hackers", /*APP_FLAG_HIDDEN|APP_FLAG_AUTOINIT*/ 0,
	hackers_init, hackers_start, hackers_event, hackers_exit);
