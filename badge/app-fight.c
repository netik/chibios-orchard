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

typedef struct _FightHandles {
  GListener glFight;
  GHandle ghExitButton;
  GHandle ghNextEnemy;
  GHandle ghPrevEnemy;
} FightHandles;

typedef enum _fight_state {
  ENEMY_SELECT,
  VS_SCREEN,
  FIGHT_APPROVE,
  MOVE_SELECT,
  MOVE_WAITACK,
  RESULTS,
  PLAYER_DEAD,
  ENEMY_DEAD
} fight_state;

static fight_state current_fight_state;

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

static void noRender(GWidgetObject* gw, void* param)
{
  (void)gw;
  (void)param;

  return;
}

static void draw_buttons(FightHandles *p) { 
  GWidgetInit wi;
  coord_t totalheight = gdispGetHeight();

  // Left
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 0;
  wi.g.y = 20;
  wi.g.width = 30;
  wi.g.height = 180;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowLeft;
  p->ghPrevEnemy = gwinButtonCreate(0, &wi);

  // Right
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 290;
  wi.g.y = 20;
  wi.g.width = 30;
  wi.g.height = 180;
  wi.text = "";
  wi.customDraw = gwinButtonDraw_ArrowRight;
  p->ghNextEnemy = gwinButtonCreate(0, &wi);
  
  // Exit
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.width = 40;
  wi.g.height = 40;
  wi.g.y = totalheight - 40;
  wi.g.x = gdispGetWidth() - 40;
  wi.text = "";
  wi.customDraw = noRender;

  p->ghExitButton = gwinButtonCreate(NULL, &wi);
}

static void redraw_enemy_select(void) {
  // enemy selection screen
  font_t fontFF, fontSM;
  const userconfig *config = getConfig();
  
  gdispClear(Black);

  putImageFile(IMG_GUARD_IDLE_L, POS_PCENTER_X, POS_PCENTER_Y);
  putImageFile(IMG_GROUND, 0, POS_FLOOR_Y);

  fontSM = gdispOpenFont (FONT_SM);
  fontFF = gdispOpenFont (FONT_FIXED);
  
  uint16_t ypos = 0; // cursor, so if we move or add things we don't have to rethink this
  uint16_t xpos = 0; // cursor, so if we move or add things we don't have to rethink this
  uint16_t lmargin = 140; // cursor, so if we move or add things we don't have to rethink this

  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      "Select a Challenger",
		      fontSM, Yellow, justifyCenter);

  // slightly above the middle

  char tmp[40];
  chsnprintf(tmp, sizeof(tmp), "LEVEL %d", 10);
  ypos = (gdispGetHeight() / 2) - 60;
  xpos = (gdispGetWidth() / 2) + 10;
  
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      config->name,
		      fontFF, Yellow, justifyLeft);
  ypos = ypos + 20;
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      tmp,
		      fontFF, Yellow, justifyLeft);
}


static uint32_t fight_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static void fight_start(OrchardAppContext *context) {
  FightHandles *p;
  
  (void)context;
  current_fight_state = ENEMY_SELECT;
  redraw_enemy_select();

  p = chHeapAlloc (NULL, sizeof(FightHandles));
  draw_buttons(p);
  context->priv = p;
  
  geventListenerInit(&p->glFight);
  gwinAttachListener(&p->glFight);
  geventRegisterCallback (&p->glFight, orchardAppUgfxCallback, &p->glFight);
}

static void fight_event(OrchardAppContext *context,
			const OrchardAppEvent *event) {
  FightHandles * p;
  p = context->priv;
  
  GEvent * pe;
  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;

    switch(pe->type) {
    case GEVENT_GWIN_BUTTON:
      if (((GEventGWinButton*)pe)->gwin == p->ghExitButton) {
	orchardAppExit();
	return;
      }
      break;
    }
  }
}

static void fight_exit(OrchardAppContext *context) {
  FightHandles * p;

  p = context->priv;
  gwinDestroy (p->ghExitButton);
  gwinDestroy (p->ghNextEnemy);
  gwinDestroy (p->ghPrevEnemy);
  geventDetachSource (&p->glFight, NULL);
  geventRegisterCallback (&p->glFight, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;
  return;
}

orchard_app("FIGHT", 0, fight_init, fight_start, fight_event, fight_exit);


