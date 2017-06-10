#include "orchard-app.h"
#include "ides_gfx.h"
#include "fontlist.h"

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

#define DIALER_MAXBUTTONS 19
#define DIALER_BLUEBOX
#define DIALER_SAMPLERATE_SILVERBOX 32500
#define DIALER_SAMPLERATE_BLUEBOX 31200
#define DIALER_SAMPLERATE_2600 20000

static const DIALER_BUTTON buttons[] =  {
	{ 0,   0,   "1", 1209, 697, 598 },
	{ 60,  0,   "2", 1336, 697, 552 },
	{ 120, 0,   "3", 1477, 697, 506 },
	{ 180, 0,   "A", 1633, 697, 874 }, 

	{ 0,   60,  "4", 1209, 770, 546 },
	{ 60,  60,  "5", 1336, 770, 168 },
	{ 120, 60,  "6", 1477, 770, 462 },
	{ 180, 60,  "B", 1633, 770, 798 }, 

	{ 0,   120, "7", 1209, 852, 494 },
	{ 60,  120, "8", 1336, 852, 456 },
	{ 120, 120, "9", 1477, 852, 418 },
	{ 180, 120, "C", 1633, 852, 304 }, 

	{ 0,   180, "*", 1209, 941, 442 },
	{ 60,  180, "0", 1336, 941, 408 },
        { 120, 180, "#", 1477, 941, 374 },
	{ 180, 180, "D", 1633, 941, 646 },

	{ 0,   240, "2600",  2600, 2600, 497  },
#ifdef DIALER_BLUEBOX
	{ 130, 240, "Blue Box", 0, 0,    0    },
#else
	{ 130, 240, "",      0,    0,    0    },
#endif
	{ 70,  280, "Exit",  0,    0,    0    },
#ifdef DIALER_BLUEBOX
	{ 0,   0,   "1",     700,  900,  748  },
	{ 60,  0,   "2",     700,  1100, 616  },
	{ 120, 0,   "3",     900,  1100, 952  },
	{ 180, 0,   "KP1",   1100, 1700, 504  }, 

	{ 0,   60,  "4",     700,  1300, 528  },
	{ 60,  60,  "5",     900,  1300, 816  },
	{ 120, 60,  "6",     1100, 1300, 504  },
	{ 180, 60,  "KP2",   1300, 1700, 504  }, 

	{ 0,   120, "7",     700,  1500, 660  },
	{ 60,  120, "8",     900,  1500, 680  },
	{ 120, 120, "9",     1100, 1500, 560  },
	{ 180, 120, "ST",    1500, 1700, 540  }, 

	{ 0,   180, "Code1", 700,  1700, 792  },
	{ 60,  180, "0",     1300, 1500, 600  },
        { 120, 180, "Code2", 900,  1700, 612  },
	{ 180, 180,  "",     0,    0,    0    },

	{ 0,   240, "2600",  2600, 2600, 497  },
	{ 130, 240, "Silver Box",0,0,    0    },
	{ 70,  280, "Exit",  0,    0,    0    }
#endif
};

