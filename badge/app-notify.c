#include "orchard-app.h"
#include "orchard-ui.h"

#include "radio.h"
#include "sound.h"
#include "fontlist.h"

#include <string.h>

extern uint8_t shout_received;
extern uint8_t shout_ok;

static char buf[KW01_PKT_MAXLEN];
static uint32_t sender;

static void notify_handler(KW01_PKT * pkt)
{
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
	    buf, font, Blue, justifyCenter);
	gdispCloseFont (font);
	playVictory ();

	chThdSleepMilliseconds (4000);

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

orchard_app("Radio notification", APP_FLAG_HIDDEN|APP_FLAG_AUTOINIT,
	notify_init, notify_start, notify_event, notify_exit);
