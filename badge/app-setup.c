#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "orchard-app.h"
#include "led.h"
#include "radio.h"
#include "orchard-ui.h"
#include "images.h"
#include "fontlist.h"

#include "userconfig.h"
#include "gfx.h"


// GHandles
typedef struct _SetupHandles {
	GHandle ghCheckSound;
	GHandle ghLabel1;
	GHandle ghLabelPattern;
	GHandle ghButtonPatDn;
	GHandle ghButtonPatUp;
	GHandle ghLabel3;
	GHandle ghLabel4;
	GHandle ghLabelDim;
	GHandle ghButtonDimUp;
	GHandle ghButtonDimDn;
	GHandle ghButtonOK;
	GHandle ghButtonCancel;
	GListener glSetup;
} SetupHandles;

static void draw_setup_buttons(SetupHandles * p) {
  userconfig *config = getConfig();
  GWidgetInit wi;
  font_t fontSM = gdispOpenFont (FONT_SM);
  char tmp[3];
  
  gwinSetDefaultFont(fontSM);

  gwinWidgetClearInit(&wi);
  // create checkbox widget: ghCheckSound
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 140;
  wi.g.width = 200;
  wi.g.height = 20;
  wi.text = " Enable Sounds";
  wi.customDraw = gwinCheckboxDraw_CheckOnLeft;
  p->ghCheckSound = gwinCheckboxCreate(0, &wi);
  gwinCheckboxCheck(p->ghCheckSound, config->sound_enabled);

  // Create label widget: ghLabel1
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 10;
  wi.g.width = 180;
  wi.g.height = 20;
  wi.text = "LED Pattern";
  p->ghLabel1 = gwinLabelCreate(0, &wi);

  // create button widget: ghButtonOK
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 170;
  wi.g.y = 190;
  wi.g.width = 140;
  wi.g.height = 40;
  wi.text = "OK";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghButtonOK = gwinButtonCreate(0, &wi);

  // Create label widget: ghLabelPattern
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 30;
  wi.g.width = 200;
  wi.g.height = 20;
  chsnprintf(tmp, sizeof(tmp), "%d", config->led_pattern+1);
  wi.text = tmp;
  wi.customParam = 0;
  p->ghLabelPattern = gwinLabelCreate(0, &wi);

  // create button widget: ghButtonPatDn
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 200;
  wi.g.y = 10;
  wi.g.width = 50;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  p->ghButtonPatDn = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonPatUp
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 270;
  wi.g.y = 10;
  wi.g.width = 50;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowDown;
  p->ghButtonPatUp = gwinButtonCreate(0, &wi);


  // Create label widget: ghLabel4
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 70;
  wi.g.width = 180;
  wi.g.height = 20;
  wi.text = "LED Brightness";
  p->ghLabel4 = gwinLabelCreate(0, &wi);

  // Create label widget: ghLabelDim
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 90;
  wi.g.width = 200;
  wi.g.height = 20;
  chsnprintf(tmp, sizeof(tmp), "%d", ( 8 - config->led_shift ));
  wi.text = tmp;
  p->ghLabelDim = gwinLabelCreate(0, &wi);

  // create button widget: ghButtonDimUp
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 200;
  wi.g.y = 70;
  wi.g.width = 50;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  p->ghButtonDimUp = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonDimDn
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 270;
  wi.g.y = 70;
  wi.g.width = 50;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowDown;
  p->ghButtonDimDn = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonCancel
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 190;
  wi.g.width = 140;
  wi.g.height = 40;
  wi.text = "Cancel";
  p->ghButtonCancel = gwinButtonCreate(0, &wi);
}

static uint32_t setup_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static void setup_start(OrchardAppContext *context) {
  SetupHandles * p;

  (void)context;
  gdispClear (Black);

  p = chHeapAlloc (NULL, sizeof(SetupHandles));

  draw_setup_buttons(p);
  context->priv = p;

  geventListenerInit(&p->glSetup);
  gwinAttachListener(&p->glSetup);
  geventRegisterCallback (&p->glSetup, orchardAppUgfxCallback, &p->glSetup);
}

static void setup_event(OrchardAppContext *context,
	const OrchardAppEvent *event) {
  GEvent * pe;
  userconfig *config = getConfig();
  char tmp[3];
  SetupHandles * p;

  p = context->priv;
  
  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;

    switch(pe->type) {

    case GEVENT_GWIN_CHECKBOX:
      if (((GEventGWinCheckbox*)pe)->gwin == p->ghCheckSound) {
	// The state of our checkbox has changed
	config->sound_enabled = ((GEventGWinCheckbox*)pe)->isChecked;
      }
      break;
    case GEVENT_GWIN_BUTTON:
        if (((GEventGWinButton*)pe)->gwin == p->ghButtonCancel) {
     	  orchardAppExit();
        }

        if (((GEventGWinButton*)pe)->gwin == p->ghButtonOK) {
     	  orchardAppExit();
          configSave(config);
        }

        if (((GEventGWinButton*)pe)->gwin == p->ghButtonDimDn) {
          config->led_shift++;
          if (config->led_shift > 7) config->led_shift = 0;
          ledSetBrightness(config->led_shift);
        }

        if (((GEventGWinButton*)pe)->gwin == p->ghButtonDimUp) {
          config->led_shift--;
          if (config->led_shift == 255) config->led_shift = 7;
          ledSetBrightness(config->led_shift);
        }

        if (((GEventGWinButton*)pe)->gwin == p->ghButtonPatDn) {
	  config->led_pattern++;
	  ledResetPattern();
          if (config->led_pattern >= LED_PATTERN_COUNT) config->led_pattern = 0;
        }

        if (((GEventGWinButton*)pe)->gwin == p->ghButtonPatUp) {
          config->led_pattern--;
	  ledResetPattern();
          if (config->led_pattern == 255) config->led_pattern = LED_PATTERN_COUNT - 1;
        }

      // update ui
      chsnprintf(tmp, sizeof(tmp), "%d",(  8 - config->led_shift ) );
      gwinSetText(p->ghLabelDim, tmp, TRUE);
      chsnprintf(tmp, sizeof(tmp), "%d", config->led_pattern + 1);
      gwinSetText(p->ghLabelPattern, tmp, TRUE);

      break;
    }
    return;
  }
}

static void setup_exit(OrchardAppContext *context) {
  SetupHandles * p;

  p = context->priv;

  gwinDestroy(p->ghCheckSound);
  gwinDestroy(p->ghLabel1);
  gwinDestroy(p->ghLabelPattern);
  gwinDestroy(p->ghButtonPatDn);
  gwinDestroy(p->ghButtonPatUp);
  gwinDestroy(p->ghLabel3);
  gwinDestroy(p->ghLabel4);
  gwinDestroy(p->ghLabelDim);
  gwinDestroy(p->ghButtonDimUp);
  gwinDestroy(p->ghButtonDimDn);
  gwinDestroy(p->ghButtonOK);
  gwinDestroy(p->ghButtonCancel);
  
  geventDetachSource (&p->glSetup, NULL);
  geventRegisterCallback (&p->glSetup, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;

  return;
}

orchard_app("Setup", setup_init, setup_start, setup_event, setup_exit);
