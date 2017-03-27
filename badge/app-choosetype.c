/*
 * app-choosetype.c
 * 
 * Allow the user to choose their player type. Run once during setup.
 * 
 * If you update the starting stats for a character, those changes
 * must be reflected in the event handler for this code, as well as
 * the messaging on the confirmation screen.
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

#define ALERT_DELAY 2500

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
static uint8_t ct_state = 0;   /* 0 = Select character page, 1 = OK/Cancel page */
static uint8_t ar_visible = 1; /* used for animation state, either blinking arrow or character anim. */

/* prototypes */
static void ct_save(OrchardAppContext *context);

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
  p->ghButton1 = NULL;
  gwinDestroy(p->ghButton2);
  p->ghButton2 = NULL;
  gwinDestroy(p->ghButton3);
  p->ghButton3 = NULL;

  /* put him in the middle */
  gdispFillArea(0,0,320,210,Black);
  
  /* draw yes no buttons and the avatar */
  putImageFile(getAvatarImage(selected, "idla", 1, false),
               110, POS_PLAYER1_Y);

  strcpy(tmp, "Play as ");
  switch(selected) {
  case 1:
    strcat(tmp, "Senator?");
    strcpy(tmp2, "+3 AGL, +1 MGT, 2X VS CAESAR");
    break;
  case 2:
    strcat(tmp, "Gladiatrix?");
    strcpy(tmp2, "+2 Might, +2 Agility");
    break;
  default: /* 0 */
    strcat(tmp, "Guard?");
    strcpy(tmp2, "+3 Might, +1 Agility");
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
  p->ghCancel = gwinButtonCreate(0, &wi);
  
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 170;
  wi.g.y = 210;
  wi.g.width = 150;
  wi.g.height = 30;
  wi.text = "CONFIRM";
  p->ghOK = gwinButtonCreate(0, &wi);

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

  if (p->ghOK) {
    gwinDestroy(p->ghOK);
    p->ghOK = NULL;
  }

  if (p->ghCancel) { 
    gwinDestroy(p->ghCancel);
    p->ghCancel = NULL;
  }
}

static void ct_save(OrchardAppContext *context) {
  /* store the final selection to flash.*/
  userconfig *config = getConfig();
  uint16_t middle = (gdispGetHeight() / 2);
  char msg[40];
  CtHandles * p;

  p = context->priv;

  config->p_type = selected;
  
  switch(selected) {
  case 2: /* gladiatrix, balanced */
    config->agl = 2;
    config->might = 2;
    strcpy(msg, "HAIL, GLADIATRIX.");
    break;
  case 1: /* senator, defensive */
    config->agl = 3;
    config->might = 1;
    strcpy(msg, "HAIL, SENATOR.");
    break;
  default: /* 0: guard, strong attack */
    config->agl = 1;
    config->might = 3;
    strcpy(msg, "HAIL, PRAETORIAN GUARD.");
    break;
  }

  gdispDrawThickLine(0,middle - 20, 320, middle -20, Red, 2, FALSE);
  gdispDrawThickLine(0,middle + 20, 320, middle +20, Red, 2, FALSE);
  gdispFillArea( 0, middle - 20, 320, 40, Black );

  gdispDrawStringBox (0,
		      (gdispGetHeight() / 2) - (gdispGetFontMetric(p->font_manteka_20, fontHeight) / 2),
		      gdispGetWidth(),
		      gdispGetFontMetric(p->font_manteka_20, fontHeight),
		      msg,
		      p->font_manteka_20, Red, justifyCenter);

  configSave(config);

  chThdSleepMilliseconds(ALERT_DELAY);
}

static void ct_event(OrchardAppContext *context,
                       const OrchardAppEvent *event)
{
  GEvent * pe;
  CtHandles * p;
  
  p = context->priv;

  if (event->type == timerEvent) {
    /* blink the arrow */
    if (ct_state == 0) ct_drawarrow();

    /* animate the character */
    if (ct_state == 1) {
      if (ar_visible) {
        putImageFile(getAvatarImage(selected, "idla", 1, false),
                     110, POS_PLAYER1_Y);
      } else {
        putImageFile(getAvatarImage(selected, "atth", 1, false),
                     110, POS_PLAYER1_Y);
      }

      ar_visible = !ar_visible;
        
    }
    
    return;
  }

  /* Joypad control */
  if (ct_state == 0) { 
    if (event->type == keyEvent) {
      if ( (event->key.code == keyRight) &&
         (event->key.flags == keyPress) )  {
        dacPlay("click.raw");
        selected++;
        if (selected > 2) { selected = 0; };
        ct_drawarrow();
        return;
      }
      
      if ( (event->key.code == keyLeft) &&
           (event->key.flags == keyPress) )  {
        dacPlay("click.raw");
        selected--;
        if (selected > 254) { selected = 2; };
        ct_drawarrow();
        return;
      }
    }
    if ( (event->key.code == keySelect) &&
         (event->key.flags == keyPress) )  {
      dacPlay("fight/select.raw");
      ct_draw_confirm(context);
      ct_state = 1;
      return;
    }
  } else {
    /* To avoid UGFX madness, we're going to make it very simple.
     * in state 2 things are a bit different. 
     *
     * LEFT is cancel / go back 
     * RIGHT or ENTER acts as confirm
     */
    if ( (event->key.code == keyLeft) &&
         (event->key.flags == keyPress) )  {
      dacPlay("click.raw");
      ct_remove_confirm(context);
      ct_state = 0;
      init_ct_ui(p);
      
    }

    if ( (( event->key.code == keyRight) || (event->key.code == keySelect) ) && 
         (event->key.flags == keyPress) )  {
      dacPlay("fight/drop.raw");
      ct_remove_confirm(context);
      ct_state = 0;
      ct_save(context);
      orchardAppRun(orchardAppByName("Badge"));
      return;
    }
    
  }

  /* End joypad controls */
  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;
    last_ui_time = chVTGetSystemTime();
    
    if (pe->type == GEVENT_GWIN_BUTTON) {
      if (ct_state == 0) { 
        
        if ( ((GEventGWinButton*)pe)->gwin == p->ghButton1) {
          dacPlay("fight/select.raw");
          selected = 0;
          ar_visible = 1;
          ct_drawarrow();
          ct_draw_confirm(context);
          ct_state = 1;
          return;
        }
        
        if ( ((GEventGWinButton*)pe)->gwin == p->ghButton2) {
          dacPlay("fight/select.raw");
          selected = 1;
          ar_visible = 1;
          ct_drawarrow();
          ct_draw_confirm(context);
          ct_state = 1;
          return;
          }
        
        if ( ((GEventGWinButton*)pe)->gwin == p->ghButton3) {
          dacPlay("fight/select.raw");
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
          dacPlay("fight/drop.raw");
          ct_remove_confirm(context);
          ct_state = 0;
          ct_save(context);
          orchardAppRun(orchardAppByName("Badge"));
          return;
        }
        if ( ((GEventGWinButton*)pe)->gwin == p->ghCancel) {
          dacPlay("click.raw");
          ct_remove_confirm(context);
          ct_state = 0;
          init_ct_ui(p);
          return;
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
