#include "orchard-app.h"
#include "orchard-ui.h"

static uint32_t notify_init(OrchardAppContext *context)
{
	(void)context;

	radioHandlerSet (&KRADIO1, RADIO_PROTOCOL_SHOUT,
		orchardAppRadioCallback);

	return (0);
}

static void notify_start(OrchardAppContext *context)
{
	(void)context;

	/* Nothing to do yet. */

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

	radioHandlerSet (&KRADIO1, RADIO_PROTOCOL_SHOUT, NULL);

	return;
}

orchard_app("Radio notification", notify_init, notify_start,
		notify_event, notify_exit);

