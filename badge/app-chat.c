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
#include "fontlist.h"
#include "sound.h"
#include "ides_gfx.h"

#include <stdlib.h>
#include <string.h>

#define MAX_PEERS	50
#define MAX_PEERMEM	(CONFIG_NAME_MAXLEN + 10)

typedef struct _ChatHandles {
	char *			listitems[MAX_PEERS + 2];
	OrchardUiContext	uiCtx;
	char			txbuf[KW01_PKT_PAYLOADLEN];
	char			rxbuf[MAX_PEERMEM + KW01_PKT_PAYLOADLEN + 3];
	uint8_t 		peers;
	int			peer;
	uint32_t		netid;
} ChatHandles;

extern orchard_app_instance instance;

static int insert_peer (OrchardAppContext * context, KW01_PKT * pkt)
{
	ChatHandles *	p;
	int		i;
	char 		peer[9];
	user *		u;

	p = context->priv;

	/* If the peer list is full, return */

	if (p->peers == (MAX_PEERS + 2))
		return (-1);

	chsnprintf (peer, sizeof(peer), "%08X", pkt->kw01_hdr.kw01_src);

	u = (user *)pkt->kw01_payload;

	for (i = 2; i < p->peers; i++) {
		/* Already an entry for this badge, just return. */
		if (strncmp (p->listitems[i], peer, 8) == 0)
			return (-1);
	}

	/* No match, add a new entry */

	p->listitems[p->peers] = chHeapAlloc (NULL, MAX_PEERMEM);
	chsnprintf (p->listitems[p->peers], MAX_PEERMEM, "%s:%s",
		peer, u->name);
	p->peers++;

	return (0);
}

static void chat_handler(KW01_PKT * pkt)
{
	/* Drop this message if the chat app isn't running. */

	if (instance.app != orchardAppByName ("Radio Chat"))
		return;

	orchardAppRadioCallback (pkt);

        return;
}

static uint32_t chat_init (OrchardAppContext *context)
{
	(void)context;

	/* Install the chat protocol handler */

	if (context == NULL) {
		radioHandlerSet (&KRADIO1,
		    RADIO_PROTOCOL_CHAT, chat_handler);
	}

	return (0);
}

static void chat_start (OrchardAppContext *context)
{
	(void)context;
	return;
}

