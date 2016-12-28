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

// 66666 uS = 15 FPS. Eeeviil... 
#define FRAME_INTERVAL_US 66666

// time to wait for a response from either side (mS)
// WAIT_TIMEs are always in system ticks.
// caveat! the system timer is a uint32_t and can roll over! be aware!

#define DEFAULT_WAIT_TIME MS2ST(20000) // was 20k, now 5k for testing
#define MAX_ACKWAIT MS2ST(5000) // was 20k, now 5k for testing
#define MOVE_WAIT_TIME MS2ST(20000)    // should be a bit faster to push people! 
#define ALERT_DELAY 1500

typedef struct _FightHandles {
  GListener glFight;
  GHandle ghExitButton;
  GHandle ghNextEnemy;
  GHandle ghPrevEnemy;
  GHandle ghAttack;
  GHandle ghDeny;
  GHandle ghAccept;

  GHandle ghBlockHi;
  GHandle ghBlockMid;
  GHandle ghBlockLow;
  GHandle ghAttackHi;
  GHandle ghAttackMid;
  GHandle ghAttackLow;
} FightHandles;

typedef enum _fight_state {
  IDLE,
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


// WidgetStyle: RedButton
const GWidgetStyle RedButtonStyle = {
  HTML2COLOR(0xff0000),              // background
  HTML2COLOR(0xff6666),              // focus

  // Enabled color set
  {
    HTML2COLOR(0xffffff),         // text
    HTML2COLOR(0x800000),         // edge
    HTML2COLOR(0xff0000),         // fill
    HTML2COLOR(0x008000),         // progress (inactive area)
  },

  // Disabled color set
  {
    HTML2COLOR(0x808080),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0x404040),         // fill
    HTML2COLOR(0x004000),         // progress (active area)
  },

  // Pressed color set
  {
    HTML2COLOR(0xFFFFFF),         // text
    HTML2COLOR(0x800000),         // edge
    HTML2COLOR(0xff6a71),         // fill
    HTML2COLOR(0x008000),         // progress (active area)
  }
};

extern orchard_app_instance instance;

static int32_t countdown = DEFAULT_WAIT_TIME; // used to hold a generic timer value. 
static fight_state current_fight_state = IDLE;

static user current_enemy;
static uint8_t current_enemy_idx = 0;

static uint32_t last_ui_time = 0;
static uint32_t last_tick_time = 0;

static uint8_t attackbitmap = 0;

// true if attacking, false if blocking.
static uint8_t isattacking = false;

/* prototypes */
/* TODO: move these primitives to some central graphics library */
static int putImageFile(char *name, int16_t x, int16_t y);
static void drawProgressBar(coord_t x, coord_t y, coord_t width, coord_t height, int32_t maxval, int32_t currentval, uint8_t use_leds, uint8_t reverse);
static void blinkText (coord_t x, coord_t y,coord_t cx, coord_t cy, char *text, font_t font, color_t color, justify_t justify, uint8_t times, int16_t delay);

static void clearstatus(void);

static uint8_t prevEnemy(void);
static uint8_t nextEnemy(void);

/* the fight sequence */
static void screen_select_draw(int8_t);
static void screen_select_close(OrchardAppContext *);
static void screen_waitapproval_draw(void);
static void screen_demand_draw(void);
static void screen_vs_draw(void);
static void show_results(void);
static void draw_attack_buttons(void);

static void end_fight(void);

static void clearstatus(void) {
  // clear instruction area
  gdispFillArea(0,
		38,
		gdispGetWidth(),
		23,
		Black);

  // remove progress bar if any
  gdispFillArea(0,gdispGetHeight() - 20,gdispGetWidth(),20,Black);

}

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

