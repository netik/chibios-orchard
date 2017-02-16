#include "orchard-app.h"
#include "orchard-ui.h"
#include "userconfig.h"
#include "ides-gfx.h"

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
  GHandle gh1Up;
  GHandle gh1Dn;
  GHandle gh2Dn;
  GHandle gh3Dn;
  GHandle gh4Dn;
  GHandle gh5Dn;
  GHandle gh2Up;
  GHandle gh3Up;
  GHandle gh4Up;
  GHandle gh5Up;
  GHandle ghBack;
  GHandle ghUnlock;
  
  // Fonts
  font_t font_dejavu_sans_32;
  font_t font_manteka_20;
  
} UnlockHandles;

static uint32_t last_ui_time = 0;
static uint8_t code[5];
static void updatecode(OrchardAppContext *, uint8_t, int8_t);

static void init_unlock_ui(UnlockHandles *p) {

  GWidgetInit wi;
  
  p->font_dejavu_sans_32 = gdispOpenFont("DejaVuSans32");
  p->font_manteka_20 = gdispOpenFont("manteka20");
  
  gwinWidgetClearInit(&wi);

  // Create label widget: ghNum1
  wi.g.show = TRUE;
  wi.g.x = 50;
  wi.g.y = 100;
  wi.g.width = 20;
  wi.g.height = 30;
  wi.text = "0";
  wi.customDraw = gwinLabelDrawJustifiedCenter;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghNum1 = gwinLabelCreate(0, &wi);
  gwinLabelSetBorder(p->ghNum1, TRUE);
  gwinSetFont(p->ghNum1, p->font_dejavu_sans_32);
  gwinRedraw(p->ghNum1);

  // Create label widget: ghNum2
  wi.g.show = TRUE;
  wi.g.x = 100;
  wi.g.y = 100;
  wi.g.width = 20;
  wi.g.height = 30;
  wi.text = "0";
  wi.customDraw = gwinLabelDrawJustifiedCenter;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghNum2 = gwinLabelCreate(0, &wi);
  gwinLabelSetBorder(p->ghNum2, TRUE);
  gwinSetFont(p->ghNum2, p->font_dejavu_sans_32);
  gwinRedraw(p->ghNum2);

  // Create label widget: ghNum3
  wi.g.show = TRUE;
  wi.g.x = 150;
  wi.g.y = 100;
  wi.g.width = 20;
  wi.g.height = 30;
  wi.text = "0";
  wi.customDraw = gwinLabelDrawJustifiedCenter;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghNum3 = gwinLabelCreate(0, &wi);
  gwinLabelSetBorder(p->ghNum3, TRUE);
  gwinSetFont(p->ghNum3, p->font_dejavu_sans_32);
  gwinRedraw(p->ghNum3);

  // Create label widget: ghNum4
  wi.g.show = TRUE;
  wi.g.x = 200;
  wi.g.y = 100;
  wi.g.width = 20;
  wi.g.height = 30;
  wi.text = "0";
  wi.customDraw = gwinLabelDrawJustifiedCenter;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghNum4 = gwinLabelCreate(0, &wi);
  gwinLabelSetBorder(p->ghNum4, TRUE);
  gwinSetFont(p->ghNum4, p->font_dejavu_sans_32);
  gwinRedraw(p->ghNum4);

  // Create label widget: ghNum5
  wi.g.show = TRUE;
  wi.g.x = 250;
  wi.g.y = 100;
  wi.g.width = 20;
  wi.g.height = 30;
  wi.text = "0";
  wi.customDraw = gwinLabelDrawJustifiedCenter;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghNum5 = gwinLabelCreate(0, &wi);
  gwinLabelSetBorder(p->ghNum5, TRUE);
  gwinSetFont(p->ghNum5, p->font_dejavu_sans_32);
  gwinRedraw(p->ghNum5);

  // create button widget: gh1Up
  wi.g.show = TRUE;
  wi.g.x = 40;
  wi.g.y = 50;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  p->gh1Up = gwinButtonCreate(0, &wi);

  // create button widget: gh1Dn
  wi.g.show = TRUE;
  wi.g.x = 40;
  wi.g.y = 140;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.customDraw = gwinButtonDraw_ArrowDown;
  p->gh1Dn = gwinButtonCreate(0, &wi);

  // create button widget: gh2Dn
  wi.g.show = TRUE;
  wi.g.x = 90;
  wi.g.y = 140;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "";  
  wi.customDraw = gwinButtonDraw_ArrowDown;
  p->gh2Dn = gwinButtonCreate(0, &wi);

  // create button widget: gh3Dn
  wi.g.show = TRUE;
  wi.g.x = 140;
  wi.g.y = 140;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowDown;
  p->gh3Dn = gwinButtonCreate(0, &wi);

  // create button widget: gh4Dn
  wi.g.show = TRUE;
  wi.g.x = 190;
  wi.g.y = 140;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowDown;
  p->gh4Dn = gwinButtonCreate(0, &wi);

  // create button widget: gh5Dn
  wi.g.show = TRUE;
  wi.g.x = 240;
  wi.g.y = 140;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowDown;
  p->gh5Dn = gwinButtonCreate(0, &wi);

  // create button widget: gh2Up
  wi.g.show = TRUE;
  wi.g.x = 90;
  wi.g.y = 50;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  p->gh2Up = gwinButtonCreate(0, &wi);

  // create button widget: gh3Up
  wi.g.show = TRUE;
  wi.g.x = 140;
  wi.g.y = 50;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  p->gh3Up = gwinButtonCreate(0, &wi);

  // create button widget: gh4Up
  wi.g.show = TRUE;
  wi.g.x = 190;
  wi.g.y = 50;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  p->gh4Up = gwinButtonCreate(0, &wi);

  // create button widget: gh5Up
  wi.g.show = TRUE;
  wi.g.x = 240;
  wi.g.y = 50;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowUp;
  p->gh5Up = gwinButtonCreate(0, &wi);

  // create button widget: ghBack
  wi.g.show = TRUE;
  wi.g.x = 10;
  wi.g.y = 210;
  wi.g.width = 60;
  wi.g.height = 20;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowLeft;
  p->ghBack = gwinButtonCreate(0, &wi);

  // create button widget: ghBack
  wi.g.show = TRUE;
  wi.g.x = 200;
  wi.g.y = 200;
  wi.g.width = 110;
  wi.g.height = 30;
  wi.text = "UNLOCK";
  wi.customDraw = 0;
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
    chprintf(stream, "under\r\n");
    code[position] = 0x0f; // underflow of unsigned int
  }

  if (code[position] > 0x0f) {
    chprintf(stream, "over\r\n");
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
  userconfig *config = getConfig();
  UnlockHandles * p;

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
      if ( ((GEventGWinButton*)pe)->gwin == p->gh1Up)
        updatecode(context,0,1);

      if ( ((GEventGWinButton*)pe)->gwin == p->gh2Up)
        updatecode(context,1,1);

      if ( ((GEventGWinButton*)pe)->gwin == p->gh3Up)
        updatecode(context,2,1);

      if ( ((GEventGWinButton*)pe)->gwin == p->gh4Up)
        updatecode(context,3,1);

      if ( ((GEventGWinButton*)pe)->gwin == p->gh5Up)
        updatecode(context,4,1);

      
      if ( ((GEventGWinButton*)pe)->gwin == p->gh1Dn)
        updatecode(context,0,-1);

      if ( ((GEventGWinButton*)pe)->gwin == p->gh2Dn)
        updatecode(context,1,-1);

      if ( ((GEventGWinButton*)pe)->gwin == p->gh3Dn)
        updatecode(context,2,-1);

      if ( ((GEventGWinButton*)pe)->gwin == p->gh4Dn)
        updatecode(context,3,-1);

      if ( ((GEventGWinButton*)pe)->gwin == p->gh5Dn)
        updatecode(context,4,-1);
    }
  }
}


static void unlock_exit(OrchardAppContext *context) {
  UnlockHandles * p;

  p = context->priv;

  gdispCloseFont (p->font_dejavu_sans_32);
  gdispCloseFont (p->font_manteka_20);
  
  gwinDestroy(p->ghNum1);
  gwinDestroy(p->ghNum2);
  gwinDestroy(p->ghNum3);
  gwinDestroy(p->ghNum4);
  gwinDestroy(p->ghNum5);
  gwinDestroy(p->gh1Up);
  gwinDestroy(p->gh1Dn);
  gwinDestroy(p->gh2Dn);
  gwinDestroy(p->gh3Dn);
  gwinDestroy(p->gh4Dn);
  gwinDestroy(p->gh5Dn);
  gwinDestroy(p->gh2Up);
  gwinDestroy(p->gh3Up);
  gwinDestroy(p->gh4Up);
  gwinDestroy(p->gh5Up);
  gwinDestroy(p->ghBack);
  gwinDestroy(p->ghUnlock);
  
  geventDetachSource (&p->glUnlockListener, NULL);
  geventRegisterCallback (&p->glUnlockListener, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;

}  

orchard_app("Unlocks", 0, unlock_init, unlock_start, unlock_event, unlock_exit);
