#include "orchard-app.h"
#include "orchard-ui.h"

#include "ff.h"
#include "ffconf.h"

#include <string.h>

struct OrchardUiContext keyboardUiContext;

static char name[32];

static uint32_t name_init(OrchardAppContext *context)
{
	(void)context;

	return (0);
}

static void name_start(OrchardAppContext *context)
{
	const OrchardUi * keyboardUi;

	keyboardUi = getUiByName("keyboard");
	keyboardUiContext.itemlist = (const char **)name;
	keyboardUiContext.total = 32;
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
	FIL fp;
	UINT bw;

	(void)context;

	if (event->type == appEvent && event->app.event == appTerminate)
		return;

	if (event->type == uiEvent) {
		if ((event->ui.code == uiComplete) &&
		    (event->ui.flags == uiOK)) {
			keyboardUi = getUiByName("keyboard");
			keyboardUi->exit (context);
			chprintf(stream, "NAME: %s\r\n", name);
			f_open (&fp, "USER.TXT", FA_READ | FA_WRITE |
				FA_OPEN_ALWAYS);
			f_write (&fp, name, strlen(name), &bw);
			f_close (&fp);
			orchardAppExit ();
		}
	}

	return;
}

static void name_exit(OrchardAppContext *context)
{
	(void)context;
	return;
}

orchard_app("Set your name", name_init, name_start, name_event, name_exit);