static void blinkText (coord_t x, coord_t y,coord_t cx, coord_t cy, char *text, font_t font, color_t color, justify_t justify, uint8_t times, int16_t delay) {
  int8_t blink=1; 
  for (int i=0; i < times; i++) {
    if (blink == 1) {
      gdispDrawStringBox (x,y,cx,cy,text,font,color,justify);
      gdispFlush();
    } else {
      gdispDrawStringBox (x,y,cx,cy,text,font,Black,justify);
      gdispFlush();
    }
    if (blink == 1) { blink = 0; } else { blink = 1; };
    
    chThdSleepMilliseconds(delay);
  }
}

static void drawProgressBar(coord_t x, coord_t y, coord_t width, coord_t height, int32_t maxval, int32_t currentval, uint8_t use_leds, uint8_t reverse) {
  // draw a bar
  // if reverse is true, we draw right to left vs left to right
  color_t c = Lime;

  if (currentval < 0) { currentval = 0; } // never overflow
  
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

  if (reverse) { 
    gdispFillArea(x,y+1,(width - remain)-1,height-2, Black);
    gdispFillArea((x+width)-remain,y,remain,height, c);
    gdispDrawBox(x,y,width,height, c);
  } else {
    gdispFillArea(x + remain,y+1,(width - remain)-1,height-2, Black);
    gdispFillArea(x,y,remain,height, c);
    gdispDrawBox(x,y,width,height, c);
  }
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


static void draw_attack_icons(void) {
  clearstatus();
  // clear middle
  gdispFillArea(140,70,40,110,Black);

  if (attackbitmap & ATTACK_HI) {
    putImageFile(IMG_GATTH1, POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(IMG_ATTACK,160,70);
  }

  if (attackbitmap & ATTACK_MID) {
    putImageFile(IMG_GATTM1, POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(IMG_ATTACK,160,115);
  }
  
  if (attackbitmap & ATTACK_LOW) {
    putImageFile(IMG_GATTL1, POS_PLAYER1_X, POS_PCENTER_Y);
    putImageFile(IMG_ATTACK,160,160);
  }

  if (attackbitmap & BLOCK_HI) 
    putImageFile(IMG_BLOCK,140,70);

  if (attackbitmap & BLOCK_MID) 
    putImageFile(IMG_BLOCK,140,115);

  if (attackbitmap & BLOCK_LOW)
    putImageFile(IMG_BLOCK,140,160);

}

static void draw_attack_buttons(void) {
  // create button widget: ghBlockhi
  GWidgetInit wi;
  FightHandles *p;
  
  gwinWidgetClearInit(&wi);
  p = instance.context->priv;
  
  wi.g.show = TRUE;
  wi.g.x = 0;
  wi.g.y = 50;
  wi.g.width = 140;
  wi.g.height = 59;
  wi.text = "1";
  wi.customDraw = noRender;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghBlockHi = gwinButtonCreate(0, &wi);
  
  // create button widget: ghBlockMid
  wi.g.show = TRUE;
  wi.g.x = 0;
  wi.g.y = 110;
  wi.g.width = 140;
  wi.g.height = 56;
  wi.text = "2";
  wi.customDraw = noRender;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghBlockMid = gwinButtonCreate(0, &wi);
  
  // create button widget: ghBlockLow
  wi.g.show = TRUE;
  wi.g.x = 0;
  wi.g.y = 160;
  wi.g.width = 140;
  wi.g.height = 56;
  wi.text = "3";
  wi.customDraw = noRender;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghBlockLow = gwinButtonCreate(0, &wi);
  
  // create button widget: ghAttackHi
  wi.g.show = TRUE;
  wi.g.x = 180;
  wi.g.y = 50;
  wi.g.width = 140;
  wi.g.height = 59;
  wi.text = "4";
  wi.customDraw = noRender;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghAttackHi = gwinButtonCreate(0, &wi);
  
  // create button widget: ghAttackMid
  wi.g.show = TRUE;
  wi.g.x = 180;
  wi.g.y = 110;
  wi.g.width = 140;
  wi.g.height = 56;
  wi.text = "5";
  wi.customDraw = noRender;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghAttackMid = gwinButtonCreate(0, &wi);
  
  // create button widget: ghAttackLow
  wi.g.show = TRUE;
  wi.g.x = 180;
  wi.g.y = 160;
  wi.g.width = 140;
  wi.g.height = 56;
  wi.text = "6";
  wi.customDraw = noRender;
  wi.customParam = 0;
  wi.customStyle = 0;
  p->ghAttackLow = gwinButtonCreate(0, &wi);

  //  // line these dudes
  //  for (int i = 0; i < 2; i++) {
  //    gdispDrawThickLine(0, 98 + (i * 53), 130, 98 + (i * 53), Red, 2, FALSE);
  //    gdispDrawThickLine(190, 98 + (i * 53), 320, 98 + (i * 53), Red, 2, FALSE);
  //  }

}
  
static void draw_select_buttons(FightHandles *p) { 

  GWidgetInit wi;
  coord_t totalheight = gdispGetHeight();
  font_t fontFF;
  fontFF = gdispOpenFont(FONT_FIXED);

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
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);
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
    // we failed, so time to die
    screen_alert_draw("NO ENEMIES NEARBY!");
    chThdSleepMilliseconds(ALERT_DELAY);
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
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppExit();
    return FALSE;
  }
}

static void show_results(void) {
  // remove the progress bar and status bar
  clearstatus();
  
  // animate the characters
  for (uint8_t i=0; i< 5; i++) { 
    putImageFile(IMG_GUARD_IDLE_L, POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(IMG_GUARD_IDLE_R, POS_PLAYER2_X, POS_PLAYER2_Y);
    chThdSleepMilliseconds(200);

    // todo - use the player-selected attack.
    putImageFile(IMG_GATTH1, POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(IMG_GATTH1R, POS_PLAYER2_X, POS_PLAYER2_Y);
    chThdSleepMilliseconds(200);
  }
    
  // calculate damages and show damage

  // update the health bars

  // if not dead transition state back to move_select
  current_fight_state = MOVE_SELECT;
  countdown=MOVE_WAIT_TIME;
  last_tick_time = chVTGetSystemTime();

  // if either side dies update states and exit app
  
}

static void screen_select_draw(int8_t initial) {
  // enemy selection screen
  // setting initial to TRUE will cause a repaint of the entire
  // scene. We set this to FALSE on next/previous moves to improve
  // redraw performance
  font_t fontFF, fontSM;
  font_t fontXS;
  user **enemies = enemiesGet();
  if (initial == TRUE) { 
    gdispClear(Black);
    current_fight_state = ENEMY_SELECT;
    putImageFile(IMG_GROUND, 0, POS_FLOOR_Y);
  }

  // blank out the center
  gdispFillArea(31,22,260,POS_FLOOR_Y-22,Black);
  putImageFile(IMG_GUARD_IDLE_L, POS_PCENTER_X, POS_PCENTER_Y);

  fontSM = gdispOpenFont (FONT_SM);
  fontFF = gdispOpenFont (FONT_FIXED);
  fontXS = gdispOpenFont (FONT_XS);

  uint16_t xpos = 0; // cursor, so if we move or add things we don't have to rethink this  
  uint16_t ypos = 0; // cursor, so if we move or add things we don't have to rethink this

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
  
  drawProgressBar(xpos,ypos,100,10,enemies[current_enemy_idx]->hp,maxhp(enemies[current_enemy_idx]->level), 0, false);
}

static void screen_demand_draw(void) {
  GWidgetInit wi;
  font_t fontFF;
  FightHandles *p;
  char tmp[40];
  int xpos, ypos;

  // sadly, our handlers cannot pass us the context. Get it from the app
  p = instance.context->priv;

  fontFF = gdispOpenFont(FONT_FIXED);
  gdispClear(Black);
  gwinSetDefaultFont(gdispOpenFont(FONT_FIXED));

  putImageFile(IMG_GUARD_IDLE_R, POS_PLAYER2_X, POS_PLAYER2_Y - 20);

  gdispDrawStringBox (0,
		      18,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontFF, fontHeight),
		      "A CHALLENGER AWAITS!",
		      fontFF, Red, justifyCenter);

  ypos = (gdispGetHeight() / 2) - 60;
  xpos = 10;

  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      current_enemy.name,
		      fontFF, Yellow, justifyLeft);
  ypos=ypos+50;
  chsnprintf(tmp, sizeof(tmp), "LEVEL %d", current_enemy.level);

  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      tmp,
		      fontFF, Yellow, justifyLeft);


  ypos = ypos + gdispGetFontMetric(fontFF, fontHeight);
  chsnprintf(tmp, sizeof(tmp), "HP %d", current_enemy.hp);
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontFF, fontHeight),
		      tmp,
		      fontFF, Yellow, justifyLeft);

  ypos = ypos + gdispGetFontMetric(fontFF, fontHeight) + 5;

  drawProgressBar(xpos,ypos,100,10,current_enemy.hp,maxhp(current_enemy.level), 0, false);
  
  // draw UI
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 30;
  wi.g.y = POS_FLOOR_Y-10;
  wi.g.width = 100;
  wi.g.height = gdispGetFontMetric(fontFF, fontHeight) + 2;
  wi.text = "NOPE";
  p->ghDeny = gwinButtonCreate(0, &wi);
  
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);

  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 190;
  wi.g.y = POS_FLOOR_Y-10;
  wi.g.width = 100;
  wi.g.height = gdispGetFontMetric(fontFF, fontHeight) + 2;
  wi.text = "FIGHT!";
  p->ghAccept = gwinButtonCreate(0, &wi);

  gwinSetDefaultStyle(&BlackWidgetStyle, FALSE);

  drawProgressBar(40,gdispGetHeight() - 20,240,20,DEFAULT_WAIT_TIME,countdown, true, false);

}
static void updatehp(void)  {
  // update the hit point counters at the top of the screen
  userconfig *config = getConfig();
  font_t font;
  font = gdispOpenFont (FONT_FIXED);

  gdispDrawStringBox (1, 16, gdispGetWidth()-1,
		      gdispGetFontMetric(font, fontHeight),
		      "KO",
		      font, Yellow, justifyCenter);

  gdispDrawStringBox (0, 15, gdispGetWidth(),
		      gdispGetFontMetric(font, fontHeight),
		      "KO",
		      font, Red, justifyCenter);

  drawProgressBar(0,22,135,12,maxhp(config->level),config->hp,false,true);
  drawProgressBar(185,22,135,12,maxhp(current_enemy.level),current_enemy.hp,false,false);
}

