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

static GListener glSetup;

// GHandles
static GHandle ghCheckSound;
static GHandle ghLabel1;
static GHandle ghLabelPattern;
static GHandle ghButtonPatDn;
static GHandle ghButtonPatUp;
static GHandle ghLabel3;
static GHandle ghLabel4;
static GHandle ghLabelDim;
static GHandle ghButtonDimUp;
static GHandle ghButtonDimDn;
static GHandle ghButtonOK;
static GHandle ghButtonCancel;

static void draw_setup_buttons(void) {
  userconfig *config = getConfig();
  GWidgetInit wi;
  font_t fontSM = gdispOpenFont (FONT_SM);
  char tmp[3];
  
  gwinSetDefaultFont(fontSM);

  gwinWidgetClearInit(&wi);
  // create checkbox widget: ghCheckSound
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 150;
  wi.g.width = 120;
  wi.g.height = 20;
  wi.text = "Enabled";
  wi.customDraw = gwinCheckboxDraw_CheckOnLeft;
  ghCheckSound = gwinCheckboxCreate(0, &wi);
  gwinCheckboxCheck(ghCheckSound, config->sound_enabled);

  // Create label widget: ghLabel1
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 10;
  wi.g.width = 120;
  wi.g.height = 20;
  wi.text = "LED Pattern";
  ghLabel1 = gwinLabelCreate(0, &wi);

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
  ghButtonOK = gwinButtonCreate(0, &wi);

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
  ghLabelPattern = gwinLabelCreate(0, &wi);

  // create button widget: ghButtonPatDn
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 220;
  wi.g.y = 30;
  wi.g.width = 40;
  wi.g.height = 20;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  ghButtonPatDn = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonPatUp
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 270;
  wi.g.y = 30;
  wi.g.width = 40;
  wi.g.height = 20;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowDown;
  ghButtonPatUp = gwinButtonCreate(0, &wi);

  // Create label widget: ghLabel3
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 130;
  wi.g.width = 120;
  wi.g.height = 20;
  wi.text = "Sound";
  ghLabel3 = gwinLabelCreate(0, &wi);

  // Create label widget: ghLabel4
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 70;
  wi.g.width = 200;
  wi.g.height = 20;
  wi.text = "LED Brightness";
  ghLabel4 = gwinLabelCreate(0, &wi);

  // Create label widget: ghLabelDim
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 90;
  wi.g.width = 200;
  wi.g.height = 20;
  chsnprintf(tmp, sizeof(tmp), "%d", ( 8 - config->led_shift ));
  wi.text = tmp;
  ghLabelDim = gwinLabelCreate(0, &wi);

  // create button widget: ghButtonDimUp
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 220;
  wi.g.y = 90;
  wi.g.width = 40;
  wi.g.height = 20;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  ghButtonDimUp = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonDimDn
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 270;
  wi.g.y = 90;
  wi.g.width = 40;
  wi.g.height = 20;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowDown;
  ghButtonDimDn = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonCancel
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 190;
  wi.g.width = 140;
  wi.g.height = 40;
  wi.text = "Cancel";
  ghButtonCancel = gwinButtonCreate(0, &wi);
}

static uint32_t setup_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static void setup_start(OrchardAppContext *context) {
  (void)context;
  gdispClear (Black);
  draw_setup_buttons();

  geventListenerInit(&glSetup);
  gwinAttachListener(&glSetup);
  geventRegisterCallback (&glSetup, orchardAppUgfxCallback, &glSetup);
}

void setup_event(OrchardAppContext *context, const OrchardAppEvent *event) {
  GEvent * pe;
  userconfig *config = getConfig();
  char tmp[3];
  (void)context;
  
  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;

    switch(pe->type) {

    case GEVENT_GWIN_CHECKBOX:
      if (((GEventGWinCheckbox*)pe)->gwin == ghCheckSound) {
	// The state of our checkbox has changed
	config->sound_enabled = ((GEventGWinCheckbox*)pe)->isChecked;
      }
      break;
    case GEVENT_GWIN_BUTTON:
        if (((GEventGWinButton*)pe)->gwin == ghButtonCancel) {
     	  orchardAppExit();
        }

        if (((GEventGWinButton*)pe)->gwin == ghButtonOK) {
     	  orchardAppExit();
          configSave(config);
        }

        if (((GEventGWinButton*)pe)->gwin == ghButtonDimDn) {
          config->led_shift++;
          if (config->led_shift > 7) config->led_shift = 0;
          ledSetBrightness(config->led_shift);
        }

        if (((GEventGWinButton*)pe)->gwin == ghButtonDimUp) {
          config->led_shift--;
          if (config->led_shift == 255) config->led_shift = 7;
          ledSetBrightness(config->led_shift);
        }

        if (((GEventGWinButton*)pe)->gwin == ghButtonPatDn) {
	  config->led_pattern++;
	  ledResetPattern();
          if (config->led_pattern >= LED_PATTERN_COUNT) config->led_pattern = 0;
        }

        if (((GEventGWinButton*)pe)->gwin == ghButtonPatUp) {
          config->led_pattern--;
	  ledResetPattern();
          if (config->led_pattern == 255) config->led_pattern = LED_PATTERN_COUNT - 1;
        }

      // update ui
      chsnprintf(tmp, sizeof(tmp), "%d",(  8 - config->led_shift ) );
      gwinSetText(ghLabelDim, tmp, TRUE);
      chsnprintf(tmp, sizeof(tmp), "%d", config->led_pattern + 1);
      gwinSetText(ghLabelPattern, tmp, TRUE);

      break;
    }
    return;
  }
}

static void setup_exit(OrchardAppContext *context) {
  (void)context;

  gwinDestroy(ghCheckSound);
  gwinDestroy(ghLabel1);
  gwinDestroy(ghLabelPattern);
  gwinDestroy(ghButtonPatDn);
  gwinDestroy(ghButtonPatUp);
  gwinDestroy(ghLabel3);
  gwinDestroy(ghLabel4);
  gwinDestroy(ghLabelDim);
  gwinDestroy(ghButtonDimUp);
  gwinDestroy(ghButtonDimDn);
  gwinDestroy(ghButtonOK);
  gwinDestroy(ghButtonCancel);
  
  geventDetachSource (&glSetup, NULL);
  geventRegisterCallback (&glSetup, NULL, NULL);
  return;
}

orchard_app("Setup", setup_init, setup_start, setup_event, setup_exit);

