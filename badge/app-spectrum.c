#include "orchard-app.h"
#include "orchard-ui.h"

#include "radio_lld.h"
#include "radio_reg.h"

#include "fontlist.h"

#include "src/gdisp/gdisp_driver.h"

#include <string.h>

#define SPAN	500000

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
 		chThdSleep (1);
        }
        
	/* Get the result. */

	rssi = radioRead (radio, KW01_RSSIVAL);

	return (rssi);
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
	GSourceHandle gs;
	GListener * gl;
	font_t font;
	char str[64];

	gdispClear (Black);

	font = gdispOpenFont (FONT_KEYBOARD);

	chsnprintf(str, sizeof(str), "Start: %dHz",
	    KW01_CARRIER_FREQUENCY - (SPAN * (320 / 2)));
  
	gdispDrawStringBox (0, 0, gdispGetWidth(),
	    gdispGetFontMetric(font, fontHeight),
	    str, font, White, justifyCenter);

	chsnprintf(str, sizeof(str), "Center: %dHz", KW01_CARRIER_FREQUENCY);
  
	gdispDrawStringBox (0, gdispGetFontMetric(font, fontHeight),
	    gdispGetWidth(), gdispGetFontMetric(font, fontHeight),
	    str, font, White, justifyCenter);

	chsnprintf(str, sizeof(str), "End: %dHz",
	    KW01_CARRIER_FREQUENCY + (SPAN * (320 / 2)));
  
	gdispDrawStringBox (0, gdispGetFontMetric(font, fontHeight) * 2,
	    gdispGetWidth(), gdispGetFontMetric(font, fontHeight),
	    str, font, White, justifyCenter);

	gdispCloseFont (font);

	gl = chHeapAlloc (NULL, sizeof(GListener));
	memset (gl, 0, sizeof(GListener));
	context->priv = gl;

	gs = ginputGetMouse (0);
	geventListenerInit (gl);
	geventAttachSource (gl, gs, GLISTEN_MOUSEMETA);
	geventRegisterCallback (gl, orchardAppUgfxCallback, gl);

	orchardAppTimer (context, 1000, FALSE);

	radioWrite (&KRADIO1, KW01_LNA, KW01_ZIN_200OHM|KW01_GAIN_G1);

	return;
}

static void
spectrum_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	uint8_t rssi;
	uint32_t startFreq;
	uint32_t stepHz;
	int i, j;

	if (event->type == appEvent && event->app.event == appStart)
		orchardAppTimer (context, 1, TRUE);

	if (event->type == ugfxEvent) {
		orchardAppExit ();
		return;
	}

	if (event->type == timerEvent) {
		stepHz = SPAN;
		startFreq = KW01_CARRIER_FREQUENCY;
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
					GDISP->p.color = White;
				else if (rssi > i)
					GDISP->p.color = Black;
				else
					GDISP->p.color = Blue;
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
	GListener * gl;

	gl = context->priv;

	if (gl != NULL) {
        	geventDetachSource (gl, NULL);
		geventRegisterCallback (gl, NULL, NULL);
		chHeapFree (gl);
	}

	radioFrequencySet (&KRADIO1, KW01_CARRIER_FREQUENCY);
	radioWrite (&KRADIO1, KW01_LNA, KW01_ZIN_200OHM|KW01_GAIN_AGC);

	return;
}

orchard_app("Radio Spectrum Display", 0, spectrum_init, spectrum_start, spectrum_event, spectrum_exit);