static void screen_vs_draw(void)  {
  // versus screen!
  font_t fontSM;
  font_t fontLG;
  userconfig *config = getConfig();
  uint16_t ypos = 0; // cursor, so if we move or add things we don't have to rethink this
  
  putImageFile(IMG_GUARD_IDLE_L, POS_PLAYER1_X, POS_PLAYER2_Y);
  putImageFile(IMG_GUARD_IDLE_R, POS_PLAYER2_X, POS_PLAYER2_Y);
  
  fontSM = gdispOpenFont(FONT_SM);
  fontLG = gdispOpenFont(FONT_LG);

  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      config->name,
		      fontSM, Lime, justifyLeft);

  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      current_enemy.name,
		      fontSM, Red, justifyRight);

  ypos = ypos + gdispGetFontMetric(fontSM, fontHeight);

  if (current_fight_state == VS_SCREEN) {
    gdispFillArea(0,
		  ypos,
		  gdispGetWidth(),
		  gdispGetFontMetric(fontSM, fontHeight),
		  Black);

    blinkText(0,
	      110,
	      gdispGetWidth(),
	      gdispGetFontMetric(fontLG, fontHeight),
	      "VS",
	      fontLG,
	      White,
	      justifyCenter,
	      10,
	      250);

    updatehp();
    
    ypos = ypos + 15;

    if (isattacking) { 
      clearstatus();
      blinkText(0,
		ypos,
		gdispGetWidth(),
		gdispGetFontMetric(fontSM, fontHeight),
		"Attack: TAP ON ENEMY TO SELECT",
		fontSM, White, justifyCenter,8,250);
      clearstatus();
      gdispDrawStringBox (0,
                          ypos,
                          gdispGetWidth(),
                          gdispGetFontMetric(fontSM, fontHeight),
                          "Attack! High, Mid, Low?",
                          fontSM, White, justifyCenter);
    } else {
      clearstatus();
      blinkText(0,
		ypos,
		gdispGetWidth(),
		gdispGetFontMetric(fontSM, fontHeight),
		"Defend: TAP ON SELF TO SELECT",
		fontSM, White, justifyCenter,8,250);
      clearstatus();
      gdispDrawStringBox (0,
                          ypos,
                          gdispGetWidth(),
                          gdispGetFontMetric(fontSM, fontHeight),
                          "Defend! High, Mid, Low?",
                          fontSM, White, justifyCenter);
    }
     
    

    draw_attack_buttons();

    last_tick_time = chVTGetSystemTime();
    current_fight_state = MOVE_SELECT;
    countdown=MOVE_WAIT_TIME;
  } else { 
    gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      "Waiting for enemy to accept!",
		      fontSM, White, justifyCenter);
  }
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

  p->ghExitButton = NULL;
  p->ghNextEnemy = NULL;
  p->ghPrevEnemy = NULL;
  p->ghAttack = NULL;
}

