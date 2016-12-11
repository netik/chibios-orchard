#include "orchard-app.h"
#include "orchard-events.h"
#include "orchard-ui.h"
#include "tpm.h"

#include <string.h>

#include "fontlist.h"

typedef struct _ListHandles {
	GListener	gl;
	GHandle		ghList;
	font_t		font;
} ListHandles;

static void list_start (OrchardAppContext *context)
{
	ListHandles * p;
	OrchardUiContext * ctx;
	GWidgetInit wi;
	unsigned int i;

	ctx = context->instance->uicontext;
	ctx->priv = chHeapAlloc (NULL, sizeof(ListHandles));
	p = ctx->priv;

	p->font = gdispOpenFont (FONT_FIXED);
	gwinSetDefaultFont (p->font);

	gdispClear (Blue);

	gdispDrawStringBox (0, 0, gdispGetWidth(),
	    gdispGetFontMetric(p->font, fontHeight),
	    ctx->itemlist[0],
	    p->font, White, justifyCenter);

	/* Draw the console/text entry widget */

	gwinWidgetClearInit (&wi);
	wi.g.show = TRUE;
	wi.g.x = 0;
	wi.g.y = gdispGetFontMetric (p->font, fontHeight);
	wi.g.width = gdispGetWidth();
	wi.g.height = gdispGetHeight() -
	    gdispGetFontMetric (p->font, fontHeight);
	p->ghList = gwinListCreate (0, &wi, FALSE);

	/* Add users to the list. */

	for (i = 0; i < ctx->total; i++)
		gwinListAddItem (p->ghList, ctx->itemlist[i + 1], FALSE);

	/* Wait for events */
	geventListenerInit (&p->gl);
	gwinAttachListener (&p->gl);

	geventRegisterCallback (&p->gl, orchardAppUgfxCallback, &p->gl);

	return;
}

static void list_event(OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	GEvent * pe;
	GEventGWinList * ple;
	OrchardUiContext * ctx;

	ctx = context->instance->uicontext;

	if (event->type != ugfxEvent)
		return;

	pe = event->ugfx.pEvent;

	if (pe->type == GEVENT_GWIN_LIST) {
		ple = (GEventGWinList *)pe;
		ctx->total = ple->item;
		chEvtBroadcast (&ui_completed);
	}

	return;  
}

static void list_exit(OrchardAppContext *context)
{
	ListHandles * p;

	p = context->instance->uicontext->priv;

	geventRegisterCallback (&p->gl, NULL, NULL);
	geventDetachSource (&p->gl, NULL);
	gwinDestroy (p->ghList);

	gdispCloseFont (p->font);

	gdispClear (Black);

	chHeapFree (p);
	context->instance->uicontext->priv = NULL;

	return;
}

orchard_ui("list", list_start, list_event, list_exit);

