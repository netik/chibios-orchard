/*
 * app-choosetype.c
 * 
 * Allow the user to choose their player type
 *
 * J. Adams 3/24/2017 
 */

#include <string.h>
#include "orchard-app.h"
#include "orchard-ui.h"
#include "userconfig.h"
#include "ides_gfx.h"
#include "images.h"
#include "sound.h"
#include "dac_lld.h"

#define ALERT_DELAY 2500   // how long alerts stay on the screen.

// handles
typedef struct _CtHandles {
  // GListener
  GListener glCtListener;

  // GHandles
  GHandle ghButton1;
  GHandle ghButton2;
  GHandle ghButton3;
  GHandle ghOK;
  GHandle ghCancel;
  
  // Fonts
  font_t font_jupiterpro_16;
  font_t font_jupiterpro_36;
  font_t font_manteka_20;

} CtHandles;

static uint32_t last_ui_time = 0;
static uint8_t selected = 0;
static uint8_t ct_state = 0;  /* 0 = Select character page, 1 = OK/Cancel page */
static uint8_t ar_visible = 1;

static void init_ct_ui(CtHandles *p) {

  GWidgetInit wi;
  gwinWidgetClearInit(&wi);

  /* background */
  putImageFile(IMG_CHOOSETYPE, 0, 0);

  // create button widget: ghButton1
  wi.g.show = TRUE;
  wi.g.x = 0;
  wi.g.y = 45;
  wi.g.width = 106;
  wi.g.height = 195;
  wi.customDraw = noRender;
  wi.customParam = 0;
  p->ghButton1 = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghButton1, p->font_jupiterpro_36);
  gwinRedraw(p->ghButton1);

  // create button widget: ghButton1
  wi.g.show = TRUE;
  wi.g.x = 107;
  wi.g.y = 45;
  wi.g.width = 106;
  wi.g.height = 195;
  wi.customDraw = noRender;
  wi.customParam = 0;
  p->ghButton2 = gwinButtonCreate(0, &wi);
  gwinRedraw(p->ghButton2);

  // create button widget: ghButton3
  wi.g.show = TRUE;
  wi.g.x = 213;
  wi.g.y = 40;
  wi.g.width = 107;
  wi.g.height = 195;
  wi.customDraw = noRender;
  wi.customParam = 0;
  p->ghButton3 = gwinButtonCreate(0, &wi);
  gwinRedraw(p->ghButton3);

}

static void ct_drawarrow(void) {
  gdispFillArea(0,35,320,47,Black);
  uint16_t x;
  
  switch(selected) {
  case 1:
    x = 127;
    break;
  case 2:
    x = 242;
    break;
  default: /* 0 */
    x = 28;
  }    
  if (ar_visible)
    putImageFile(IMG_ARROW_DN_50, x, 37);

  ar_visible = !ar_visible;
}

static uint32_t ct_init(OrchardAppContext *context)
{
  (void)context;
  
  /*
   * We don't want any extra stack space allocated for us.
   * We'll use the heap.
   */
  
  return (0);
}


static void ct_draw_confirm(OrchardAppContext *context) {
  /* remove the old buttons */
  CtHandles * p;
  GWidgetInit wi;
  char tmp[40];
  char tmp2[40];
  
  p = context->priv;

  gwinDestroy(p->ghButton1);
  gwinDestroy(p->ghButton2);
  gwinDestroy(p->ghButton3);

  /* put him in the middle */
  gdispFillArea(0,0,320,210,Black);
  
  /* draw yes no buttons and the avatar */
  putImageFile(getAvatarImage(selected, "idla", 1, false),
               110, POS_PLAYER1_Y);

  strcpy(tmp, "Play as ");
  switch(selected) {
  case 1:
    strcat(tmp, "Senator?");
    strcpy(tmp2, "+2 AGL, +1 MGT, 2X VS CAESAR");
    break;
  case 2:
    strcat(tmp, "Gladiatrix?");
    strcpy(tmp2, "+1 Might, +1 Agility");
    break;
  default: /* 0 */
    strcat(tmp, "Guard?");
    strcpy(tmp2, "+1 Might, +0 Agility");
  }
  
  gdispDrawStringBox (0,
		      0,
		      gdispGetWidth(),
		      gdispGetFontMetric(p->font_jupiterpro_16, fontHeight),
		      tmp,
		      p->font_jupiterpro_16, White, justifyCenter);
  gdispDrawStringBox (0,
		      20,
		      gdispGetWidth(),
		      gdispGetFontMetric(p->font_jupiterpro_16, fontHeight),
		      tmp2,
		      p->font_jupiterpro_16, Yellow, justifyCenter);
  // draw UI
  gwinSetDefaultFont (p->font_manteka_20);
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 0;
  wi.g.y = 210;
  wi.g.width = 150;
  wi.g.height = 30;
  wi.text = "CANCEL";
  p->ghOK = gwinButtonCreate(0, &wi);
  
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 170;
  wi.g.y = 210;
  wi.g.width = 150;
  wi.g.height = 30;
  wi.text = "CONFIRM";
  p->ghCancel = gwinButtonCreate(0, &wi);

  gwinSetDefaultStyle(&BlackWidgetStyle, FALSE);
}


