#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "orchard-app.h"
#include "led.h"
#include "orchard-ui.h"
#include "images.h"
#include "fontlist.h"

#include "userconfig.h"
#include "gfx.h"

#include "ides_gfx.h"

// GHandles
typedef struct _SetupHandles {
  GHandle ghCheckSound;
  GHandle ghCheckAirplane;
  GHandle ghLabel1;
  GHandle ghLabelPattern;
  GHandle ghButtonPatDn;
  GHandle ghButtonPatUp;
  GHandle ghLabel4;
  GHandle ghButtonDimUp;
  GHandle ghButtonDimDn;
  GHandle ghButtonOK;
  GHandle ghButtonCancel;
  GHandle ghButtonCalibrate;
  GListener glSetup;
} SetupHandles;

static uint32_t last_ui_time = 0;
extern struct FXENTRY fxlist[];

static font_t fontSM;
static font_t fontXS;

static void draw_setup_buttons(SetupHandles * p) {
  userconfig *config = getConfig();
  GWidgetInit wi;
  char tmp[40];
  
  gwinSetDefaultFont(fontSM);

  gwinWidgetClearInit(&wi);

  // create checkbox widget: ghCheckSound
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 140;
  wi.g.width = 200;
  wi.g.height = 20;
  wi.text = " Sounds";
  wi.customDraw = gwinCheckboxDraw_CheckOnLeft;
  p->ghCheckSound = gwinCheckboxCreate(0, &wi);
  gwinCheckboxCheck(p->ghCheckSound, config->sound_enabled);

  // create checkbox widget: ghCheckAirplane
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 160;
  wi.g.width = 200;
  wi.g.height = 20;
  wi.text = " Airplane Mode";
  wi.customDraw = gwinCheckboxDraw_CheckOnLeft;
  p->ghCheckAirplaned = gwinCheckboxCreate(0, &wi);
  gwinCheckboxCheck(p->ghCheckAirplane, config->airplane_mode);

  // Create label widget: ghLabel1
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 10;
  wi.g.width = 180;
  wi.g.height = 20;
  wi.text = "LED Pattern";
  p->ghLabel1 = gwinLabelCreate(0, &wi);

  // Create label widget: ghLabelPattern
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 30;
  wi.g.width = 190;
  wi.g.height = 20;
  wi.text = fxlist[config->led_pattern].name;
  wi.customParam = 0;
  p->ghLabelPattern = gwinLabelCreate(0, &wi);

  // create button widget: ghButtonPatDn
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 270;
  wi.g.y = 10;
  wi.g.width = 50;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  p->ghButtonPatDn = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonPatUp
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 200;
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

  // dimmer bar 
  drawProgressBar(10,100,180,10, 8, (8-config->led_shift), false, false);

  // create button widget: ghButtonDimUp
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 270;
  wi.g.y = 70;
  wi.g.width = 50;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowRight;
  p->ghButtonDimUp = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonDimDn
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 200;
  wi.g.y = 70;
  wi.g.width = 50;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowLeft;
  p->ghButtonDimDn = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonCancel
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 170;
  wi.g.y = 140;
  wi.g.width = 140;
  wi.g.height = 20;
  wi.text = "Calibrate TS";
  p->ghButtonCalibrate = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonCancel
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 210;
  wi.g.width = 140;
  wi.g.height = 20;
  wi.text = "Cancel";
  p->ghButtonCancel = gwinButtonCreate(0, &wi);

  // create button widget: ghButtonOK
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 170;
  wi.g.y = 210;
  wi.g.width = 140;
  wi.g.height = 20;
  wi.text = "OK";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghButtonOK = gwinButtonCreate(0, &wi);

  // show network id
  chsnprintf(tmp,sizeof(tmp), "Net ID: %08x", config->netid);
  gdispDrawStringBox (0,
		      194,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontXS, fontHeight),
		      tmp,
		      fontXS, Purple, justifyCenter);
    
}

static uint32_t setup_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static void setup_start(OrchardAppContext *context) {
  SetupHandles * p;

  fontSM = gdispOpenFont (FONT_SM);
  fontXS = gdispOpenFont (FONT_XS);

  gdispClear (Black);

  // fires once a second for updates. 
  orchardAppTimer(context, 1000, true);
  last_ui_time = chVTGetSystemTime();

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
  SetupHandles * p;

  p = context->priv;
  if (event->type == timerEvent) {
    if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
      orchardAppRun(orchardAppByName("Badge"));
    }
    return;
  }

  if (event->type == keyEvent) {
    last_ui_time = chVTGetSystemTime();
    if ( (event->key.code == keyLeft) &&
         (event->key.flags == keyPress) )  {
      config->led_shift++;
      if (config->led_shift > 7) config->led_shift = 0;
      ledSetBrightness(config->led_shift);
    }
    if ( (event->key.code == keyRight) &&
         (event->key.flags == keyPress) )  {
      	config->led_shift--;
        if (config->led_shift == 255) config->led_shift = 7;
        ledSetBrightness(config->led_shift);
    }
    if ( (event->key.code == keyUp) &&
         (event->key.flags == keyPress) )  {
      config->led_pattern--;
      ledResetPattern();
      if (config->led_pattern == 255) config->led_pattern = LED_PATTERN_COUNT - 1;
      ledSetFunction(fxlist[config->led_pattern].function);
    }
    if ( (event->key.code == keyDown) &&
         (event->key.flags == keyPress) )  {
      config->led_pattern++;
      ledResetPattern();
      if (config->led_pattern >= LED_PATTERN_COUNT) config->led_pattern = 0;
      ledSetFunction(fxlist[config->led_pattern].function);
    }
  }

  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;
    last_ui_time = chVTGetSystemTime();
    
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
	return;
      }
      
      if (((GEventGWinButton*)pe)->gwin == p->ghButtonOK) {
          configSave(config);
          orchardAppExit();
          return;
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
        ledSetFunction(fxlist[config->led_pattern].function);
      }
      
      if (((GEventGWinButton*)pe)->gwin == p->ghButtonPatUp) {
	config->led_pattern--;
	ledResetPattern();
	if (config->led_pattern == 255) config->led_pattern = LED_PATTERN_COUNT - 1;
        ledSetFunction(fxlist[config->led_pattern].function);
      }
      break;
    }
  }
  // update ui
  drawProgressBar(10,100,180,10, 8, (8-config->led_shift), false, false);
  gwinSetText(p->ghLabelPattern, fxlist[config->led_pattern].name, TRUE);
  return;
}

static void setup_exit(OrchardAppContext *context) {
  SetupHandles * p;

  p = context->priv;

  gwinDestroy(p->ghCheckSound);
  gwinDestroy(p->ghLabel1);
  gwinDestroy(p->ghLabelPattern);
  gwinDestroy(p->ghButtonPatDn);
  gwinDestroy(p->ghButtonPatUp);
  gwinDestroy(p->ghLabel4);
  gwinDestroy(p->ghButtonDimUp);
  gwinDestroy(p->ghButtonDimDn);
  gwinDestroy(p->ghButtonOK);
  gwinDestroy(p->ghButtonCancel);
  gwinDestroy(p->ghButtonCalibrate);
  
  gdispCloseFont(fontXS);
  gdispCloseFont(fontSM);
    
  geventDetachSource (&p->glSetup, NULL);
  geventRegisterCallback (&p->glSetup, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;

  return;
}

orchard_app("Setup", 0, setup_init, setup_start, setup_event, setup_exit);
