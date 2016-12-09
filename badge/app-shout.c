#include "orchard-app.h"
#include "orchard-ui.h"
#include "radio.h"
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
	char * p;

	p = chHeapAlloc (NULL, KW01_PKT_AES_MAXLEN - KW01_PKT_HDRLEN);

	memset (p, 0, KW01_PKT_AES_MAXLEN - KW01_PKT_HDRLEN);

        keyboardUiContext = chHeapAlloc(NULL, sizeof(OrchardUiContext));
	keyboardUiContext->itemlist = (const char **)chHeapAlloc(NULL,
		sizeof(char *) * 2);
	keyboardUiContext->itemlist[0] =
                "Shout something,\npress ENTER when done";
	keyboardUiContext->itemlist[1] = p;
	keyboardUiContext->total = KRADIO1.kw01_maxlen - KW01_PKT_HDRLEN;

	context->instance->ui = getUiByName ("keyboard");
	context->instance->uicontext = keyboardUiContext;
        context->instance->ui->start (context);

	return;
}

static void shout_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	OrchardUiContext * keyboardUiContext;
	const OrchardUi * keyboardUi;
	font_t font;

	keyboardUi = context->instance->ui;
	keyboardUiContext = context->instance->uicontext;

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

			radioSend (&KRADIO1, RADIO_BROADCAST_ADDRESS,
				RADIO_PROTOCOL_SHOUT,
				KW01_PKT_AES_MAXLEN - KW01_PKT_HDRLEN,
				keyboardUiContext->itemlist[1]);

			chHeapFree ((char *)keyboardUiContext->itemlist[1]);

			/* Display a confirmation message */

			font = gdispOpenFont(FONT_FIXED);
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

orchard_app("Radio shout", 0, shout_init, shout_start, shout_event, shout_exit);