static void ct_start(OrchardAppContext *context)
{
  CtHandles *p;
  
  // clear the screen
  gdispClear (Black);

  // idle ui timer
  last_ui_time = chVTGetSystemTime();

  p = chHeapAlloc (NULL, sizeof(CtHandles));
  memset(p, 0, sizeof(CtHandles));
  p->font_jupiterpro_16 = gdispOpenFont("Jupiter_Pro_2561316");
  p->font_jupiterpro_36 = gdispOpenFont("Jupiter_Pro_2561336");
  p->font_manteka_20 = gdispOpenFont("manteka20");
  
  init_ct_ui(p);

  ct_drawarrow();
  context->priv = p;

  geventListenerInit(&p->glCtListener);
  gwinAttachListener(&p->glCtListener);
  geventRegisterCallback (&p->glCtListener,
                          orchardAppUgfxCallback,
                          &p->glCtListener);

  orchardAppTimer(context, 500000, true);
}

static void ct_remove_confirm(OrchardAppContext *context) {
  CtHandles * p;
  p = context->priv;

  if (p->ghOK)
    gwinDestroy(p->ghOK);

  if (p->ghCancel)
    gwinDestroy(p->ghCancel);
}

static void ct_event(OrchardAppContext *context,
                       const OrchardAppEvent *event)
{
  GEvent * pe;
  CtHandles * p;
  userconfig *config = getConfig();
  
  p = context->priv;

  if (event->type == timerEvent) {
    /*
      // idle tiemout 
      if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
        orchardAppRun(orchardAppByName("Badge"));
      }
    */
    if (ct_state == 0) 
      ct_drawarrow();

    return;
  }
  
  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;
    last_ui_time = chVTGetSystemTime();
    
    if (pe->type == GEVENT_GWIN_BUTTON) {
        dacPlay("click.raw");
        if (ct_state == 0) { 
          if ( ((GEventGWinButton*)pe)->gwin == p->ghButton1) {
            selected = 0;
            ar_visible = 1;
            ct_drawarrow();
            ct_draw_confirm(context);
            ct_state = 1;
            return;
          }
          
          if ( ((GEventGWinButton*)pe)->gwin == p->ghButton2) {
            selected = 1;
            ar_visible = 1;
            ct_drawarrow();
            ct_draw_confirm(context);
            ct_state = 1;
            return;
          }
          
          if ( ((GEventGWinButton*)pe)->gwin == p->ghButton3) {
            selected = 2;
            ar_visible = 1;
            ct_drawarrow();
            ct_draw_confirm(context);
            ct_state = 1;
            return;
          }
        }

        if (ct_state == 1) { 
          if ( ((GEventGWinButton*)pe)->gwin == p->ghOK) {
            ct_remove_confirm(context);
            ct_state = 0;
            init_ct_ui(p);
            /* TODO: save it */
          }
          if ( ((GEventGWinButton*)pe)->gwin == p->ghCancel) {
            ct_remove_confirm(context);
            ct_state = 0;
            init_ct_ui(p);
          }
        }
    }
  }
}


static void ct_exit(OrchardAppContext *context) {
  CtHandles * p;

  p = context->priv;

  gdispCloseFont (p->font_manteka_20);
  gdispCloseFont (p->font_jupiterpro_36);
  gdispCloseFont (p->font_jupiterpro_16);
  
  if (p->ghButton1)
    gwinDestroy(p->ghButton1);

  if (p->ghButton2)
    gwinDestroy(p->ghButton2);

  if (p->ghButton3)
    gwinDestroy(p->ghButton3);

  if (p->ghOK)
    gwinDestroy(p->ghOK);

  if (p->ghCancel)
    gwinDestroy(p->ghCancel);
      
  geventDetachSource (&p->glCtListener, NULL);
  geventRegisterCallback (&p->glCtListener, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;
}  

orchard_app("Choosetype", 0, ct_init, ct_start, ct_event, ct_exit);
