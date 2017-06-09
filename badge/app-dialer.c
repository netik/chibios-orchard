#include <string.h>
#include "orchard-app.h"
#include "ides_gfx.h"

#include "dac_lld.h"
#include "dac_reg.h"
#include "pit_lld.h"
#include "pit_reg.h"

#include <stdint.h>
#include <math.h>
#include <string.h>

typedef struct dialer_button {
	coord_t		button_x;
	coord_t		button_y;
	char *		button_text;
	uint16_t	button_freq_a;
	uint16_t	button_freq_b;
	uint16_t	button_samples;
} DIALER_BUTTON;

/*
 * Pre-computed sample counts are based on a sample
 * rate of 32500 Hz.
 */

#define DIALER_SAMPLERATE 32500
#define DIALER_MAXBUTTONS 16

static const DIALER_BUTTON buttons[] =  {
	{ 0,   0,  "1", 1209, 697, 598 },
	{ 60,  0,  "2", 1336, 697, 552 },
	{ 120, 0,  "3", 1477, 697, 506 },
	{ 180, 0,  "A", 1633, 697, 874 }, 

	{ 0,   60,  "4", 1209, 770, 546 },
	{ 60,  60,  "5", 1336, 770, 168 },
	{ 120, 60,  "6", 1477, 770, 462 },
	{ 180, 60,  "B", 1633, 770, 798 }, 

	{ 0,   120, "7", 1209, 852, 494 },
	{ 60,  120, "8", 1336, 852, 456 },
	{ 120, 120, "9", 1477, 852, 418 },
	{ 180, 120, "C", 1633, 852, 38  }, 

	{ 0,   180, "*", 1209, 941, 442 },
	{ 60,  180, "0", 1336, 941, 408 },
        { 120, 180, "#", 1477, 941, 374 },
	{ 180, 180, "D", 1633, 941, 646 }, 
	{ 0,   0,   NULL, 0,   0,   0 }
};

typedef struct _DHandles {
  // GListeners
  GListener glDListener;

  // GHandles
  GHandle ghButtons[16];

} DHandles;

static uint32_t last_ui_time = 0;


double fast_sin(double x)
{
	const double PI	=  3.14159265358979323846264338327950288;
	const double INVPI =  0.31830988618379067153776752674502872;
	const double A	 =  0.00735246819687011731341356165096815;
	const double B	 = -0.16528911397014738207016302002888890;
	const double C	 =  0.99969198629596757779830113868360584;

	int32_t k;
	double x2;

	/* find offset of x from the range -pi/2 to pi/2 */
	k = round (INVPI * x);

	/* bring x into range */
	x -= k * PI;

	/* calculate sine */
	x2 = x * x;
	x = x*(C + x2*(B + A*x2));

	/* if x is in an odd pi count we must flip */
	if (k % 2) x = -x;

	return x;
}

static void
tonePlay (OrchardAppContext *context, uint16_t freqa,
    uint16_t freqb, uint16_t samples)
{
	int i;
	double fract1;
	double fract2;
	double point1;
	double point2;
	double result;
	double pi;
	uint16_t * buf;

	(void)context;

	buf = chHeapAlloc (NULL, samples * sizeof(uint16_t));

	pi = (3.14159265358979323846264338327950288 * 2);

	fract1 = (double)(pi)/(double)(DIALER_SAMPLERATE/freqa);
	fract2 = (double)(pi)/(double)(DIALER_SAMPLERATE/freqb);

	for (i = 0; i < samples; i++) {
		point1 = fract1 * i;
		point2 = fract2 * i;
		result = 2048.0 + (fast_sin (point1) * 2047.0);
		result += 2048.0 + (fast_sin (point2) * 2047.0);
		buf[i] = (uint16_t)round (result / 2.0);
	}

	dacStop ();

	chThdSetPriority (ABSPRIO);

	pitEnable (&PIT1, 1);

	CSR_WRITE_4(&PIT1, PIT_LDVAL1,
 	    KINETIS_BUSCLK_FREQUENCY / DIALER_SAMPLERATE);

	i = 0;
	while (1) {
		dacSamplesPlay (buf, samples);
		i += samples;
		if (i > 10000)
			break;
		dacSamplesWait ();
	}

	pitDisable (&PIT1, 1);

	CSR_WRITE_4(&PIT1, PIT_LDVAL1,
 	    KINETIS_BUSCLK_FREQUENCY / DAC_SAMPLERATE);

	chThdSetPriority (ORCHARD_APP_PRIO);

	chHeapFree (buf);

	return;
}