static void fight_start(OrchardAppContext *context) {
  FightHandles *p = context->priv;
  user **enemies = enemiesGet();

  p = chHeapAlloc (NULL, sizeof(FightHandles));
  memset(p, 0, sizeof(FightHandles));
  context->priv = p;

  orchardAppTimer(context, FRAME_INTERVAL_US, true);
  last_ui_time = chVTGetSystemTime();
 
  // are we entering a fight?
  if (current_fight_state == APPROVAL_DEMAND) { 
    last_tick_time = chVTGetSystemTime();
    countdown=DEFAULT_WAIT_TIME;
    playAttacked();
    screen_demand_draw();
  } 

  if (current_fight_state == IDLE) {
    current_fight_state = ENEMY_SELECT;
    if (enemyCount() > 0) {
      screen_select_draw(TRUE);
      draw_select_buttons(p);
      if (enemies[current_enemy_idx] == NULL) {
	nextEnemy();
      }
    } else {
      // punt if no enemies
      screen_alert_draw("NO ENEMIES NEARBY!");
      chThdSleepMilliseconds(ALERT_DELAY);
      orchardAppExit();
      return;
    }
  }

  // if we got this far, we can enable listeners. 
  geventListenerInit(&p->glFight);
  gwinAttachListener(&p->glFight);
  geventRegisterCallback (&p->glFight, orchardAppUgfxCallback, &p->glFight);
}

