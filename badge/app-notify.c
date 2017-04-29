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

#include "radio_lld.h"
#include "sound.h"
#include "fontlist.h"

#include <string.h>

extern uint8_t shout_received;
extern uint8_t shout_ok;

static char buf[KW01_PKT_MAXLEN];
static uint32_t sender;

static void notify_handler(KW01_PKT * pkt)
{
	userconfig * config;

	config = getConfig();

	if (pkt->kw01_hdr.kw01_dst != RADIO_BROADCAST_ADDRESS &&
	    pkt->kw01_hdr.kw01_dst != config->netid)
		return;

	memcpy (buf, pkt->kw01_payload, pkt->kw01_length);
	sender = pkt->kw01_hdr.kw01_src;

	shout_received = 1;
	if (shout_ok == 1)
		orchardAppRun (orchardAppByName("Radio notification"));

	return;
}

static uint32_t notify_init(OrchardAppContext *context)
{
	(void)context;

	/* This should only happen for auto-init */

	if (context == NULL) {
		radioHandlerSet (&KRADIO1, RADIO_PROTOCOL_SHOUT,
			notify_handler);
		shout_received = 0;
	}

	return (0);
}

static void notify_start(OrchardAppContext *context)
{
	font_t font;
	char id[22];

	(void)context;

	gdispClear (Black);

	font = gdispOpenFont(FONT_FIXED);
	chsnprintf (id, 22, "Badge %x says:", sender);

	gdispDrawStringBox (0, (gdispGetHeight() / 2) -
	    gdispGetFontMetric(font, fontHeight),
	    gdispGetWidth(), gdispGetFontMetric(font, fontHeight),
	    id, font, White, justifyCenter);
	gdispDrawStringBox (0, (gdispGetHeight() / 2), 
	    gdispGetWidth(), gdispGetFontMetric(font, fontHeight),
	    buf, font, Green, justifyCenter);
	gdispCloseFont (font);
	playVictory ();

	chThdSleepMilliseconds (5000);

	orchardAppExit ();

	return;
}

void notify_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	(void)context;
	(void)event;

	return;
}

static void notify_exit(OrchardAppContext *context)
{
	(void)context;

	return;
}

orchard_app("Radio notification", NULL, APP_FLAG_HIDDEN|APP_FLAG_AUTOINIT,
	notify_init, notify_start, notify_event, notify_exit, 9999);
