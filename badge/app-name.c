#include "orchard-app.h"
#include "orchard-ui.h"
#include "userconfig.h"

static uint32_t name_init(OrchardAppContext *context)
{
	(void)context;

	/*
	 * We don't want any extra stack space allocated for us.
	 * We'll use the heap.
	 */

	return (0);
}

static void name_start(OrchardAppContext *context)
{
	OrchardUiContext * keyboardUiContext;
	userconfig * config;

	config = getConfig();

	keyboardUiContext = chHeapAlloc(NULL, sizeof(OrchardUiContext));
	keyboardUiContext->itemlist = (const char **)chHeapAlloc(NULL,
            sizeof(char *) * 2);
	keyboardUiContext->itemlist[0] =
		"Type in your name,\npress ENTER when done";
	keyboardUiContext->itemlist[1] = config->name;
	keyboardUiContext->total = CONFIG_NAME_MAXLEN - 1;

    	context->instance->ui = getUiByName("keyboard");
	context->instance->uicontext = keyboardUiContext;
	context->instance->ui->start (context);

	return;
}

static void name_event(OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	const OrchardUi * keyboardUi;
	userconfig * config;
	  
	keyboardUi = context->instance->ui;
	config = getConfig();

	if (event->type == ugfxEvent)
		keyboardUi->event (context, event);

	if (event->type == appEvent && event->app.event == appTerminate)
		return;

	if (event->type == uiEvent) {
		if ((event->ui.code == uiComplete) &&
		    (event->ui.flags == uiOK)) {
			keyboardUi->exit (context);
			configSave (config);
			orchardAppExit ();
		}
	}

	return;
}

static void name_exit(OrchardAppContext *context)
{
    	chHeapFree (context->instance->uicontext->itemlist);
    	chHeapFree (context->instance->uicontext);

	return;
}

orchard_app("Set your name", 0, name_init, name_start, name_event, name_exit);