static void chat_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	OrchardUiContext * uiContext;
	const OrchardUi * ui;
	ChatHandles * p;
	KW01_PKT * pkt;
	userconfig * config;
	OrchardAppEvent e;
	char * c;
	int i;

	/*
	 * In order to avoid any chance of a race condition between
	 * the start end exit routines and the event notifications,
	 * we have to handle context creation and destruction here
	 * in the event handler. This forces the entire app to be
	 * serialized through the event handler thread.
	 */

	if (event->type == appEvent) {
		if (event->app.event == appStart) {
			p = chHeapAlloc(NULL, sizeof(ChatHandles));
			memset (p, 0, sizeof(ChatHandles));
			p->peers = 2;

			if (airplane_mode_check() == true)
				return;

			p->listitems[0] = "Scanning for users...";
			p->listitems[1] = "Exit";

			p->uiCtx.itemlist = (const char **)p->listitems;
			p->uiCtx.total = 1;

			context->instance->ui = getUiByName ("list");
			context->instance->uicontext = &p->uiCtx;
       			context->instance->ui->start (context);

			context->priv = p;
		}
		if (event->app.event == appTerminate) {
			ui = context->instance->ui;
			ui->exit (context);
			p = context->priv;
			for (i = 2; i < p->peers; i++) {
				if (p->listitems[i] != NULL)
					chHeapFree (p->listitems[i]);
			}
			chHeapFree (p);
			context->priv = NULL;
		}
		return;
	}

	p = context->priv;

	/* Shouldn't happen, but check anyway. */

	if (p == NULL)
		return;

	ui = context->instance->ui;
	uiContext = context->instance->uicontext;
	config = getConfig();

	if (event->type == ugfxEvent)
		ui->event (context, event);

	if (event->type == radioEvent && event->radio.pPkt != NULL) {
		pkt = event->radio.pPkt;

		/* A chat message arrived, update the text display. */

		if (pkt->kw01_hdr.kw01_prot == RADIO_PROTOCOL_CHAT &&
		    pkt->kw01_hdr.kw01_dst == config->netid) {
			chsnprintf (p->rxbuf, sizeof(p->rxbuf), "<%s> %s",
			    p->listitems[p->peer], pkt->kw01_payload);
			p->listitems[0] = p->rxbuf;
			/* Tell the keyboard UI to redraw */
			e.type = uiEvent;
			ui->event (context, &e);
		}

		/* A new ping message arrived, update the user list. */

		if (pkt->kw01_hdr.kw01_prot == RADIO_PROTOCOL_PING &&
		    ui == getUiByName ("list") &&
		    insert_peer (context, pkt) != -1) {
			uiContext->selected = p->peers - 1;
			e.type = uiEvent;
			e.ui.flags = uiOK;
			ui->event (context, &e);
		}
	}

	if (event->type == uiEvent &&
	    event->ui.code == uiComplete &&
	    event->ui.flags == uiOK) {
		/*
		 * If this is the list ui exiting, it means we chose a
		 * user to chat with. Now switch to the keyboard ui.
		 */
		if (ui == getUiByName ("list")) {
			/* User chose the "EXIT" selection, bail out. */

			if (uiContext->selected == 0) {
				orchardAppExit ();
				return;
			}

			ui->exit (context);

			/*
			 * This is a little messy. Now that we've chosen
			 * a user to talk to, we need their netid to use
			 * as the destination address for the radio packet
			 * header. We don't want to waste the RAM to save
			 * a netid in addition to the name string, so instead
			 * we recover the netid of the chosen user from
			 * the name string and cache it.
			 */

			p->peer = uiContext->selected + 1;
			c = &p->listitems[p->peer][8];
			*c = '\0';
			p->netid = strtoul (p->listitems[p->peer], NULL, 16);
			*c = ':';

			/*
			 * Now that we chose a peer, release any memory
			 * for the other peers.
			 */

			for (i = 2; i < p->peers; i++) {
				if (i == p->peer)
					continue;
				if (p->listitems[i] != NULL) {
					chHeapFree (p->listitems[i]);
					p->listitems[i] = NULL;
				}
			}

			/* Shout to the user that we want to talk to them. */

			chsnprintf (p->txbuf, sizeof(p->txbuf),
			    "%s WANTS TO CHAT!",
			    config->name);

			radioSend (&KRADIO1, p->netid, RADIO_PROTOCOL_SHOUT,
			    strlen (p->txbuf) + 1, p->txbuf);

			p->listitems[0] = "Type @ to exit";
			memset (p->txbuf, 0, sizeof(p->txbuf));
			p->listitems[1] = p->txbuf;

			/* Load the keyboard UI. */

			p->uiCtx.itemlist = (const char **)p->listitems;
			p->uiCtx.total = KW01_PKT_PAYLOADLEN - 1;
			context->instance->ui = getUiByName ("keyboard");
			context->instance->uicontext = &p->uiCtx;
       			context->instance->ui->start (context);
		} else {
			/* 0xFF means exit chat */
			if (uiContext->total == 0xFF) {
				orchardAppExit ();
			} else {
				p->txbuf[uiContext->selected] = 0x0;
				radioSend (&KRADIO1, p->netid,
				    RADIO_PROTOCOL_CHAT,
				    uiContext->selected + 1, p->txbuf);
				memset (p->txbuf, 0, sizeof(p->txbuf));
				p->uiCtx.total =  KW01_PKT_PAYLOADLEN - 1;
				/* Tell the keyboard UI to redraw */
				e.type = uiEvent;
				e.ui.flags = uiCancel;
				ui->event (context, &e);
			}
		}
	}

	return;
}

static void chat_exit (OrchardAppContext *context)
{
	(void)context;
	return;
}

orchard_app("Radio Chat", "chat.rgb", APP_FLAG_AUTOINIT,
	chat_init, chat_start, chat_event, chat_exit);
