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
#include "sound.h"

#include "userconfig.h"

// time to wait for a response from either side (mS)
#define DEFAULT_WAIT_TIME 20000

typedef struct _FightHandles {
  GListener glFight;
  GHandle ghExitButton;
  GHandle ghNextEnemy;
  GHandle ghPrevEnemy;
  GHandle ghAttack;
} FightHandles;

typedef enum _fight_state {
  ENEMY_SELECT,
  APPROVAL_DEMAND,
  APPROVAL_WAIT,
  VS_SCREEN,
  MOVE_SELECT,
  MOVE_WAITACK,
  RESULTS,
  PLAYER_DEAD,
  ENEMY_DEAD
} fight_state;

static fight_state current_fight_state;
static user current_enemy;
static int current_enemy_idx = 0;

static uint32_t last_ui_time = 0;
static uint32_t last_tick_time = 0;

static int16_t ticktock = DEFAULT_WAIT_TIME; // used to hold a generic timer value. 

/* prototypes */
/* TODO: move these primitives to some central graphics library */
static int putImageFile(char *name, int16_t x, int16_t y);
static void drawProgressBar(coord_t x, coord_t y, coord_t height, coord_t width, uint16_t maxval, uint16_t currentval, uint8_t use_leds);

static uint8_t prevEnemy(void);
static uint8_t nextEnemy(void);
static uint16_t maxhp(uint8_t level);

static void screen_select_draw(void);
static void screen_select_close(OrchardAppContext *);
static void screen_waitapproval_draw(OrchardAppContext *);

static void start_fight(void);
static void end_fight(void);

#ifdef notyet
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
#endif

static uint16_t maxhp(uint8_t level) {
  // for now
  return ((level-1) * 100) + 337;
}

