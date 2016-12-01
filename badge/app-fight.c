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
static int current_enemy = 0;

/* prototypes */
static int putImageFile(char *name, int16_t x, int16_t y);
static uint8_t prevEnemy(void);
static uint8_t nextEnemy(void);

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

static void redraw_no_enemies(void) {
  font_t fontFF;
  gdispClear(Black);
  fontFF = gdispOpenFont (FONT_FIXED);

  gdispDrawStringBox (0,
		      (gdispGetHeight() / 2) - (gdispGetFontMetric(fontFF, fontHeight) / 2),
		      gdispGetWidth(),
		      gdispGetFontMetric(fontFF, fontHeight),
		      "NO ENEMIES NEARBY!",
		      fontFF, Red, justifyCenter);
  
}

static uint8_t nextEnemy() {
  /* walk the list looking for an enemy. */
  user **enemies = enemiesGet();
  int8_t ce = current_enemy;
  uint8_t distance = 0;
  
  do { 
    ce++;
    distance++;
    if (ce > MAX_ENEMIES-1) {
      ce = 0;
    }
  } while ( (enemies[ce] == NULL) && (distance < MAX_ENEMIES) );
		   
  if (enemies[ce] != NULL) {
    current_enemy = ce;
    return TRUE;
  } else {
    // we failed, so time to die.
    redraw_no_enemies();
    chThdSleepMilliseconds(1000);
    orchardAppExit();
    return FALSE;
  }
}

static uint8_t prevEnemy() {
  /* walk the list looking for an enemy. */
  user **enemies = enemiesGet();
  int8_t ce = current_enemy;
  uint8_t distance = 0;
  
  do { 
    ce--;
    distance++;
    if (ce < 0) {
      ce = MAX_ENEMIES-1;
    }
  } while ( (enemies[ce] == NULL) && (distance < MAX_ENEMIES) );
		   
  if (enemies[ce] != NULL) {
    current_enemy = ce;
    return TRUE;
  } else {
    // we failed, so time to die.
    redraw_no_enemies();
    chThdSleepMilliseconds(1000);
    orchardAppExit();
    return FALSE;
  }
}

static void redraw_enemy_select(void) {
  // enemy selection screen
  font_t fontFF, fontSM;
  user **enemies = enemiesGet();

  // blank out the center
  gdispFillArea(31,22,260,POS_FLOOR_Y-22,Black);
    
  putImageFile(IMG_GUARD_IDLE_L, POS_PCENTER_X, POS_PCENTER_Y);
  putImageFile(IMG_GROUND, 0, POS_FLOOR_Y);

  fontSM = gdispOpenFont (FONT_SM);
  fontFF = gdispOpenFont (FONT_FIXED);
  
  uint16_t ypos = 0; // cursor, so if we move or add things we don't have to rethink this
  uint16_t xpos = 0; // cursor, so if we move or add things we don't have to rethink this

  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      "Select a Challenger",
		      fontSM, Yellow, justifyCenter);

  // slightly above the middle

  char tmp[40];
  chsnprintf(tmp, sizeof(tmp), "LEVEL %d", enemies[current_enemy]->level);
  ypos = (gdispGetHeight() / 2) - 60;
  xpos = (gdispGetWidth() / 2) + 10;
  
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      enemies[current_enemy]->name,
		      fontFF, Yellow, justifyLeft);
  // level
  ypos = ypos + 25;
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
  if (enemyCount() > 0) { 
    current_fight_state = ENEMY_SELECT;
    gdispClear(Black);
    redraw_enemy_select();
  } else {
    // punt if no enemies
    redraw_no_enemies();
    chThdSleepMilliseconds(1000);
    orchardAppExit();
  }

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
      if (((GEventGWinButton*)pe)->gwin == p->ghNextEnemy) {
	if (nextEnemy()) { 
	  redraw_enemy_select();
	}
	return;
      }
      if (((GEventGWinButton*)pe)->gwin == p->ghPrevEnemy) {
	if (prevEnemy()) { 
	  redraw_enemy_select();
	}
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


