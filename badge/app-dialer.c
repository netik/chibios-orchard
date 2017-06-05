#include <string.h>
#include "orchard-app.h"
#include "ides_gfx.h"

#include "dac_lld.h"

typedef struct _DHandles {
  // GListeners
  GListener glDListener;

  // GHandles
  GHandle ghContainerPage0;
  GHandle ghButton1;
  GHandle ghButton2;
  GHandle ghButton3;
  GHandle ghButton4;
  GHandle ghButton5;
  GHandle ghButton6;
  GHandle ghButton7;
  GHandle ghButton8;
  GHandle ghButton9;
  GHandle ghStar;
  GHandle ghButton0;
  GHandle ghPound;
  GHandle ghButtonA;
  GHandle ghButtonB;
  GHandle ghButtonC;
  GHandle ghButtonD;
  GHandle ghButton2600;
  GHandle ghButtonSwitch;
} DHandles;

static uint32_t last_ui_time = 0;

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

  p = context->priv;
  gwinWidgetClearInit(&wi);

  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 10;
  wi.g.width = 60;
  wi.g.height = 60;
  wi.text = "1";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customStyle = &RedButtonStyle;
  p->ghButton1 = gwinButtonCreate(0, &wi);

  // create button widget: ghButton2
  wi.g.show = TRUE;
  wi.g.x = 90;
  wi.g.y = 10;
  wi.text = "2";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghButton2 = gwinButtonCreate(0, &wi);

  // create button widget: ghButton3
  wi.g.show = TRUE;
  wi.g.x = 170;
  wi.g.y = 10;
  wi.text = "3";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghButton3 = gwinButtonCreate(0, &wi);

  // create button widget: ghButton4
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 90;
  wi.text = "4";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghButton4 = gwinButtonCreate(0, &wi);

  // create button widget: ghButton5
  wi.g.show = TRUE;
  wi.g.x = 90;
  wi.g.y = 90;
  wi.text = "5";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghButton5 = gwinButtonCreate(0, &wi);

  // create button widget: ghButton6
  wi.g.show = TRUE;
  wi.g.x = 170;
  wi.g.y = 90;
  wi.text = "6";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghButton6 = gwinButtonCreate(0, &wi);

  // create button widget: ghButton7
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 170;
  wi.text = "7";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghButton7 = gwinButtonCreate(0, &wi);

  // create button widget: ghButton8
  wi.g.show = TRUE;
  wi.g.x = 90;
  wi.g.y = 170;
  wi.text = "8";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghButton8 = gwinButtonCreate(0, &wi);

  // create button widget: ghButton9
  wi.g.show = TRUE;
  wi.g.x = 170;
  wi.g.y = 170;
  wi.text = "9";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghButton9 = gwinButtonCreate(0, &wi);

  // create button widget: ghStar
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 250;
  wi.text = "*";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghStar = gwinButtonCreate(0, &wi);

  // create button widget: ghButton0
  wi.g.show = TRUE;
  wi.g.x = 90;
  wi.g.y = 250;
  wi.text = "0";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghButton0 = gwinButtonCreate(0, &wi);

  // create button widget: ghPound
  wi.g.show = TRUE;
  wi.g.x = 170;
  wi.g.y = 250;
  wi.text = "#";
  wi.customDraw = gwinButtonDraw_Normal;
  p->ghPound = gwinButtonCreate(0, &wi);
}

static void dialer_start(OrchardAppContext *context) {
  gdispClear(Black);
  gdispSetOrientation (0);
  DHandles *p;
  p = chHeapAlloc (NULL, sizeof(DHandles));
  memset(p, 0, sizeof(DHandles));
  context->priv = p;

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
      gdispSetOrientation (GDISP_DEFAULT_ORIENTATION);
      orchardAppExit();
    }
  }

  // ugfx
  if (event->type == ugfxEvent) {
    char dial='\0';
    dacWait();
    
    pe = event->ugfx.pEvent;
    last_ui_time = chVTGetSystemTime();

    if (pe->type == GEVENT_GWIN_BUTTON) { 
      if (((GEventGWinButton*)pe)->gwin == p->ghButton1) {
        dial = '1';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghButton2) {
        dial = '2';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghButton3) {
        dial = '3';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghButton4) {
        dial = '4';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghButton5) {
        dial = '5';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghButton6) {
        dial = '6';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghButton7) {
        dial = '7';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghButton8) {
        dial = '8';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghButton9) {
        dial = '9';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghButton0) {
        dial = '0';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghStar) {
        dial = 's';
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghPound) {
        dial = 'p';
      }

      // end
      if (dial != '\0') {
        char dialstr[14];

        chsnprintf(dialstr,sizeof(dialstr),"dtmf-%c.raw", dial);
        dacPlay(dialstr);
      }
    }
  }

}

static void dialer_exit(OrchardAppContext *context) {
  DHandles *p;
  p = context->priv;

  gwinDestroy(p->ghContainerPage0);
  gwinDestroy(p->ghButton1);
  gwinDestroy(p->ghButton2);
  gwinDestroy(p->ghButton3);
  gwinDestroy(p->ghButton4);
  gwinDestroy(p->ghButton5);
  gwinDestroy(p->ghButton6);
  gwinDestroy(p->ghButton7);
  gwinDestroy(p->ghButton8);
  gwinDestroy(p->ghButton9);
  gwinDestroy(p->ghStar);
  gwinDestroy(p->ghPound);

  geventDetachSource (&p->glDListener, NULL);
  geventRegisterCallback (&p->glDListener, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;

}

orchard_app("Dialer", "phone.rgb", 0, dialer_init,
             dialer_start, dialer_event, dialer_exit, 9999);

  
