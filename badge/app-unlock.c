/*
 * app-unlock.c
 * 
 * the unlock screen that allows people to unlock features in 
 * the badge
 *
 */

#include <string.h>
#include "orchard-app.h"
#include "orchard-ui.h"
#include "userconfig.h"
#include "ides_gfx.h"
#include "images.h"
#include "sound.h"
#include "dac_lld.h"
#include "led.h"

#define ALERT_DELAY 2500   // how long alerts stay on the screen.

// WidgetStyle: Ivory
const GWidgetStyle ivory = {
  HTML2COLOR(0xffefbe),              // background
  HTML2COLOR(0x2A8FCD),              // focus

  // Enabled color set
  {
    HTML2COLOR(0x000000),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0xffefbe),         // background
    HTML2COLOR(0x00E000),         // progress (inactive area)
  },

  // Disabled color set
  {
    HTML2COLOR(0xC0C0C0),         // text
    HTML2COLOR(0x808080),         // edge
    HTML2COLOR(0xE0E0E0),         // fill
    HTML2COLOR(0xC0E0C0),         // progress (active area)
  },

  // Pressed color set
  {
    HTML2COLOR(0x404040),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0x808080),         // fill
    HTML2COLOR(0x00E000),         // progress (active area)
  }
};

// handles
typedef struct _UnlockHandles {
  // GListener
  GListener glUnlockListener;

  // GHandles
  GHandle ghNum1;
  GHandle ghNum2;
  GHandle ghNum3;
  GHandle ghNum4;
  GHandle ghNum5;

  GHandle ghBack;
  GHandle ghUnlock;
  
  // Fonts
  font_t font_jupiterpro_36;
  font_t font_manteka_20;

} UnlockHandles;

#define MAX_ULCODES 10

/* codes, packed as bits. Note only last five bits are used, MSB of 1st byte is always zero */
static uint8_t unlock_codes[MAX_ULCODES][3] = { { 0x01, 0xde, 0xf1 }, // 0 +10% DEF
                                                { 0x0d, 0xef, 0xad }, // 1 +10% HP
                                                { 0x0a, 0x7a, 0xa7 }, // 2 +1 MIGHT
                                                { 0x07, 0x70, 0x07 }, // 3 +20% LUCK
                                                { 0x0a, 0xed, 0x17 }, // 4 FASTER HEAL
                                                { 0x00, 0x1a, 0xc0 }, // 5 MOAR LEDs
                                                { 0x0b, 0xae, 0xac }, // 6 CAESAR
                                                { 0x0d, 0xe0, 0x1a }, // 7 SENATOR
                                                { 0x09, 0x00, 0x46 }, // 8 TIMELORD
                                                { 0x0b, 0xbb, 0xbb }  // 9 BENDER
};
static char *unlock_desc[] = { "+10% DEF",
                               "+10% HP",
                               "+1 MIGHT",
                               "+20% LUCK",
                               "FASTER HEAL",
                               "MOAR LEDs",
                               "U R CAESAR",
                               "U R SENATOR",
                               "U R TIMELORD",
                               "BENDER" };

static uint32_t last_ui_time = 0;
static uint8_t code[5];
static int8_t focused = 0;

static void updatecode(OrchardAppContext *, uint8_t, int8_t);
static void unlock_result(UnlockHandles *, char *);

static void unlock_result(UnlockHandles *p, char *msg) {
  gdispClear(Black);
  gdispDrawStringBox (0,
		      (gdispGetHeight() / 2) - (gdispGetFontMetric(p->font_manteka_20, fontHeight) / 2),
		      gdispGetWidth(),
		      gdispGetFontMetric(p->font_manteka_20, fontHeight),
		      msg,
		      p->font_manteka_20, Yellow, justifyCenter);
}

static void init_unlock_ui(UnlockHandles *p) {

  GWidgetInit wi;
  
  p->font_jupiterpro_36 = gdispOpenFont("Jupiter_Pro_2561336");
  p->font_manteka_20 = gdispOpenFont("manteka20");
  
  gwinWidgetClearInit(&wi);

  // create button widget: ghNum1
  wi.g.show = TRUE;
  wi.g.x = 40;
  wi.g.y = 40;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "0";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customParam = 0;
  wi.customStyle = &ivory;
  p->ghNum1 = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghNum1, p->font_jupiterpro_36);
  gwinRedraw(p->ghNum1);

  // create button widget: ghNum2
  wi.g.show = TRUE;
  wi.g.x = 90;
  wi.g.y = 40;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "0";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customParam = 0;
  wi.customStyle = &ivory;
  p->ghNum2 = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghNum2, p->font_jupiterpro_36);
  gwinRedraw(p->ghNum2);

  // create button widget: ghNum3
  wi.g.x = 140;