static void screen_waitapproval_draw(void) {
  gdispClear(Black);
  screen_vs_draw(); // draw the start of the vs screen

  // progress bar
  last_tick_time = chVTGetSystemTime();
  countdown = DEFAULT_WAIT_TIME; // used to hold a generic timer value.
  drawProgressBar(40,gdispGetHeight() - 20,240,20,DEFAULT_WAIT_TIME,countdown, true, false);
}

static void sendGamePacket(uint8_t opcode) {
  // sends a game packet to current_enemy
  userconfig *config;
  user upkt;
 
  config = getConfig();
  memset (&upkt, 0, sizeof(upkt));

  upkt.ttl = 4;
  upkt.opcode = opcode;
  strncpy(upkt.name, config->name, CONFIG_NAME_MAXLEN);

  upkt.in_combat = config->in_combat;
  upkt.hp = config->hp;
  upkt.level = config->level;
  upkt.p_type = config->p_type;

  upkt.attack_bitmap = attackbitmap;
  
  radioSend (&KRADIO1, current_enemy.netid, RADIO_PROTOCOL_FIGHT, sizeof(upkt), &upkt);
}

static void end_fight(void) {
  // set my incombat to true
  userconfig *config = getConfig();
  config->in_combat = 0;
  current_fight_state = IDLE;
}

