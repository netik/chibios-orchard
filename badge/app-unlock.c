/*
 * app-unlock.c
 * 
 * the unlock screen that allows people to unlock features in 
 * the badge
 *
 */

#include "orchard-app.h"
#include "orchard-ui.h"
#include "userconfig.h"
#include "ides_gfx.h"
#include "images.h"


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

#define MAX_ULCODES 9

/* codes, packed as bits */
static uint8_t unlock_codes[MAX_ULCODES][3] = { { 0x01, 0xef, 0xf1 }, // 0
                                                { 0x0d, 0xef, 0xad }, // 1
                                                { 0x0a, 0x7a, 0xa7 }, // 2
                                                { 0x07, 0x70, 0x07 }, // 3
                                                { 0x0a, 0xed, 0x17 }, // 4
                                                { 0x00, 0x1a, 0xc0 }, // 5
                                                { 0x0b, 0xae, 0xac }, // 6
                                                { 0x0d, 0xe0, 0x1a }, // 7
                                                { 0x09, 0x00, 0x46 }  // 8
};
static char *unlock_desc[] = { "+10% DEF",
                               "+10% HP",
                               "+1 MIGHT",
                               "+20% LUCK",
                               "FASTER HEAL",
                               "LEDs",
                               "CAESAR",
                               "SENATOR",
                               "TIMELORD" };

static uint32_t last_ui_time = 0;
static uint8_t code[5];
static void updatecode(OrchardAppContext *, uint8_t, int8_t);

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
  wi.g.show = TRUE;
  wi.g.x = 140;
  wi.g.y = 40;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "0";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customParam = 0;
  wi.customStyle = &ivory;
  p->ghNum3 = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghNum3, p->font_jupiterpro_36);
  gwinRedraw(p->ghNum3);

  // create button widget: ghNum4
  wi.g.show = TRUE;
  wi.g.x = 190;
  wi.g.y = 40;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "0";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customParam = 0;
  wi.customStyle = &ivory;
  p->ghNum4 = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghNum4, p->font_jupiterpro_36);
  gwinRedraw(p->ghNum4);

  // create button widget: ghNum5
  wi.g.show = TRUE;
  wi.g.x = 240;
  wi.g.y = 40;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "0";
  wi.customDraw = gwinButtonDraw_Normal;
  wi.customParam = 0;
  wi.customStyle = &ivory;
  p->ghNum5 = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghNum5,  p->font_jupiterpro_36);
  gwinRedraw(p->ghNum5);
  
  // create button widget: ghBack
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 100;
  wi.g.width = 110;
  wi.g.height = 30;
  wi.text = "<-";
  wi.customStyle = &ivory;
  p->ghBack = gwinButtonCreate(0, &wi);

  // create button widget: ghUnlock
  wi.g.show = TRUE;
  wi.g.x = 200;
  wi.g.y = 100;
  wi.g.width = 110;
  wi.g.height = 30;
  wi.text = "UNLOCK";
  wi.customDraw = 0;
  wi.customStyle = &ivory;
  p->ghUnlock = gwinButtonCreate(0, &wi);
  gwinSetFont(p->ghUnlock, p->font_manteka_20);
  gwinRedraw(p->ghUnlock);
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


static void unlock_event(OrchardAppContext *context,
                       const OrchardAppEvent *event)
{
  GEvent * pe;
  UnlockHandles * p;
  uint8_t i;

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
        orchardAppExit();
        return;
      }
      
      if ( ((GEventGWinButton*)pe)->gwin == p->ghUnlock) {
        /* ceck for valid code */
        for (i=0; i < MAX_ULCODES; i++) {

          if ((unlock_codes[i][0] == codes[0]) &&
              (unlock_codes[i][1] == ((codes[1] << 4) + codes[2])) &&
              (unlock_codes[i][2] == ((codes[3] << 4) + codes[4]))) {
            printf("match! %d\n" , i);
            // set bit 

            // save to config

            // display message -- should we tell them? or just say unlock successful?

          }
        }


        orchardAppExit();
        return;
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

      /* TODO: support buttons */

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

}  

orchard_app("Unlocks", 0, unlock_init, unlock_start, unlock_event, unlock_exit);
