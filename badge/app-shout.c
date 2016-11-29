#include "orchard-app.h"
#include "orchard-ui.h"
#include "radio.h"
#include "userconfig.h"
#include "fontlist.h"
#include "sound.h"

#include <string.h>

static uint32_t shout_init (OrchardAppContext *context)
{
	(void)context;

	/*
	 * We don't want any extra stack space allocated for us.
	 * We'll use the heap.
	 */

	return (0);
}

static void shout_start (OrchardAppContext *context)
{
	OrchardUiContext * keyboardUiContext;
	userconfig * config;
	KW01_PKT * pkt;

	/*
	 * Save some RAM by re-using the payload buffer in the
	 * radio context handle.
	 */

	pkt = &KRADIO1.kw01_pkt;

	memset (pkt->kw01_payload, 0, sizeof(pkt->kw01_payload));

	/* Copy the netid into the first four bytes of the payload buffer. */

	config = getConfig ();
	memcpy (pkt->kw01_payload, &config->netid, sizeof(config->netid));

        keyboardUiContext = chHeapAlloc(NULL, sizeof(OrchardUiContext));
	keyboardUiContext->itemlist = (const char **)chHeapAlloc(NULL,
		sizeof(char *) * 2);
	keyboardUiContext->itemlist[0] =
                "Shout something, press ENTER when done";
	keyboardUiContext->itemlist[1] = (char *)(pkt->kw01_payload +
		sizeof(config->netid));
	keyboardUiContext->total = KRADIO1.kw01_maxlen - KW01_PKT_HDRLEN -
		sizeof(config->netid);

	context->instance->ui = getUiByName ("keyboard");
	context->instance->uicontext = keyboardUiContext;
        context->instance->ui->start (context);

	return;
}

static void shout_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	const OrchardUi * keyboardUi;
	KW01_PKT * pkt;
	font_t font;

	pkt = &KRADIO1.kw01_pkt;

	keyboardUi = context->instance->ui;

	if (event->type == ugfxEvent)
		keyboardUi->event (context, event);

	if (event->type == appEvent && event->app.event == appTerminate)
		return;

	if (event->type == uiEvent) {
		if ((event->ui.code == uiComplete) &&
		    (event->ui.flags == uiOK)) {

			/* Terminate UI */

			keyboardUi->exit (context);

			/* Send the message */

			radioSend (&KRADIO1, 255, RADIO_PROTOCOL_SHOUT,
				KRADIO1.kw01_maxlen - KW01_PKT_HDRLEN,
				pkt->kw01_payload);

			/* Display a confirmation message */

			font = gdispOpenFont(FONT_SM);
			gdispDrawStringBox (0, (gdispGetHeight() / 2) -
				gdispGetFontMetric(font, fontHeight),
				gdispGetWidth(),
				gdispGetFontMetric(font, fontHeight),
				"Shout sent!", font, Red, justifyCenter);
			gdispCloseFont (font);
			playVictory ();

			/* Wait for a second, then exit */

			chThdSleepMilliseconds (1000);

			orchardAppExit ();
		}
	}
	return;
}

static void shout_exit (OrchardAppContext *context)
{
	chHeapFree (context->instance->uicontext->itemlist);
	chHeapFree (context->instance->uicontext);

	return;
}

orchard_app("Radio shout", shout_init, shout_start, shout_event, shout_exit);