static void fight_event(OrchardAppContext *context,
			const OrchardAppEvent *event) {

  FightHandles * p = context->priv;
  GEvent * pe;
  user **enemies = enemiesGet();
  userconfig *config = getConfig();
  
  // the timerEvent fires on the frame interval and is overloaded a bit to handle
  // real-time functionality in the fight sequence
  //
  // this returns us to the badge screen on idle.
  // we don't want this for all states, just select and end-of-fight
  if (event->type == timerEvent) {
    // timer events always decrement countdown unless countdown <= 0
    //    chprintf(stream, "[gamestate %d] tick last: %d now: %d remain: %d\r\n", current_fight_state, last_tick_time, chVTGetSystemTime(), countdown);

    if (countdown > 0) {
      // our time reference is based on elapsed time. 
      countdown = countdown - ( chVTGetSystemTime() - last_tick_time );
      last_tick_time = chVTGetSystemTime();
    }
  
    /* handle idle timeout in the ENEMY_SELECT state */
    if (current_fight_state == ENEMY_SELECT) {
      if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
    	  orchardAppRun(orchardAppByName("Badge"));
      } 
      return;
    }
    if (current_fight_state == MOVE_SELECT) {
      drawProgressBar(40,gdispGetHeight() - 20,240,20,MOVE_WAIT_TIME,countdown, true, false);

      if (countdown <= 0) {
	// transmit my move
        sendGamePacket(OP_YOUGO);
        // so things sort of break here.
        // if we both transmit at the same time, we're boned.
        // the attacker should send his move now, wait for an ack, followed by the
        // challenger
        current_fight_state = MOVE_WAITACK;
      }
    }
    if ((current_fight_state == APPROVAL_WAIT) || (current_fight_state == APPROVAL_DEMAND)) {
      drawProgressBar(40,gdispGetHeight() - 20,240,20,DEFAULT_WAIT_TIME,countdown, true,false);

      if (countdown <= 0) {
       orchardAppTimer(context, 0, false); // shut down the timer
       screen_alert_draw("TIMED OUT!");
       ledSetProgress(-1);
       end_fight();
       playHardFail();
       chThdSleepMilliseconds(ALERT_DELAY);
       orchardAppRun(orchardAppByName("Badge"));
      }

      return;
    }

    if (current_fight_state == MOVE_WAITACK) {
      // prevent us from becoming stuck waiting on an ACK. 
      if ( (chVTGetSystemTime() - last_tick_time) > MAX_ACKWAIT ) {
	orchardAppTimer(context, 0, false); // shut down the timer
	screen_alert_draw("OTHER PLAYER WENT AWAY");
	ledSetProgress(-1);
	end_fight();
	playHardFail();
	chThdSleepMilliseconds(ALERT_DELAY);
	orchardAppRun(orchardAppByName("Badge"));
	return;
      }
	
    }
  }

  if (event->type == ugfxEvent) {
      pe = event->ugfx.pEvent;
      // we handle all of the buttons on all of the possible screens
      // in this event handler, along with our countdowns
      switch(pe->type) {
      case GEVENT_GWIN_BUTTON:
	if ( ((GEventGWinButton*)pe)->gwin == p->ghExitButton) { 
	  orchardAppExit();
	  return;
	}
        if ( ((GEventGWinButton*)pe)->gwin == p->ghNextEnemy) { 
	  if (nextEnemy()) {
	       last_ui_time = chVTGetSystemTime();
	       screen_select_draw(FALSE);
	  }
	  return;
	}
  if ( ((GEventGWinButton*)pe)->gwin == p->ghAttack) { 
	  // we are attacking. 
	  last_ui_time = chVTGetSystemTime();
	  screen_select_close(context);
	  memcpy(&current_enemy, enemies[current_enemy_idx], sizeof(user));
	  config->in_combat = true;
	  sendGamePacket(OP_STARTBATTLE);
	  current_fight_state = APPROVAL_WAIT;
	  screen_waitapproval_draw();
	  isattacking=true;
	  return;
	}
	if ( ((GEventGWinButton*)pe)->gwin == p->ghDeny) { 
	  // "war is sweet to those who have not experienced it."
	  // Pindar, 522-443 BC.
	  // do our cleanups now so that the screen doesn't redraw while exiting
	  orchardAppTimer(context, 0, false); // shut down the timer
	  gwinDestroy (p->ghDeny);
	  gwinDestroy (p->ghAccept);
	  p->ghDeny = NULL;
	  p->ghAccept = NULL;
	  
	  sendGamePacket(OP_DECLINED);
	  screen_alert_draw("Dulce bellum inexpertis.");
	  playHardFail();
	  ledSetProgress(-1);
	  end_fight();
	  chThdSleepMilliseconds(3000);
	  orchardAppRun(orchardAppByName("Badge"));
	  return;
	}
	if ( ((GEventGWinButton*)pe)->gwin == p->ghAccept) { 
	  gwinDestroy (p->ghDeny);
	  gwinDestroy (p->ghAccept);
	  p->ghDeny = NULL;
	  p->ghAccept = NULL;
	  current_fight_state = VS_SCREEN;
	  sendGamePacket(OP_STARTBATTLE_ACK);
	  gdispClear(Black);
	  screen_vs_draw();
	  isattacking=false;
	  return;
	}
	if ( ((GEventGWinButton*)pe)->gwin == p->ghPrevEnemy) { 
	  if (prevEnemy()) {
	    last_ui_time = chVTGetSystemTime();
	    screen_select_draw(FALSE);
	  }
	  return;
	}

	if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackLow) {
	  attackbitmap &= ~ATTACK_MASK;
	  attackbitmap |= ATTACK_LOW;
	  draw_attack_icons();
	}
	if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackMid) {
	  attackbitmap &= ~ATTACK_MASK;
	  attackbitmap |= ATTACK_MID;
	  draw_attack_icons();
	}
	if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackHi) {
	  attackbitmap &= ~ATTACK_MASK;
	  attackbitmap |= ATTACK_HI;
	  draw_attack_icons();
	}
	if ( ((GEventGWinButton*)pe)->gwin == p->ghBlockLow) {
	  attackbitmap &= ~BLOCK_MASK;
	  attackbitmap |= BLOCK_LOW;
	  draw_attack_icons();
	}
	if ( ((GEventGWinButton*)pe)->gwin == p->ghBlockMid) {
	  attackbitmap &= ~BLOCK_MASK;
	  attackbitmap |= BLOCK_MID;
	  draw_attack_icons();
	}
	if ( ((GEventGWinButton*)pe)->gwin == p->ghBlockHi) {
	  attackbitmap &= ~BLOCK_MASK;
	  attackbitmap |= BLOCK_HI;
	  draw_attack_icons();
	}

      }
  }
}

