#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "orchard-app.h"
#include "led.h"
#include "radio.h"
#include "orchard-ui.h"
#include "images.h"

#include "userconfig.h"

static GHandle ghExitButton;
static GListener glBadge;

static int putImageFile(char *name, int16_t x, int16_t y);

static int putImageFile(char *name, int16_t x, int16_t y) {

  gdispImage img;
    
  if (gdispImageOpenFile (&img, name) == GDISP_IMAGE_ERR_OK) {
    gdispImageDraw (&img,
		    x, y,
		    img.width,
		    img.height, 0, 0);
    
    gdispImageClose (&img);
    return(1);
  } else {
    chprintf(stream, "\r\ncan't load image %s!!\r\n", name);
    return(0);
  }    

}
static void draw_badge_buttons(void) {
  GWidgetInit wi;
  coord_t totalheight = gdispGetHeight();
  
  // Apply some default values for GWIN
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.width = 80;
  wi.g.height = 30;
  wi.g.y = totalheight - 40;
  wi.g.x = 2;
  wi.text = "<--";

  ghExitButton = gwinButtonCreate(NULL, &wi);
}

static void redraw_badge(void) {
  // draw the entire background badge image. Shown when the screen is idle. 
  font_t fontLG, fontSM;
  const userconfig *config = getConfig();
  
  gdispClear(Black);

  putImageFile(IMG_GUARD_IDLE_L, POS_PLAYER1_X, POS_PLAYER1_Y);
  gdispDrawThickLine(0, POS_FLOOR_Y, 320, POS_FLOOR_Y, White, 2, FALSE);

  fontLG = gdispOpenFont ("augustus36");
  fontSM = gdispOpenFont ("DejaVuSans32");
  coord_t leftpos = 165;

  gdispDrawStringBox (leftpos,
		      gdispGetHeight() / 2 - (gdispGetFontMetric(fontLG, fontHeight) / 2),
		      gdispGetWidth() - leftpos,
		      gdispGetFontMetric(fontLG, fontHeight),
		      config->name,
		      fontLG, Yellow, justifyRight);

  char tmp[20];
  chsnprintf(tmp, sizeof(tmp), "LEVEL %d", config->level);

  /* Level */
  gdispDrawStringBox (leftpos,
		      50,
		      gdispGetWidth() - leftpos,
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp,
		      fontSM, Yellow, justifyRight);
}

static uint32_t default_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static void default_start(OrchardAppContext *context) {
  (void)context;
  redraw_badge();
  draw_badge_buttons();
  
  geventListenerInit(&glBadge);
  gwinAttachListener(&glBadge);
  geventRegisterCallback (&glBadge, orchardAppUgfxCallback, &glBadge);
}

void default_event(OrchardAppContext *context, const OrchardAppEvent *event) {
  (void)context;
  GEvent * pe;
  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;

    switch(pe->type) {
    case GEVENT_GWIN_BUTTON:
      if (((GEventGWinButton*)pe)->gwin == ghExitButton) {
	orchardAppExit();
	return;
      }
      break;
    }
  }
}

static void default_exit(OrchardAppContext *context) {
  (void)context;
  gwinDestroy (ghExitButton);
  geventDetachSource (&glBadge, NULL);
  geventRegisterCallback (&glBadge, NULL, NULL);
  return;
}

orchard_app("Badge", default_init, default_start, default_event, default_exit);