static uint32_t dialer_init(OrchardAppContext *context) {
	(void)context;

	/*
	 * We don't want any extra stack space allocated for us.
	 * We'll use the heap.
	 */

	return (0);
}

static void draw_keypad(OrchardAppContext *context) {
  DHandles *p;
  GWidgetInit wi;
  const DIALER_BUTTON * b;
  int i = 0;

  p = context->priv;
  gwinWidgetClearInit(&wi);

  wi.g.show = TRUE;
  wi.g.width = 60;
  wi.g.height = 60;
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customStyle = &RedButtonStyle;

  /* Create button widgets */

  for (i = 0; i < DIALER_MAXBUTTONS; i++) {
    b = &buttons[i];
    wi.g.x = b->button_x;
    wi.g.y = b->button_y;
    wi.text = b->button_text;
    p->ghButtons[i] = gwinButtonCreate(0, &wi);
  }

}

static void dialer_start(OrchardAppContext *context) {
  DHandles *p;

  p = chHeapAlloc (NULL, sizeof(DHandles));
  memset(p, 0, sizeof(DHandles));
  context->priv = p;

  gdispClear(Black);
  gdispSetOrientation (0);

  // set up our UI timers and listeners
  last_ui_time = chVTGetSystemTime();

  draw_keypad(context);
  
  geventListenerInit(&p->glDListener);
  gwinAttachListener(&p->glDListener);

  geventRegisterCallback (&p->glDListener,
                          orchardAppUgfxCallback,
                          &p->glDListener);

  orchardAppTimer(context, 500000, true);
}

static void dialer_event(OrchardAppContext *context,
                       const OrchardAppEvent *event) {

  GEvent * pe;
  DHandles *p;
  p = context->priv;
  int i;

  // timer
  if (event->type == timerEvent) {
    // deal with lack of activity
    if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
      orchardAppRun(orchardAppByName("Badge"));
    }
    return;
  }

  // joypad
  if (event->type == keyEvent) {
    if (event->key.flags == keyPress) {
      // any key to exit
      orchardAppExit();
    }
  }

  // ugfx
  if (event->type == ugfxEvent) {
    
    pe = event->ugfx.pEvent;
    last_ui_time = chVTGetSystemTime();

    if (pe->type == GEVENT_GWIN_BUTTON) {
      for (i = 0; i < DIALER_MAXBUTTONS; i++) {
        if (((GEventGWinButton*)pe)->gwin == p->ghButtons[i])
          break;
      }
      if (i != DIALER_MAXBUTTONS) {
        dacWait ();
        tonePlay (context, buttons[i].button_freq_a,
            buttons[i].button_freq_b, buttons[i].button_samples);
      }
    }
  }

}

static void dialer_exit(OrchardAppContext *context) {
  DHandles *p;
  p = context->priv;
  int i;

  for (i = 0; i < DIALER_MAXBUTTONS; i++)
      gwinDestroy(p->ghButtons[i]);

  geventDetachSource (&p->glDListener, NULL);
  geventRegisterCallback (&p->glDListener, NULL, NULL);

  chHeapFree (p);
  context->priv = NULL;

  gdispSetOrientation (GDISP_DEFAULT_ORIENTATION);
}

orchard_app("Dialer", "mplay.rgb", 0, dialer_init,
             dialer_start, dialer_event, dialer_exit, 9999);