typedef struct _DHandles {
  // GListeners
  GListener glDListener;

  // GHandles
  GHandle ghButtons[DIALER_MAXBUTTONS];
#ifdef DIALER_BLUEBOX
  int mode;
#endif

  font_t font;

  orientation_t o;
  uint8_t sound;

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
tonePlay (DHandles * p, uint16_t freqa, uint16_t freqb, uint16_t samples)
{
	int i;
	double fract1;
	double fract2;
	double point1;
	double point2;
	double result;
	double pi;
	uint16_t * buf;
	uint16_t samplerate;

	if (freqa == 0 && freqa == 0)
		return;

	if (freqa == 2600)
		samplerate = DIALER_SAMPLERATE_2600;
	else if (p->mode)
		samplerate = DIALER_SAMPLERATE_BLUEBOX;
	else
		samplerate = DIALER_SAMPLERATE_SILVERBOX;

	buf = chHeapAlloc (NULL, samples * sizeof(uint16_t));

	pi = (3.14159265358979323846264338327950288 * 2);

	fract1 = (double)(pi)/(double)(samplerate/freqa);
	fract2 = (double)(pi)/(double)(samplerate/freqb);

	for (i = 0; i < samples; i++) {
		point1 = fract1 * i;
		result = 2048.0 + (fast_sin (point1) * 2047.0);
		point2 = fract2 * i;
		result += 2048.0 + (fast_sin (point2) * 2047.0);
		result /= 2.0;
		buf[i] = (uint16_t)round (result);
	}

	chThdSetPriority (NORMALPRIO + 1);

	pitEnable (&PIT1, 1);

	CSR_WRITE_4(&PIT1, PIT_LDVAL1, KINETIS_BUSCLK_FREQUENCY / samplerate);

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
  int i;
  int off = 0;

  p = context->priv;
  gwinWidgetClearInit(&wi);

  wi.g.show = TRUE;
  wi.g.width = 60;
  wi.g.height = 60;
  wi.customDraw = gwinButtonDraw_Normal;

  /* Create button widgets */
#ifdef DIALER_BLUEBOX
  if (p->mode) {
    wi.customStyle = &DarkPurpleFilledStyle;
    off = DIALER_MAXBUTTONS;
  } else
#endif
    wi.customStyle = &RedButtonStyle;

  for (i = 0; i < DIALER_MAXBUTTONS; i++) {
    if (i > 15) {
      wi.g.width = 110;
      wi.g.height = 40;
    }
    b = &buttons[i + off];
    wi.g.x = b->button_x;
    wi.g.y = b->button_y;
    wi.text = b->button_text;
    p->ghButtons[i] = gwinButtonCreate (0, &wi);
  }

}

static void destroy_keypad(OrchardAppContext *context) {
  DHandles *p;
  int i;
  p = context->priv;

  for (i = 0; i < DIALER_MAXBUTTONS; i++)
      gwinDestroy (p->ghButtons[i]);

  return;
}

static void dialer_start(OrchardAppContext *context) {
  DHandles * p;
  userconfig * config;

  p = chHeapAlloc (NULL, sizeof(DHandles));
  memset(p, 0, sizeof(DHandles));
  context->priv = p;
  p->font = gdispOpenFont (FONT_KEYBOARD);

  gdispClear(Black);
  p->o = gdispGetOrientation ();
  gdispSetOrientation (0);
  gwinSetDefaultFont (p->font);

  dacStop ();

  config = getConfig ();
  p->sound = config->sound_enabled;
  config->sound_enabled = 0;

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
      if (i == 18) {
        orchardAppExit();
      } else if (i == 17) {
#ifdef DIALER_BLUEBOX
        p->mode = !p->mode;
        destroy_keypad (context);
        draw_keypad (context);
#else
	return;
#endif
      } else {
#ifdef DIALER_BLUEBOX
        if (p->mode)
          i += DIALER_MAXBUTTONS;
#endif
        tonePlay (p, buttons[i].button_freq_a,
            buttons[i].button_freq_b, buttons[i].button_samples);
      }
    }
  }

}

static void dialer_exit(OrchardAppContext *context) {
  DHandles *p;
  userconfig * config;
  p = context->priv;

  destroy_keypad (context);

  gdispSetOrientation (p->o);
  gdispCloseFont (p->font);

  geventDetachSource (&p->glDListener, NULL);
  geventRegisterCallback (&p->glDListener, NULL, NULL);

  config = getConfig();
  config->sound_enabled = p->sound;

  chHeapFree (p);
  context->priv = NULL;

}

orchard_app("DTMF Dialer", "mplay.rgb", 0, dialer_init,
             dialer_start, dialer_event, dialer_exit, 9999);
