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
static uint32_t last_ui_time = 0;

/* prototypes */
static int putImageFile(char *name, int16_t x, int16_t y);
static void drawHPBox(coord_t x, coord_t y, coord_t width, uint16_t hp, uint16_t maxhp);
static uint8_t prevEnemy(void);
static uint8_t nextEnemy(void);
static uint16_t maxhp(uint8_t level);

/* levels */
static uint16_t base_xp[10] = {
  0,
  400,
  800,
  1200,
  1800,
  2600,
  3600,
  4800,
  6200,
  7800
};

static uint8_t calc_level(uint16_t xp) {
  if(xp >= 7800) {
    return 10;
  }
  if(xp >= 6200) {
    return 9;
  }
  if(xp >= 4800) {
    return 8;
  }
  if(xp >= 3600) {
    return 7;
  }
  if(xp >= 2600) {
    return 6;
  }
  if(xp >= 1800) {
    return 5;
  }
  if(xp >= 1200) {
    return 4;
  }
  if(xp >= 800) {
    return 3;
  }
  if(xp >= 400) {
    return 2;
  }
  return 1;
}

static uint16_t maxhp(uint8_t level) {
  // for now
  return 1337;
}

static void drawHPBox(coord_t x, coord_t y, coord_t width, uint16_t hp, uint16_t maxhp) {
  // draw a HP bargraph.
  color_t c = Lime;

  float remain_f = (float) hp / (float)maxhp;
  int remain = width * remain_f;
  
  if (remain_f >= 0.8) {
    c = Lime;
  } else if (remain_f >= 0.5) {
    c = Yellow;
  } else {
    c = Red;
  }
  
  gdispDrawBox(x,y,width,10, c);
  gdispFillArea(x,y,remain,10, c);
}

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
  font_t fontXS;
  user **enemies = enemiesGet();

  // blank out the center
  gdispFillArea(31,22,260,POS_FLOOR_Y-22,Black);
    
  putImageFile(IMG_GUARD_IDLE_L, POS_PCENTER_X, POS_PCENTER_Y);
  putImageFile(IMG_GROUND, 0, POS_FLOOR_Y);

  fontSM = gdispOpenFont (FONT_SM);
  fontFF = gdispOpenFont (FONT_FIXED);
  fontXS = gdispOpenFont (FONT_XS);
  
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
  ypos = (gdispGetHeight() / 2) - 60;
  xpos = (gdispGetWidth() / 2) + 10;

  // count
  chsnprintf(tmp, sizeof(tmp), "%d of %d", current_enemy + 1, enemyCount() );
  gdispDrawStringBox (xpos,
		      ypos - 20,
		      gdispGetWidth() - xpos - 30,
		      gdispGetFontMetric(fontXS, fontHeight),
		      tmp,
		      fontXS, White, justifyRight);
  // who 
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      enemies[current_enemy]->name,
		      fontFF, Yellow, justifyLeft);
  // level
  ypos = ypos + 25;
  chsnprintf(tmp, sizeof(tmp), "LEVEL %d", enemies[current_enemy]->level);
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      tmp,
		      fontFF, Yellow, justifyLeft);

  ypos = ypos + 50;
  chsnprintf(tmp, sizeof(tmp), "HP %d", enemies[current_enemy]->hp);
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      tmp,
		      fontFF, Yellow, justifyLeft);

  ypos = ypos + gdispGetFontMetric(fontFF, fontHeight) + 5;
  
  drawHPBox(xpos,ypos,100,enemies[current_enemy]->hp,maxhp(enemies[current_enemy]->level));
  
}


static uint32_t fight_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static void fight_start(OrchardAppContext *context) {
  FightHandles *p;
  user **enemies = enemiesGet();
  (void)context;

  // fires once a second for updates. 
  orchardAppTimer(context, 1000, true);
  last_ui_time = chVTGetSystemTime();
  
  if (enemyCount() > 0) { 
    current_fight_state = ENEMY_SELECT;
    gdispClear(Black);

    if (enemies[current_enemy] == NULL) {
      nextEnemy();
    }
    
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
  if (event->type == timerEvent) {
    if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
      orchardAppRun(orchardAppByName("Badge"));
    }
    return;
  }
  else 
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
	    last_ui_time = chVTGetSystemTime();
	    redraw_enemy_select();
	  }
	  return;
	}
	if (((GEventGWinButton*)pe)->gwin == p->ghPrevEnemy) {
	  if (prevEnemy()) {
	    last_ui_time = chVTGetSystemTime();
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