static void drawProgressBar(coord_t x, coord_t y, coord_t width, coord_t height, uint16_t maxval, uint16_t currentval, uint8_t use_leds) {
  // draw a bargraph.
  color_t c = Lime;

  float remain_f = (float) currentval / (float)maxval;
  int16_t remain = width * remain_f;

 if (use_leds == 1) {
   ledSetProgress(100 * remain_f);
 }
  
  if (remain_f >= 0.8) {
    c = Lime;
  } else if (remain_f >= 0.5) {
    c = Yellow;
  } else {
    c = Red;
  }

  gdispFillArea(x + remain,y+1,(width - remain),height-1, Black);
  gdispFillArea(x,y,remain,height, c);
  gdispDrawBox(x,y,width,height, c);
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

static void draw_select_buttons(FightHandles *p) { 
  GWidgetInit wi;
  coord_t totalheight = gdispGetHeight();
  font_t fontFF;
  fontFF = gdispOpenFont(FONT_FIXED);

  const GWidgetStyle AttackButtonStyle = {
    HTML2COLOR(0x000000),		// window background
    HTML2COLOR(0x2A8FCD),               // focused
    
    // enabled color set
    {
      HTML2COLOR(0xffffff),		// text
      HTML2COLOR(0xC0C0C0),		// edge
      HTML2COLOR(0xf06060),		// fill
      HTML2COLOR(0x404040),		// progress - inactive area
    },
    
    // disabled color set
    {
      HTML2COLOR(0x808080),		// text
      HTML2COLOR(0x404040),		// edge
      HTML2COLOR(0x404040),		// fill
      HTML2COLOR(0x004000),		// progress - active area
    },
    
    // pressed color set
    {
      HTML2COLOR(0xff0000),		// text
      HTML2COLOR(0x00ff00),		// edge
      HTML2COLOR(0x0000ff),		// fill
      HTML2COLOR(0xffff00),		// progress - active area
    },
  };

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

  // Fight
  gwinSetDefaultStyle(&AttackButtonStyle, FALSE);
  gwinSetDefaultFont(gdispOpenFont(FONT_FIXED));
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 70;
  wi.g.y = POS_FLOOR_Y+10;
  wi.g.width = 180;
  wi.g.height = gdispGetFontMetric(fontFF, fontHeight) + 2;
  wi.text = "ATTACK";
  p->ghAttack = gwinButtonCreate(0, &wi);
  gwinSetDefaultStyle(&BlackWidgetStyle, FALSE);
  
  // Exit
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.width = 40;
  wi.g.height = 20;
  wi.g.y = totalheight - 40;
  wi.g.x = gdispGetWidth() - 40;
  wi.text = "";
  wi.customDraw = noRender;
  
  p->ghExitButton = gwinButtonCreate(NULL, &wi);

}

static void screen_alert_draw(char *msg) {
  font_t fontFF;
  gdispClear(Black);
  fontFF = gdispOpenFont (FONT_FIXED);
  gdispDrawStringBox (0,
		      (gdispGetHeight() / 2) - (gdispGetFontMetric(fontFF, fontHeight) / 2),
		      gdispGetWidth(),
		      gdispGetFontMetric(fontFF, fontHeight),
		      msg,
		      fontFF, Red, justifyCenter);
}

static uint8_t nextEnemy() {
  /* walk the list looking for an enemy. */
  user **enemies = enemiesGet();
  int8_t ce = current_enemy_idx;
  uint8_t distance = 0;
  
  do { 
    ce++;
    distance++;
    if (ce > MAX_ENEMIES-1) {
      ce = 0;
    }
  } while ( (enemies[ce] == NULL) && (distance < MAX_ENEMIES) );
		   
  if (enemies[ce] != NULL) {
    current_enemy_idx = ce;
    return TRUE;
  } else {
    // we failed, so time to die.
    screen_alert_draw("NO ENEMIES NEARBY!");
    chThdSleepMilliseconds(1000);
    orchardAppExit();
    return FALSE;
  }
}

static uint8_t prevEnemy() {
  /* walk the list looking for an enemy. */
  user **enemies = enemiesGet();
  int8_t ce = current_enemy_idx;
  uint8_t distance = 0;
  
  do { 
    ce--;
    distance++;
    if (ce < 0) {
      ce = MAX_ENEMIES-1;
    }
  } while ( (enemies[ce] == NULL) && (distance < MAX_ENEMIES) );
		   
  if (enemies[ce] != NULL) {
    current_enemy_idx = ce;
    return TRUE;
  } else {
    // we failed, so time to die.
    screen_alert_draw("NO ENEMIES NEARBY!");
    chThdSleepMilliseconds(1000);
    orchardAppExit();
    return FALSE;
  }
}


static void screen_select_draw(void) {
  // enemy selection screen
  font_t fontFF, fontSM;
  font_t fontXS;
  user **enemies = enemiesGet();

  // blank out the center
  gdispFillArea(31,22,260,POS_FLOOR_Y-22,Black);

  putImageFile(IMG_GUARD_IDLE_L, POS_PCENTER_X, POS_PCENTER_Y);

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
  chsnprintf(tmp, sizeof(tmp), "%d of %d", current_enemy_idx + 1, enemyCount() );
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
		      enemies[current_enemy_idx]->name,
		      fontFF, Yellow, justifyLeft);
  // level
  ypos = ypos + 25;
  chsnprintf(tmp, sizeof(tmp), "LEVEL %d", enemies[current_enemy_idx]->level);
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      tmp,
		      fontFF, Yellow, justifyLeft);

  ypos = ypos + 50;
  chsnprintf(tmp, sizeof(tmp), "HP %d", enemies[current_enemy_idx]->hp);
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      tmp,
		      fontFF, Yellow, justifyLeft);

  ypos = ypos + gdispGetFontMetric(fontFF, fontHeight) + 5;
  
  drawProgressBar(xpos,ypos,100,10,enemies[current_enemy_idx]->hp,maxhp(enemies[current_enemy_idx]->level), 0);
}

static void screen_select_close(OrchardAppContext *context) {
  FightHandles * p;

  // shut down the select screen 
  gdispClear(Black);

  p = context->priv;
  gwinDestroy (p->ghExitButton);
  gwinDestroy (p->ghNextEnemy);
  gwinDestroy (p->ghPrevEnemy);
  gwinDestroy (p->ghAttack);
}

static uint32_t fight_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static void fight_start(OrchardAppContext *context) {
  FightHandles *p;

  user **enemies = enemiesGet();
  (void)context;
  p = chHeapAlloc (NULL, sizeof(FightHandles));
  memset(p, 0, sizeof(FightHandles));
  context->priv = p;
  
  // fires once a second for updates. 
  orchardAppTimer(context, 1000, true);
  last_ui_time = chVTGetSystemTime();

  if (enemyCount() > 0) {
    // render the select page. 
    gdispClear(Black);

    current_fight_state = ENEMY_SELECT;
    putImageFile(IMG_GROUND, 0, POS_FLOOR_Y);

    screen_select_draw();
    draw_select_buttons(p);
    
    geventListenerInit(&p->glFight);
    gwinAttachListener(&p->glFight);
    geventRegisterCallback (&p->glFight, orchardAppUgfxCallback, &p->glFight);

    if (enemies[current_enemy_idx] == NULL) {
      nextEnemy();
    }

  } else {
    // punt if no enemies
    screen_alert_draw("NO ENEMIES NEARBY!");
    chThdSleepMilliseconds(1000);
    orchardAppExit();
  }
}

