#include "orchard-app.h"
#include "orchard-events.h"
#include "orchard-ui.h"
#include "tpm.h"

#include <string.h>

#include "fontlist.h"

/*
 * We need two widgets: a console and the keyboard.
 * We also need one listener object for detecting keypresses
 */

static GListener	gl;
static GHandle		ghConsole;
static GHandle		ghKeyboard;
static font_t		font;
static uint8_t		pos;

static uint8_t handle_input (char * name, uint8_t * pos, uint8_t max,
	GEventKeyboard * pk, GConsoleObject * cons)
{
	uint8_t r;

	if (pk->bytecount == 0)
		return (0);

	cons->cx = 0;
	gdispGFillArea (ghConsole->display, cons->cx, cons->cy,
	    gdispGetWidth(), gdispGetFontMetric (font, fontHeight), Black);

	switch (pk->c[0]) {
	case GKEY_BACKSPACE:
		(*pos)--;
		name[*pos] = 0x0;
		gwinPrintf (ghConsole, "%s_", name);
		r = 0;
		break;
	case GKEY_ENTER:
		r = 1;
		break;
	default:
		if (*pos == max) {
			pwmToneStart (400);
			chThdSleepMilliseconds (100);
			pwmToneStop ();
		} else {
			name[*pos] = pk->c[0];
			(*pos)++;
		}
		gwinPrintf (ghConsole, "%s_", name);
		r = 0;
		break;
	}

	return (r);
}

static void keyboard_start (OrchardAppContext *context)
{
	GWidgetInit wi;

	(void)context;

	font = gdispOpenFont (FONT_BITMAP);
	gwinSetDefaultFont (font);

	/* Draw the console/text entry widget */

	gwinWidgetClearInit (&wi);
	wi.g.show = FALSE;
	wi.g.x = 0;
	wi.g.y = 0;
	wi.g.width = gdispGetWidth();
	wi.g.height = gdispGetHeight() / 2;
	ghConsole = gwinConsoleCreate (0, &wi.g);
	gwinSetColor (ghConsole, Yellow);
	gwinSetBgColor (ghConsole, Black);
	gwinShow (ghConsole);
	gwinClear (ghConsole);
	gwinPrintf (ghConsole, "%s\n\n",
		context->instance->uicontext->itemlist[0]);

	/* Draw the keyboard widget */
	wi.g.show = FALSE;
	wi.g.x = 0;
	wi.g.y = gdispGetHeight() / 2;
	wi.g.width = gdispGetWidth();
	wi.g.height = gdispGetHeight() / 2;
	ghKeyboard = gwinKeyboardCreate (0, &wi);
	gwinSetStyle (ghKeyboard, &WhiteWidgetStyle);
	gwinShow (ghKeyboard);

	/* Wait for events */
	geventListenerInit (&gl);
	gwinAttachListener (&gl);

	geventAttachSource (&gl, gwinKeyboardGetEventSource (ghKeyboard),
		GLISTEN_KEYTRANSITIONS | GLISTEN_KEYUP);

	geventRegisterCallback (&gl, orchardAppUgfxCallback, &gl);

	pos = 0;

	gwinPutChar (ghConsole, '_');

	return;
}

static void keyboard_event(OrchardAppContext *context,
	const OrchardAppEvent *event)
{
 	GEventKeyboard * pk;
	GConsoleObject * cons;
	GEvent * pe;
	char * name;
	uint8_t max;

	(void)context;

	if (event->type != ugfxEvent)
		return;

	pe = event->ugfx.pEvent;

	if (pe->type == GEVENT_KEYBOARD)
		pk = (GEventKeyboard *)pe;
	else
		return;

	name = (char *)context->instance->uicontext->itemlist[1];
	max = context->instance->uicontext->total;
	cons = (GConsoleObject *)ghConsole;

	if (handle_input (name, &pos, max, pk, cons) != 0)
		chEvtBroadcast(&ui_completed);

	return;  
}

static void keyboard_exit(OrchardAppContext *context)
{
	(void)context;

	geventRegisterCallback (&gl, NULL, NULL);
	geventDetachSource (&gl, NULL);
	gwinDestroy (ghConsole);
	gwinDestroy (ghKeyboard);
	gdispCloseFont (font);

	gdispClear (Black);

	return;
}

orchard_ui("keyboard", keyboard_start, keyboard_event, keyboard_exit);