#ifdef notdef
  wi.g.show = TRUE;
  wi.g.y = 40;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "0";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customParam = 0;
  wi.customStyle = &ivory;
#endif
  p->ghNum3 = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghNum3, p->font_jupiterpro_36);
  gwinRedraw(p->ghNum3);

  // create button widget: ghNum4
  wi.g.x = 190;
#ifdef notdef
  wi.g.show = TRUE;
  wi.g.y = 40;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "0";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customParam = 0;
  wi.customStyle = &ivory;
#endif
  p->ghNum4 = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghNum4, p->font_jupiterpro_36);
  gwinRedraw(p->ghNum4);

  // create button widget: ghNum5
  wi.g.x = 240;
#ifdef notdef
  wi.g.show = TRUE;
  wi.g.y = 40;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "0";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customParam = 0;
  wi.customStyle = &ivory;
#endif
  p->ghNum5 = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghNum5,  p->font_jupiterpro_36);
  gwinRedraw(p->ghNum5);
  
  // create button widget: ghBack
#ifdef notdef
  wi.g.show = TRUE;
  wi.customStyle = &ivory;
#endif
  wi.g.x = 10;
  wi.g.y = 100;
  wi.g.width = 110;
  wi.g.height = 30;
  wi.text = "<-";
  p->ghBack = gwinButtonCreate(0, &wi);

  // create button widget: ghUnlock
  wi.g.x = 200;
  wi.g.y = 100;
#ifdef notdef
  wi.g.show = TRUE;
  wi.g.width = 110;
  wi.g.height = 30;
  wi.customStyle = &ivory;
#endif
  wi.text = "UNLOCK";
  wi.customDraw = 0;
  p->ghUnlock = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghUnlock, p->font_manteka_20);
  gwinRedraw(p->ghUnlock);

  // reset focus
  focused = 0;
  gwinSetFocus(p->ghNum1);
}


static uint32_t unlock_init(OrchardAppContext *context)
{
  (void)context;
  
  /*
   * We don't want any extra stack space allocated for us.
   * We'll use the heap.
   */
  
  return (0);
}

static void unlock_start(OrchardAppContext *context)
{
  UnlockHandles *p;
  uint8_t i;

  // clear the screen
  gdispClear (Black);

  // aha, you found us!
  dacPlay("fight/leveiup.raw");
  dacWait ();
  /* background */
  putImageFile(IMG_UNLOCKBG, 0, 0);

  // idle ui timer
  orchardAppTimer(context, 1000, true);
  last_ui_time = chVTGetSystemTime();

  p = chHeapAlloc (NULL, sizeof(UnlockHandles));
  init_unlock_ui(p);
  context->priv = p;

  geventListenerInit(&p->glUnlockListener);
  gwinAttachListener(&p->glUnlockListener);
  geventRegisterCallback (&p->glUnlockListener,
                          orchardAppUgfxCallback,
                          &p->glUnlockListener);

  /* starting code */
  for (i=0;i<5;i++) {
    code[i] = 0;
  }
  ledSetFunction(leds_show_unlocks);
  orchardAppTimer(context, 1000, true);  
}

static void updatecode(OrchardAppContext *context, uint8_t position, int8_t direction) {
  GHandle wi;
  UnlockHandles * p;
  char tmp[2];

  p = context->priv;

  code[position] += direction;

  // check boundaries
  if (code[position] & 0x80) {
    code[position] = 0x0f; // underflow of unsigned int
  }

  if (code[position] > 0x0f) {
    code[position] = 0;
  }

  // update labels
  switch(position) {
  case 0:
    wi = p->ghNum1;
    break;
  case 1:
    wi = p->ghNum2;
    break;
  case 2:
    wi = p->ghNum3;
    break;
  case 3:
    wi = p->ghNum4;
    break;
  case 4:
    wi = p->ghNum5;
    break;
  }

  if (code[position] < 0x0a) { 
    tmp[0] = (char) 0x30 + code[position];
  } else {
    tmp[0] = (char) 0x37 + code[position];
  }
  
  tmp[1] = '\0';
  
  gwinSetText(wi, tmp, TRUE);
}

static uint8_t validate_code(OrchardAppContext *context, userconfig *config) { 
  /* check for valid code */
  uint8_t i;
  char tmp[40];
  UnlockHandles *p = context->priv;
  
  for (i=0; i < MAX_ULCODES; i++) {
    
    if ((unlock_codes[i][0] == code[0]) &&
        (unlock_codes[i][1] == ((code[1] << 4) + code[2])) &&
        (unlock_codes[i][2] == ((code[3] << 4) + code[4]))) {
      // set bit
      config->unlocks |= (1 << i);
      
      // if it's the luck upgrade, we set luck directly.
      if (i == 3) {
        config->luck = 40;
      }
      
      strcpy(tmp, unlock_desc[i]);
      strcat(tmp, " unlocked!");
      unlock_result(p, tmp);
        
      ledSetFunction(leds_all_strobe);
      // if it's the bender upgrade, we become bender.
      if (i == 9) {
        config->current_type = p_bender;
        dacPlay("biteass.raw");
      } else {
        // default sound
        dacPlay("fight/leveiup.raw");
      }

      // save to config
      configSave(config);
      
      chThdSleepMilliseconds(ALERT_DELAY);
      orchardAppRun(orchardAppByName("Badge"));
      return true;
    }
  }
  return false;
}