static void screen_waitapproval_draw(OrchardAppContext *context) {
  (void)context;
  
  // Create label widget: ghLabel1
  screen_alert_draw("WAITING FOR ENEMY TO ACCEPT!");

  // progess bar
  ticktock = DEFAULT_WAIT_TIME; // used to hold a generic timer value.
  drawProgressBar(40,(gdispGetHeight() / 2) + 60,240,20,DEFAULT_WAIT_TIME,ticktock, 1);
  last_tick_time = chVTGetSystemTime();
}

static void start_fight(void) {
  // set my incombat to true
  KW01_PKT pkt;
  userconfig *config = getConfig();
  user **enemies = enemiesGet();
  user upkt;
    
  config->in_combat = 1;

  // clear the packet
  memset (pkt.kw01_payload, 0, sizeof(pkt.kw01_payload));

  // copy data
  memcpy(&current_enemy, enemies[current_enemy_idx], sizeof(user));

  // send one ping. 
  upkt.ttl = 4;
  strncpy(upkt.name, config->name, CONFIG_NAME_MAXLEN);
  upkt.in_combat = 1; // doesn't matter
  upkt.hp = config->hp;
  upkt.level = config->level;
  upkt.p_type = config->p_type;
  
  memcpy(pkt.kw01_payload, &upkt, sizeof(upkt));
	 
  radioSend (&KRADIO1, current_enemy.netid,
	     RADIO_PROTOCOL_STARTBATTLE,
	     KRADIO1.kw01_maxlen - KW01_PKT_HDRLEN,
	     &pkt);
}

static void end_fight(void) {
  // set my incombat to true
  userconfig *config = getConfig();
  config->in_combat = 0;
}

static void fight_event(OrchardAppContext *context,
			const OrchardAppEvent *event) {

  FightHandles * p = context->priv;
  GEvent * pe;
  
  // the timerEvent fires once a second and is overloaded a bit to handle
  // real-time functionality in the fight sequence
  //
  // this returns us to the badge screen on idle.
  // we don't want this for all states, just select and end-of-fight
  if ( (event->type == timerEvent) && current_fight_state == ENEMY_SELECT ) {
      if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
	orchardAppRun(orchardAppByName("Badge"));
      }
      return;
  }
  
  if ( (event->type == timerEvent) &&
       (current_fight_state == APPROVAL_WAIT) &&
       ((chVTGetSystemTime() - last_tick_time) > 1000) ) {
    ticktock = ticktock - 1000;
    last_tick_time = chVTGetSystemTime();

    // progess bar
    drawProgressBar(40,(gdispGetHeight() / 2) + 60,240,20,DEFAULT_WAIT_TIME,ticktock, 1);
    if (ticktock <= 0) { 
      screen_alert_draw("TIMED OUT!");
      ledSetProgress(-1);
      end_fight();
      playHardFail();
      chThdSleepMilliseconds(2000);
      orchardAppRun(orchardAppByName("Badge"));
    }

    return;
  }
  
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
	    screen_select_draw();
	  }
	  return;
	}
	if (((GEventGWinButton*)pe)->gwin == p->ghAttack) {
	  last_ui_time = chVTGetSystemTime();
	  current_fight_state = APPROVAL_WAIT;
	  screen_select_close(context);
	  start_fight();
	  screen_waitapproval_draw(context);
	  return;
	}
	if (((GEventGWinButton*)pe)->gwin == p->ghPrevEnemy) {
	  if (prevEnemy()) {
	    last_ui_time = chVTGetSystemTime();
	    screen_select_draw();
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
  gwinDestroy (p->ghAttack);
  
  geventDetachSource (&p->glFight, NULL);
  geventRegisterCallback (&p->glFight, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;
  return;
}

orchard_app("FIGHT", 0, fight_init, fight_start, fight_event, fight_exit);