static void fight_exit(OrchardAppContext *context) {
  FightHandles * p;
  userconfig *config = getConfig();
  
  p = context->priv;
  if (current_fight_state == ENEMY_SELECT) { 
    gwinDestroy (p->ghExitButton);
    gwinDestroy (p->ghNextEnemy);
    gwinDestroy (p->ghPrevEnemy);
    gwinDestroy (p->ghAttack);
  }

  if (p->ghDeny != NULL)
      gwinDestroy (p->ghDeny);

  if (p->ghAccept != NULL)
      gwinDestroy (p->ghAccept);

  geventDetachSource (&p->glFight, NULL);
  geventRegisterCallback (&p->glFight, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;

  config->in_combat = 0;
  current_fight_state = IDLE;
  return;
}

static void radio_updatestate(KW01_PKT * pkt)
{
  /* this is the state machine that handles transitions for the game via the radio */
  user *u;
  userconfig *config;
 
  config = getConfig();
  
  u = (user *)pkt->kw01_payload;
  last_ui_time = chVTGetSystemTime(); // radio is a state-change, so reset this.

  chprintf(stream, "gamepacket with mystate %d, incoming opcode %d\n", current_fight_state, u->opcode);
  
  switch (current_fight_state) {
  case IDLE:
  case ENEMY_SELECT:
    if (u->opcode == OP_STARTBATTLE && config->in_combat == 0) {
      /* we can accept a new fight */
      memcpy(&current_enemy, u, sizeof(user));
      // put up screen, accept fight from user foobar/badge foobar
      if (current_fight_state == IDLE) { 
        // we are not actually in our app, so run it to pick up context. 
        orchardAppRun(orchardAppByName("Fight"));
      } else {
	// build the screen
        countdown=DEFAULT_WAIT_TIME;
	playAttacked();
	screen_demand_draw();
      }
      current_fight_state = APPROVAL_DEMAND;
    }
    break;
  case APPROVAL_WAIT:
    if (u->opcode == OP_DECLINED) {
      screen_alert_draw("DENIED.");
      playHardFail();
      orchardAppTimer(instance.context, 0, false); // shut down the timer
      ledSetProgress(-1);
      chThdSleepMilliseconds(ALERT_DELAY);

      screen_select_draw(TRUE);
      draw_select_buttons(instance.context->priv);

      // start the clock again.
      last_ui_time = chVTGetSystemTime();
      current_fight_state = ENEMY_SELECT;
      orchardAppTimer(instance.context, FRAME_INTERVAL_US, true);

    }
    if (u->opcode == OP_STARTBATTLE_ACK) {
      current_fight_state = VS_SCREEN;
      sendGamePacket(OP_STARTBATTLE_ACK);
      gdispClear(Black);
      screen_vs_draw();
    }
    break;
  case MOVE_SELECT:
    if (u->opcode == OP_YOUGO) {
	// If I get a YOUGO in move select, that means the other end
	// timed out, and it's time to move on.
        memcpy(&current_enemy, u, sizeof(user));
	// ack their yougo packet. 
	sendGamePacket(OP_MOVEACK);
	// immediately transition to results
	current_fight_state = RESULTS;
	show_results();
      }
    break;
  case MOVE_WAITACK:
    if (u->opcode == OP_MOVEACK || u->opcode == OP_YOUGO) {
      // just incase, we ACK again,
      memcpy(&current_enemy, u, sizeof(user));
      sendGamePacket(OP_MOVEACK);
      current_fight_state = RESULTS;
      show_results();
    }
    break;
  case APPROVAL_DEMAND:
  case VS_SCREEN:
  case RESULTS:
  case PLAYER_DEAD:
  case ENEMY_DEAD:
    break;
  }

}

static uint32_t fight_init(OrchardAppContext *context) {
  FightHandles *p;

  if (context == NULL) {
    /* This should only happen for auto-init */
    radioHandlerSet (&KRADIO1, RADIO_PROTOCOL_FIGHT,
		     radio_updatestate);
  } else {
    p = chHeapAlloc (NULL, sizeof(FightHandles));
    memset(p, 0, sizeof(FightHandles));
    context->priv = p;
  }

  return 0;
}

orchard_app("Fight", APP_FLAG_AUTOINIT, fight_init, fight_start, fight_event, fight_exit);