static void unlock_event(OrchardAppContext *context,
                       const OrchardAppEvent *event)
{
  GEvent * pe;
  UnlockHandles * p;
  userconfig *config = getConfig();
  
  p = context->priv;

  // idle timeout
  if (event->type == timerEvent) {
    if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
      orchardAppRun(orchardAppByName("Badge"));
    }
    return;
  }

  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;
    last_ui_time = chVTGetSystemTime();
    
    if (pe->type == GEVENT_GWIN_BUTTON) {
      if ( ((GEventGWinButton*)pe)->gwin == p->ghBack) {
        orchardAppRun(orchardAppByName("Badge"));
        return;
      }
      
      if ( ((GEventGWinButton*)pe)->gwin == p->ghUnlock) {
        if (validate_code(context, config)) {
          return;
        } else {
          ledSetFunction(leds_all_strobered);
          unlock_result(p, "unlock failed.");
          dacPlay("fight/pathtic.raw");
          chThdSleepMilliseconds(ALERT_DELAY);
          orchardAppRun(orchardAppByName("Badge"));
          return;
        }
      }
      
      /* tapping the number advances the value */
      if ( ((GEventGWinButton*)pe)->gwin == p->ghNum1)
        updatecode(context,0,1);

      if ( ((GEventGWinButton*)pe)->gwin == p->ghNum2)
        updatecode(context,1,1);

      if ( ((GEventGWinButton*)pe)->gwin == p->ghNum3)
        updatecode(context,2,1);

      if ( ((GEventGWinButton*)pe)->gwin == p->ghNum4)
        updatecode(context,3,1);

      if ( ((GEventGWinButton*)pe)->gwin == p->ghNum5)
        updatecode(context,4,1);
    }
  
  }

  // keyboard support
  if (event->type == keyEvent) { 
    last_ui_time = chVTGetSystemTime();
    if ( (event->key.code == keyUp) &&
         (event->key.flags == keyPress) ) {
      updatecode(context,focused,1);
    }
    if ( (event->key.code == keyDown) &&
         (event->key.flags == keyPress) ) {
      updatecode(context,focused,-1);
    }
    if ( (event->key.code == keyLeft) &&
         (event->key.flags == keyPress) ) {
      focused--;
      if (focused < 0) focused = 4;
    }
    if ( (event->key.code == keyRight) &&
         (event->key.flags == keyPress) ) {
      focused++;
      if (focused > 4) focused = 0;
    }

    // set focus 
    gwinSetFocus(NULL);
    switch(focused) {
    case 0:
      gwinSetFocus(p->ghNum1);
      break;
    case 1:
      gwinSetFocus(p->ghNum2);
      break;
    case 2:
      gwinSetFocus(p->ghNum3);
      break;
    case 3:
      gwinSetFocus(p->ghNum4);
      break;
    case 4:
      gwinSetFocus(p->ghNum5);
      break;
    default:
      break;
    }
    
    if ( (event->key.code == keySelect) &&
         (event->key.flags == keyPress) ) {
      if (validate_code(context, config)) {
        return;
      } else {
        ledSetFunction(leds_all_strobered);
        unlock_result(p, "unlock failed.");
        dacPlay("fight/pathtic.raw");
        chThdSleepMilliseconds(ALERT_DELAY);
        orchardAppRun(orchardAppByName("Badge"));
        return;
      }
    }
  }
}


static void unlock_exit(OrchardAppContext *context) {
  UnlockHandles * p;

  p = context->priv;

  gdispCloseFont (p->font_manteka_20);
  gdispCloseFont (p->font_jupiterpro_36);
  
  gwinDestroy(p->ghNum1);
  gwinDestroy(p->ghNum2);
  gwinDestroy(p->ghNum3);
  gwinDestroy(p->ghNum4);
  gwinDestroy(p->ghNum5);

  gwinDestroy(p->ghBack);
  gwinDestroy(p->ghUnlock);
  
  geventDetachSource (&p->glUnlockListener, NULL);
  geventRegisterCallback (&p->glUnlockListener, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;

  ledSetFunction(NULL);

}  

/* We are a hidden app, only accessible through the konami code on the
   badge screen. */
orchard_app("Unlocks", TRUE, unlock_init, unlock_start, unlock_event, unlock_exit);
