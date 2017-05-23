/*
 * app-verify-update.c
 *
 * Runs before the firmware update to confirm update
 *
 */

#include <string.h>
#include "orchard-app.h"
#include "orchard-ui.h"
#include "ides_gfx.h"

typedef struct _VHandles {
  GListener glCtListener;
  GHandle ghOK;
  GHandle ghCANCEL;
  font_t font_manteka_20;
} VHandles;

static uint32_t last_ui_time = 0;

static uint32_t vu_init(OrchardAppContext *context)
{
  (void)context;
  
  /*
   * We don't want any extra stack space allocated for us.
   * We'll use the heap.
   */
  
  return (0);
}



static void vu_start(OrchardAppContext *context) {
  VHandles *p;
  GWidgetInit wi;
  
  // clear the screen
  gdispClear (Black);

  // idle ui timer
  last_ui_time = chVTGetSystemTime();

  p = chHeapAlloc (NULL, sizeof(VHandles));
  memset(p, 0, sizeof(VHandles));
  p->font_manteka_20 = gdispOpenFont("manteka20");

  context->priv = p;
  
  // draw UI
  gwinSetDefaultFont (p->font_manteka_20);

  gdispDrawStringBox(0,
                     40,
                     320,
                     20,
                     "UPDATE FIRMWARE",
                     p->font_manteka_20,
                     White, justifyCenter);

  gdispDrawStringBox(0,
                     100,
                     320,
                     20,
                     "ARE YOU SURE?",
                     p->font_manteka_20,
                     Red, justifyCenter);

  
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 0;
  wi.g.y = 210;
  wi.g.width = 150;
  wi.g.height = 30;
  wi.text = "CANCEL";
  p->ghCANCEL = gwinButtonCreate(0, &wi);
  
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

  geventListenerInit(&p->glCtListener);
  gwinAttachListener(&p->glCtListener);
  geventRegisterCallback (&p->glCtListener,
                          orchardAppUgfxCallback,
                          &p->glCtListener);

  orchardAppTimer(context, 500000, true);
}

static void vu_event(OrchardAppContext *context,
                       const OrchardAppEvent *event)
{
  GEvent * pe;
  VHandles * p;
  
  p = context->priv;

  if (event->type == timerEvent) {
    return;
  }

  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;
    last_ui_time = chVTGetSystemTime();
    
    if (pe->type == GEVENT_GWIN_BUTTON) {
      if ( ((GEventGWinButton*)pe)->gwin == p->ghOK) {
        orchardAppRun(orchardAppByName("UFW2"));
        return;
      }
      if ( ((GEventGWinButton*)pe)->gwin == p->ghCANCEL) {
        orchardAppRun(orchardAppByName("Launcher"));
        return;
      }
    }
  }
}
static void vu_exit(OrchardAppContext *context) {
  VHandles * p;

  p = context->priv;
  gdispCloseFont (p->font_manteka_20);
  gwinDestroy(p->ghOK);
  gwinDestroy(p->ghCANCEL);

  geventDetachSource (&p->glCtListener, NULL);
  geventRegisterCallback (&p->glCtListener, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;

}

orchard_app("Update FW", "update.rgb", 0,
            vu_init, vu_start, vu_event, vu_exit, 9999);
