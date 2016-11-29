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

void noRender(GWidgetObject* gw, void* param)
{
  (void)gw;
  (void)param;

  return;
}

static void draw_badge_buttons(void) {
  GWidgetInit wi;
  coord_t totalheight = gdispGetHeight();
  
  // Apply some default values for GWIN
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.g.y = totalheight - 40;
  wi.g.x = gdispGetWidth() - 40;
  wi.text = "";
  wi.customDraw = noRender;

  ghExitButton = gwinButtonCreate(NULL, &wi);
}

static void redraw_badge(void) {
  // draw the entire background badge image. Shown when the screen is idle. 
  font_t fontLG, fontSM;
  const userconfig *config = getConfig();
  
  gdispClear(Black);

  putImageFile(IMG_GUARD_IDLE_L, POS_PLAYER1_X, POS_PLAYER1_Y);
  putImageFile(IMG_GROUND, 0, POS_FLOOR_Y);

  fontLG = gdispOpenFont (FONT_LG);
  fontSM = gdispOpenFont (FONT_FIXED);

  uint16_t ypos = 0; // cursor, so if we move or add things we don't have to rethink this
  uint16_t lmargin = 140; // cursor, so if we move or add things we don't have to rethink this
  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontLG, fontHeight),
		      config->name,
		      fontLG, Yellow, justifyRight);

  char tmp[20];
  char tmp2[40];
  chsnprintf(tmp, sizeof(tmp), "LEVEL %d", config->level);
  
  /* Level */
  ypos = ypos + gdispGetFontMetric(fontLG, fontHeight);
  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp,
		      fontSM, Yellow, justifyRight);

  /* XP */
  ypos = ypos + gdispGetFontMetric(fontSM, fontHeight) + 4;
  gdispDrawThickLine(165, ypos, 320, ypos, White, 2, FALSE);
  ypos = ypos + 10;
  
  chsnprintf(tmp2, sizeof(tmp2), " XP: %3d GOLD: %3d", config->xp, config->gold);
  gdispDrawStringBox (lmargin,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp2,
		      fontSM, Yellow, justifyLeft);

  /* won/los */
  ypos = ypos + gdispGetFontMetric(fontSM, fontHeight) + 2;
  chsnprintf(tmp2, sizeof(tmp2), "WON: %3d LOST: %3d", config->won, config->lost);
  gdispDrawStringBox (lmargin,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp2,
		      fontSM, Yellow, justifyLeft);

  /* stats */
  ypos = ypos + gdispGetFontMetric(fontSM, fontHeight) + 20;
  chsnprintf(tmp2, sizeof(tmp2), " SPR: %2d   STR: %2d", config->spr, config->str);
  gdispDrawStringBox (lmargin,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp2,
		      fontSM, Yellow, justifyLeft);

  ypos = ypos + gdispGetFontMetric(fontSM, fontHeight) + 2;
  chsnprintf(tmp2, sizeof(tmp2), " DEF: %2d   DEX: %2d", config->def, config->dex);
  gdispDrawStringBox (lmargin,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      tmp2,
		      fontSM, Yellow, justifyLeft);
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


