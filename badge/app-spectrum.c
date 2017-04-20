#include "orchard-app.h"
#include "orchard-ui.h"

#include "radio_lld.h"
#include "radio_reg.h"
#include "dac_lld.h"

#include "fontlist.h"

#include "src/gdisp/gdisp_driver.h"

#include "userconfig.h"

#include <string.h>

#define SPAN	10000
#define STEP	1000000

typedef struct spectrum_state {
	uint32_t	span;
	uint32_t	center;
	uint8_t		airplane;
	GListener	gl;
} SpectrumState;

static uint8_t
rssiGet (RADIODriver * radio)
{
	uint8_t rssi;
	int i;

	/* Initiate an RSSI reading */

	radioWrite (radio, KW01_RSSICONF, KW01_RSSICONF_START);

	/* Wait until it finishes. */
 
	for (i = 0; i < KW01_DELAY; i++) {
		rssi = radioRead (radio, KW01_RSSICONF);
		if (rssi & KW01_RSSICONF_DONE)
			break;
        }

	/* Get the result. */

	rssi = radioRead (radio, KW01_RSSIVAL);

	return (rssi);
}

static void
paramShow (uint32_t spanHz, uint32_t centerHz)
{
	font_t font;
	char str[64];

	gdispGFillArea (GDISP, 40, 0, gdispGetWidth () - 80, 80, Teal);

	font = gdispOpenFont (FONT_KEYBOARD);

	chsnprintf(str, sizeof(str), "Start: %dHz",
	    centerHz - (spanHz * (320 / 2)));
  
	gdispDrawStringBox (0, 0, gdispGetWidth(),
	    gdispGetFontMetric(font, fontHeight),
	    str, font, White, justifyCenter);

	chsnprintf(str, sizeof(str), "Center: %dHz", centerHz);
  
	gdispDrawStringBox (0, gdispGetFontMetric(font, fontHeight),
	    gdispGetWidth(), gdispGetFontMetric(font, fontHeight),
	    str, font, White, justifyCenter);

	chsnprintf(str, sizeof(str), "End: %dHz",
	    centerHz + (spanHz * (320 / 2)));
  
	gdispDrawStringBox (0, gdispGetFontMetric(font, fontHeight) * 2,
	    gdispGetWidth(), gdispGetFontMetric(font, fontHeight),
	    str, font, White, justifyCenter);

	chsnprintf(str, sizeof(str), "Span: %dHz", spanHz);
  
	gdispDrawStringBox (0, gdispGetFontMetric(font, fontHeight) * 3,
	    gdispGetWidth(), gdispGetFontMetric(font, fontHeight),
	    str, font, White, justifyCenter);

	gdispCloseFont (font);

	return;
}

static uint32_t
spectrum_init (OrchardAppContext *context)
{
	(void)context;

	/*
	 * We don't want any extra stack space allocated for us.
	 * We'll use the heap.
	 */

	return (0);
}

static void
spectrum_start (OrchardAppContext *context)
{
	SpectrumState * state;
	GSourceHandle gs;
	userconfig * config;

	dacWait ();

	gdispClear (Teal);

	state = chHeapAlloc (NULL, sizeof(SpectrumState));
	context->priv = state;

	state->span = 500000;
	state->center = KW01_CARRIER_FREQUENCY;

	paramShow (state->span, state->center);

	gs = ginputGetMouse (0);
	geventListenerInit (&state->gl);
	geventAttachSource (&state->gl, gs, GLISTEN_MOUSEMETA);
	geventRegisterCallback (&state->gl,
	    orchardAppUgfxCallback, &state->gl);

	orchardAppTimer (context, 1000, FALSE);

	config = getConfig ();
	state->airplane = config->airplane_mode;
	config->airplane_mode = 1;

	return;
}

static void
spectrum_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	SpectrumState * state;
	uint8_t rssi;
	uint32_t startFreq;
	uint32_t stepHz;
	int i, j;

	state = context->priv;

	if (event->type == keyEvent && event->key.flags == keyPress) {
		dacWait ();
		if (event->key.code == keyUp)
			state->span += SPAN;
		if (event->key.code == keyDown) {
			state->span -= SPAN;
			if (state->span == 0)
				state->span = SPAN;
		}	
		if (event->key.code == keyLeft)
			state->center -= STEP;
		if (event->key.code == keyRight)
			state->center += STEP;
		paramShow (state->span, state->center);
	}

	if (event->type == appEvent && event->app.event == appStart)
		orchardAppTimer (context, 1, TRUE);

	if (event->type == ugfxEvent) {
		orchardAppExit ();
		return;
	}

	if (event->type == timerEvent) {
		stepHz = state->span;
		startFreq = state->center;
		startFreq -= stepHz * (gdispGetWidth () / 2);
		GDISP->p.y = 112;
		GDISP->p.cx = 1;
		GDISP->p.cy = gdispGetHeight () - 112;
		for (j = 0; j < gdispGetWidth (); j++) {
			startFreq += stepHz;
			radioFrequencySet (&KRADIO1, startFreq);
			rssi = rssiGet (&KRADIO1);
			rssi >>= 1;
			GDISP->p.x = j;
			gdisp_lld_write_start (GDISP);
			for (i = 0; i < 128; i++) {
				if (rssi == i)
					GDISP->p.color = Yellow;
				else if (rssi > i)
					GDISP->p.color = Teal;
				else
					GDISP->p.color = Navy;
				gdisp_lld_write_color (GDISP);
			}
			gdisp_lld_write_stop (GDISP);
		}
	}

	return;
}

static void
spectrum_exit (OrchardAppContext *context)
{
	SpectrumState * state;
	userconfig * config;

	state = context->priv;

	radioFrequencySet (&KRADIO1, KW01_CARRIER_FREQUENCY);

	config = getConfig ();
	config->airplane_mode = state->airplane;

       	geventDetachSource (&state->gl, NULL);
	geventRegisterCallback (&state->gl, NULL, NULL);
	chHeapFree (state);

	return;
}

orchard_app("RF Spectrum", "spect.rgb", 0, spectrum_init, spectrum_start, spectrum_event, spectrum_exit);
