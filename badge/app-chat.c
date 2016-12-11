#include "orchard-app.h"
#include "orchard-ui.h"
#include "radio.h"
#include "fontlist.h"
#include "sound.h"

#include <string.h>

#define CHAT_STS_COLD		0x01
#define CHAT_STS_CHATTING	0x02
#define CHAT_STS_GOT_MSG	0x04

static uint8_t chat_sts = CHAT_STS_COLD;

typedef struct _ChatHandles {
	char *			listitems[MAX_ENEMIES + 1];
	OrchardUiContext	uiCtx;
	char			txbuf[KW01_PKT_PAYLOADLEN];
	char			rxbuf[KW01_PKT_PAYLOADLEN];
	uint32_t		sender;
} ChatHandles;

static ChatHandles * pHandles;

static void chat_handler(KW01_PKT * pkt)
{
	orchardAppRadioCallback (pkt);
        return;
}

static uint32_t chat_init (OrchardAppContext *context)
{
	(void)context;

	if (context == NULL)
		radioHandlerSet (&KRADIO1, RADIO_PROTOCOL_CHAT, chat_handler);

	return (0);
}

static void chat_start (OrchardAppContext *context)
{
	ChatHandles * p;
	font_t font;
	user ** enemies;
	userconfig * config;
	int i;

	if (chat_sts == CHAT_STS_COLD) {
		p = chHeapAlloc(NULL, sizeof(ChatHandles));
		context->priv = p;
		pHandles = p;
		gdispClear (Black);

		p->listitems[0] = "Select a badge/user";

		enemies = enemiesGet();
		for (i = 0; i < MAX_ENEMIES; i++) {
			if (enemies[i] == NULL)
				break;
			p->listitems[i+ 1] = enemies[i]->name;
		}

		if (i == 0) {
			font = gdispOpenFont(FONT_FIXED);
			gdispDrawStringBox (0, (gdispGetHeight() / 2) -
			    gdispGetFontMetric(font, fontHeight),
			    gdispGetWidth(),
			    gdispGetFontMetric (font, fontHeight),
			    "No badges found!", font, Red, justifyCenter);
			gdispCloseFont (font);
			chThdSleepMilliseconds (4000);
			orchardAppExit ();
			return;
		}

		p->uiCtx.itemlist = (const char **)p->listitems;
		p->uiCtx.total = i;

		context->instance->ui = getUiByName ("list");
		context->instance->uicontext = &p->uiCtx;
        	context->instance->ui->start (context);
	} else {
		context->priv = pHandles;
		p = pHandles;

		config = getConfig();

		chsnprintf (p->txbuf, sizeof(p->txbuf), "%s WANTS TO CHAT!",
			config->name);

		radioSend (&KRADIO1, p->sender, RADIO_PROTOCOL_SHOUT,
			strlen(p->txbuf), p->txbuf);

		p->listitems[0] = "Type @ to exit";
		memset (p->txbuf, 0, sizeof(p->txbuf));
		p->listitems[1] = p->txbuf;

		p->uiCtx.itemlist = (const char **)p->listitems;
		p->uiCtx.total = KW01_PKT_PAYLOADLEN;

		context->instance->ui = getUiByName ("keyboard");
		context->instance->uicontext = &p->uiCtx;
        	context->instance->ui->start (context);
	}

	return;
}

static void chat_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	OrchardUiContext * listUiContext;
	const OrchardUi * listUi;
	ChatHandles * p;
	KW01_PKT * pkt;
	user ** enemies;

	p = context->priv;
	listUi = context->instance->ui;
	listUiContext = context->instance->uicontext;

	if (event->type == ugfxEvent)
		listUi->event (context, event);

	if (event->type == radioEvent && event->radio.pPkt != NULL) {
		/* Terminate UI */
		pkt = event->radio.pPkt;
		if (pkt->kw01_hdr.kw01_prot == RADIO_PROTOCOL_CHAT &&
		    pkt->kw01_hdr.kw01_src == p->sender) {
			listUi->exit (context);
			memset (p->rxbuf, 0, sizeof(p->rxbuf));
			memcpy (p->rxbuf, pkt->kw01_payload, pkt->kw01_length);
			p->listitems[0] = p->rxbuf;
			listUi->start (context);
		}
	}

	if (event->type == appEvent && event->app.event == appTerminate)
		return;

	if (event->type == uiEvent) {
		if ((event->ui.code == uiComplete) &&
		    (event->ui.flags == uiOK)) {

			if (context->instance->ui == getUiByName ("list")) {
				listUi->exit (context);
				enemies = enemiesGet();
				p->sender =
					enemies[listUiContext->total]->netid;
				chat_sts = CHAT_STS_CHATTING;
				orchardAppRun(orchardAppByName ("Radio Chat"));
			} else {
				/* 0xFF means exit chat */
				if (listUiContext->total == 0xFF) {
					listUi->exit (context);
					chat_sts = CHAT_STS_COLD;
					orchardAppExit ();
				} else {
					listUi->exit (context);
					radioSend (&KRADIO1, p->sender,
					    RADIO_PROTOCOL_CHAT,
					    strlen (p->txbuf), p->txbuf);
					memset (p->txbuf, 0, sizeof(p->txbuf));
					p->uiCtx.total = KW01_PKT_PAYLOADLEN;
					listUi->start (context);
				}
			}
		}
	}

	return;
}

static void chat_exit (OrchardAppContext *context)
{
	ChatHandles * p;

	if (chat_sts == CHAT_STS_COLD) {
		p = context->priv;
		chHeapFree (p);
	}

	return;
}

orchard_app("Radio Chat", APP_FLAG_AUTOINIT,
	chat_init, chat_start, chat_event, chat_exit);
