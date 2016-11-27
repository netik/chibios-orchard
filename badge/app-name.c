#include "orchard-app.h"
#include "orchard-ui.h"

#include "ff.h"
#include "ffconf.h"

#include <string.h>

#include "userconfig.h"

struct OrchardUiContext keyboardUiContext;

static char name[CONFIG_NAME_MAXLEN];

static uint32_t name_init(OrchardAppContext *context)
{
	(void)context;

	return (0);
}

static void name_start(OrchardAppContext *context)
{
	const OrchardUi * keyboardUi;

	memset (name, 0, sizeof(name));

	keyboardUi = getUiByName("keyboard");
	keyboardUiContext.itemlist = (const char **)chHeapAlloc(NULL,
            sizeof(char *) * 2);
	keyboardUiContext.itemlist[0] =
		"Type in your name, press ENTER when done";
	keyboardUiContext.itemlist[1] = name;
	keyboardUiContext.total = CONFIG_NAME_MAXLEN-2;
	if (keyboardUi != NULL) {
    		context->instance->uicontext = &keyboardUiContext;
    		context->instance->ui = keyboardUi;
	}

	keyboardUi->start (context);

	return;
}

void name_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	const OrchardUi * keyboardUi;
	userconfig *config = getConfig();
	  
	(void)context;

	keyboardUi = getUiByName("keyboard");

	if (event->type == ugfxEvent)
		keyboardUi->event (context, event);

	if (event->type == appEvent && event->app.event == appTerminate)
		return;

	if (event->type == uiEvent) {
		if ((event->ui.code == uiComplete) &&
		    (event->ui.flags == uiOK)) {
			keyboardUi->exit (context);

			strncpy(config->name, name, CONFIG_NAME_MAXLEN);
			configSave(config);

			orchardAppExit ();
		}
	}

	return;
}

static void name_exit(OrchardAppContext *context)
{
	(void)context;

	chHeapFree (keyboardUiContext.itemlist);

	return;
}

orchard_app("Set your name", name_init, name_start, name_event, name_exit);


