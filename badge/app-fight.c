#include <stdio.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "rand.h"

#include "orchard-app.h"
#include "led.h"
#include "radio.h"
#include "orchard-ui.h"
#include "images.h"
#include "fontlist.h"
#include "sound.h"

#include "userconfig.h"
#include "app-fight.h"

/* Globals */
static int32_t countdown = DEFAULT_WAIT_TIME; // used to hold a generic timer value. 
static user current_enemy;                    // current enemy we are attacking/talking to 
static user packet;                           // the last packet we sent, for retransmission

static fight_state current_fight_state = IDLE;  // current state 
static fight_state next_fight_state = NONE;     // upon ACK, we will transition to this state. 

static uint8_t current_enemy_idx = 0;
static uint32_t last_ui_time = 0;
static uint32_t last_tick_time = 0;
static uint8_t ourattack = 0;
static uint8_t theirattack = 0;
static uint32_t lastseq = 0;
static uint16_t last_hit = 0;

static void clearstatus(void) {
  // clear instruction area
  gdispFillArea(0,
		38,
		gdispGetWidth(),
		23,
		Black);
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

uint16_t calc_hit(void) {
  // return random shit for now
  return ((((uint8_t)rand()) % 200) +1);
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


static void sendAttack(void) {
  // animate the attack on our screen and send it to the the
  // challenger
  
  clearstatus();
  font_t fontSM;
  fontSM = gdispOpenFont (FONT_SM);
  
  // clear middle
  gdispFillArea(140,70,40,110,Black);

  if (ourattack & ATTACK_HI) {
    putImageFile(IMG_GATTH1, POS_PLAYER1_X, POS_PLAYER1_Y);
  }

  if (ourattack & ATTACK_MID) {
    putImageFile(IMG_GATTM1, POS_PLAYER1_X, POS_PLAYER1_Y);
  }
  
  if (ourattack & ATTACK_LOW) {
    putImageFile(IMG_GATTL1, POS_PLAYER1_X, POS_PCENTER_Y);
  }

  // we always say we're waiting. 
  gdispDrawStringBox (0,
                      38,
                      gdispGetWidth(),
                      gdispGetFontMetric(fontSM, fontHeight),
                      "WAITING FOR CHALLENGER'S MOVE",
                      fontSM, White, justifyCenter);

  next_fight_state = POST_MOVE;
  sendGamePacket(OP_IMOVED);
}

static void draw_attack_buttons(void) {
  GWidgetInit wi;
  FightHandles *p;
  
  gwinWidgetClearInit(&wi);
  p = instance.context->priv;
  
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


static void draw_move_select(void) {
  uint16_t ypos = 38;
  font_t fontSM;
  fontSM = gdispOpenFont (FONT_SM);

  clearstatus();
  putImageFile(IMG_GUARD_IDLE_L, POS_PLAYER1_X, POS_PLAYER2_Y);
  putImageFile(IMG_GUARD_IDLE_R, POS_PLAYER2_X, POS_PLAYER2_Y);
  gdispDrawStringBox (0,
                      ypos,
                      gdispGetWidth(),
                      gdispGetFontMetric(fontSM, fontHeight),
                      "Attack! High, Mid, Low?",
                      fontSM, White, justifyCenter);
  
  draw_attack_buttons();
  
  last_tick_time = chVTGetSystemTime();
  current_fight_state = MOVE_SELECT;
  countdown=MOVE_WAIT_TIME;

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
  char c=' ';
  char attackfn1[13];
  char attackfn2[13];
  char ourdmg_s[10];
  char theirdmg_s[10];
  uint16_t textx, texty;
  font_t fontFF;
  FightHandles *p;
  
  fontFF = gdispOpenFont(FONT_FIXED);

  // when we enter this state we must remove the buttons from MOVE_SELECT
  p = instance.context->priv;
  gwinDestroy (p->ghAttackHi);
  gwinDestroy (p->ghAttackMid);
  gwinDestroy (p->ghAttackLow);
  p->ghAttackHi = NULL;
  p->ghAttackMid = NULL;
  p->ghAttackLow = NULL;
  
  clearstatus();
  gdispFillArea(0,gdispGetHeight() - 20,gdispGetWidth(),20,Black);

  // get the right filename for the attack
  if (ourattack & ATTACK_HI) c='h';
  if (ourattack & ATTACK_MID) c='m';
  if (ourattack & ATTACK_LOW) c='l';

  if (c == ' ') 
    strcpy(attackfn1, IMG_GIDLA1);
  else 
    chsnprintf(attackfn1, sizeof(attackfn1), "gatt%c1.rgb", c);

  c = ' ';
  textx = 160;
  if (theirattack & ATTACK_HI) {
    c='h';
    texty = 40;
  }
  
  if (theirattack & ATTACK_MID) {
    c='m';
    texty = 120;
  }
  
  if (theirattack & ATTACK_LOW) {
    c='l';
    texty = 180;
  }
  
  if (c == ' ') 
    strcpy(attackfn2, IMG_GIDLA1R);
  else 
    chsnprintf(attackfn2, sizeof(attackfn2), "gatt%c1r.rgb", c);

  // animate the characters
  chsnprintf (ourdmg_s, sizeof(ourdmg_s), "-%d", 999 );
  chsnprintf (theirdmg_s, sizeof(theirdmg_s), "-%d", 999 );
  
  for (uint8_t i=0; i < 4; i++) { 
    putImageFile(attackfn1, POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(attackfn2, POS_PLAYER2_X, POS_PLAYER2_Y);

    // you attacking us
    gdispDrawStringBox (textx,texty,50,50,ourdmg_s,fontFF,Red,justifyLeft);
    // us attacking you
    gdispDrawStringBox (textx+40,texty,50,50,theirdmg_s,fontFF,Red,justifyLeft);
    
    chThdSleepMilliseconds(200);
    gdispDrawStringBox (textx,texty,50,50,ourdmg_s,fontFF,Black,justifyLeft);
    gdispDrawStringBox (textx+40,texty,50,50,theirdmg_s,fontFF,Black,justifyLeft);

    putImageFile(IMG_GUARD_IDLE_L, POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(IMG_GUARD_IDLE_R, POS_PLAYER2_X, POS_PLAYER2_Y);
    chThdSleepMilliseconds(200);
  }

  // update the health bars
  updatehp();
  
  // if not dead transition state back to move_select
  next_fight_state = NEXTROUND;
  sendGamePacket(OP_NEXTROUND);
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

  // TODO - support target hp and animation
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
  char attackfn1[13];
  char attackfn2[13];
  
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
    // disable the timer, we are going to do some animations
    orchardAppTimer(instance.context, 0, false); // shut down the timer
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
	      200);

    updatehp();
    
    ypos = ypos + 15;

    clearstatus();

    // animate them 
    char pos[4] = "hml";
    
    for (uint8_t i=0; i < 3; i++) {
      chsnprintf(attackfn1, sizeof(attackfn1), "gatt%c1.rgb", pos[i]);
      chsnprintf(attackfn2, sizeof(attackfn2), "gatt%c1r.rgb", pos[i]);

      putImageFile(attackfn1, POS_PLAYER1_X, POS_PLAYER1_Y);
      putImageFile(attackfn2, POS_PLAYER2_X, POS_PLAYER2_Y);

      // we always say we're waiting. 
      gdispDrawStringBox (0,
                          gdispGetHeight() - 20,
                          gdispGetWidth(),
                          gdispGetFontMetric(fontSM, fontHeight),
                          "GET READY!",
                          fontSM, White, justifyCenter);

      chThdSleepMilliseconds(200);

    }

    // re-enable the timer, our animations are over.
    orchardAppTimer(instance.context, FRAME_INTERVAL_US, true);

    current_fight_state = MOVE_SELECT_INIT;
    
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

static void sendACK(user *inbound) {
  // acknowlege an inbound packet.
  userconfig *config;
  config = getConfig();
  
  memset (&packet, 0, sizeof(packet)); 
  packet.seq = lastseq++;
  packet.netid = config->netid; // netid is id of sender, always.
  packet.acknum = inbound->seq;
  packet.ttl = 4;
  packet.opcode = OP_ACK;
  chprintf(stream, "\r\ntransmit ACK (to=%08x for seq %d): ttl=%d, myseq=%d, opcode=0x%x\r\n",
           inbound->netid,
           inbound->seq,
           packet.ttl,
           packet.seq,
           packet.opcode);
  
  radioSend (&KRADIO1, inbound->netid, RADIO_PROTOCOL_FIGHT, sizeof(packet), &packet);
}

static void sendRST(user *inbound) {
  // do the minimal amount of work to send a reset packet based on an
  // inbound packet.
  userconfig *config;
  config = getConfig();
  
  memset (&packet, 0, sizeof(packet)); 
  packet.seq = ++lastseq;
  packet.netid = config->netid; // netid is id of sender, always.
  packet.acknum = inbound->seq; 
  packet.ttl = 4;
  packet.opcode = OP_RST;
  chprintf(stream, "\r\nTransmit RST (to=%08x for seq %d): ttl=%d, myseq=%d, opcode=0x%x\r\n",
           inbound->netid,
           inbound->seq,
           packet.ttl,
           packet.seq,
           packet.opcode);
  
  radioSend (&KRADIO1, inbound->netid, RADIO_PROTOCOL_FIGHT, sizeof(packet), &packet);
}

static void resendPacket(void) {
  // TODO: random holdoff?
  packet.ttl--;
  current_fight_state = WAITACK;
  chprintf(stream, "\r\n%ld Transmit (to=%08x): ttl=%d, seq=%d, opcode=0x%x\r\n", chVTGetSystemTime()  , current_enemy.netid, packet.ttl, packet.seq, packet.opcode);
  radioSend (&KRADIO1, current_enemy.netid, RADIO_PROTOCOL_FIGHT, sizeof(packet), &packet);
}

static void sendGamePacket(uint8_t opcode) {
  // sends a game packet to current_enemy
  userconfig *config;
 
  config = getConfig();
  memset (&packet, 0, sizeof(packet));

  packet.netid = config->netid;
  packet.seq = lastseq++;
  packet.ttl = MAX_RETRIES+1;
  packet.opcode = opcode;
  strncpy(packet.name, config->name, CONFIG_NAME_MAXLEN);

  packet.in_combat = config->in_combat;
  packet.hp = config->hp;
  packet.level = config->level;
  packet.p_type = config->p_type;

  packet.won = config->won;
  packet.lost = config->lost;
  packet.gold = config->gold;
  packet.xp = config->xp;
  packet.spr = config->spr;
  packet.str = config->str;
  packet.def = config->def;
  packet.dex = config->dex;

  packet.attack_bitmap = ourattack;
  
  resendPacket();
}

static void end_fight(void) {
  // set my incombat to false, clear out the fight.
  userconfig *config = getConfig();

  memset(&current_enemy, 0, sizeof(current_enemy));
  config->in_combat = 0;
  ourattack = 0;
  theirattack = 0;
  current_fight_state = IDLE;
  ledSetProgress(-1);
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
    chprintf(stream, "tick @ %d mystate is %d, countdown is %d, last tick %d, ourattack %d, theirattack %d, delta %d\r\n", chVTGetSystemTime(),
             current_fight_state,
             countdown,
             last_tick_time,
             ourattack,
             theirattack,
             (chVTGetSystemTime() - last_tick_time));
    // timer events always decrement countdown unless countdown <= 0

    // handle ACKs first before anything else 
    if (current_fight_state == WAITACK) {
      // transmit/retry logic
      if ( (chVTGetSystemTime() - last_tick_time) > MAX_ACKWAIT ) {
        if (packet.ttl > 0)  {
          resendPacket();
        } else { 
          orchardAppTimer(context, 0, false); // shut down the timer
          screen_alert_draw("OTHER PLAYER WENT AWAY");
          end_fight();
          playHardFail();
          chThdSleepMilliseconds(ALERT_DELAY);
          orchardAppRun(orchardAppByName("Badge"));
          return;
        }
      }
    }

    // now update the clock.
    if (countdown > 0) {
      // our time reference is based on elapsed time. 
      countdown = countdown - ( chVTGetSystemTime() - last_tick_time );
      last_tick_time = chVTGetSystemTime();
    }

    if (current_fight_state == NEXTROUND) {
      /* we are starting a new round. */
      countdown=MOVE_WAIT_TIME;
      last_tick_time = chVTGetSystemTime();

      chprintf(stream, "\r\nStarting new round!\r\n", last_hit);
      // if either side dies update states and exit app
      ourattack = 0;
      theirattack = 0;
      current_fight_state=MOVE_SELECT;
      return;
    }
    
    /* handle idle timeout in the ENEMY_SELECT state */
    if (current_fight_state == ENEMY_SELECT) {
      if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
    	  orchardAppRun(orchardAppByName("Badge"));
      } 
      return;
    }

    if (current_fight_state == VS_SCREEN) {
      gdispClear(Black);
      screen_vs_draw();
    }

    if (current_fight_state == MOVE_SELECT_INIT) {
      draw_move_select();
      current_fight_state = MOVE_SELECT;
    }
    
    if (current_fight_state == MOVE_SELECT) {
      drawProgressBar(40,gdispGetHeight() - 20,240,20,MOVE_WAIT_TIME,countdown, true, false);

      if (countdown <= 0) {
	// transmit my move
        chprintf(stream, "\r\nTIMEOUT - transmitting hit %d\r\n", last_hit);

        // we're in MOVE_SELECT, so this tells us that we haven't moved at all. sad.
        // we'll now tell our opponent, that our turn is over. 
        next_fight_state = POST_MOVE;
        sendGamePacket(OP_TURNOVER);
      }
    }
    
    if ((current_fight_state == APPROVAL_WAIT) || (current_fight_state == APPROVAL_DEMAND)) {
      drawProgressBar(40,gdispGetHeight() - 20,240,20,DEFAULT_WAIT_TIME,countdown, true,false);

      if (countdown <= 0) {
       orchardAppTimer(context, 0, false); // shut down the timer
       screen_alert_draw("TIMED OUT!");
       end_fight();
       playHardFail();
       chThdSleepMilliseconds(ALERT_DELAY);
       orchardAppRun(orchardAppByName("Badge"));
      }

      return;
    }
    if (current_fight_state == POST_MOVE) { 
      drawProgressBar(40,gdispGetHeight() - 20,240,20,DEFAULT_WAIT_TIME,countdown, true,false);
      
      // if we have a move and they have a move, then we go straight to show results
      // and we send them a op_turnover packet to push them along
      
      if ( ( ((theirattack & ATTACK_MASK) > 0) &&
             ((ourattack & ATTACK_MASK) > 0)) ||
           (countdown <=0 )) {
        next_fight_state = SHOW_RESULTS;
        sendGamePacket(OP_TURNOVER);
      }
    }
    
    if (current_fight_state == SHOW_RESULTS) {
      show_results();
      return;
    }
    
  }

  if (event->type == ugfxEvent) {
    chprintf(stream, "ugfxevent \r\n");
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

	  next_fight_state = APPROVAL_WAIT;
	  sendGamePacket(OP_STARTBATTLE);

	  screen_waitapproval_draw();
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

          next_fight_state = IDLE;
	  sendGamePacket(OP_DECLINED);
	  screen_alert_draw("Dulce bellum inexpertis.");
	  playHardFail();
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

	  next_fight_state = VS_SCREEN;
	  sendGamePacket(OP_STARTBATTLE_ACK);
	  return;
	}
	if ( ((GEventGWinButton*)pe)->gwin == p->ghPrevEnemy) { 
	  if (prevEnemy()) {
	    last_ui_time = chVTGetSystemTime();
	    screen_select_draw(FALSE);
	  }
	  return;
	}

        if ((ourattack & ATTACK_MASK) == 0) {
          // TODO -- modify for three attacks per Egan's spec. You
          // can't change.
          if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackLow) {
            ourattack &= ~ATTACK_MASK;
            ourattack |= ATTACK_LOW;
            sendAttack();
          }
          if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackMid) {
            ourattack &= ~ATTACK_MASK;
            ourattack |= ATTACK_MID;
            sendAttack();
          }
          if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackHi) {
            ourattack &= ~ATTACK_MASK;
            ourattack |= ATTACK_HI;
            sendAttack();
          }
        } else {
          // can't move.
          chprintf(stream, "rejecting event -- already went\r\n");
          playNope();
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
    p->ghExitButton = NULL;
    p->ghNextEnemy = NULL;
    p->ghPrevEnemy = NULL;
    p->ghAttack = NULL;
  }

  if (p->ghDeny != NULL) { 
      gwinDestroy (p->ghDeny);
      p->ghDeny = NULL;
  }

  if (p->ghAccept != NULL) { 
      gwinDestroy (p->ghAccept);
      p->ghAccept = NULL;
  }

  geventDetachSource (&p->glFight, NULL);
  geventRegisterCallback (&p->glFight, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;

  config->in_combat = 0;
  current_fight_state = IDLE;
  return;
}

static void fightRadioEventHandler(KW01_PKT * pkt)
{
  /* this is the state machine that handles transitions for the game via the radio */
  user *u;
  userconfig *config;
 
  config = getConfig();
  
  u = (user *)pkt->kw01_payload;
  last_ui_time = chVTGetSystemTime(); // radio is a state-change, so reset this.

  // DEBUG
  if (u->opcode == OP_ACK) {
    chprintf(stream, "\r\n%08x --> RECV ACK (seq=%d, acknum=%d, state=%d, opcode 0x%x)\r\n", u->netid, u->seq, u->acknum, current_fight_state, next_fight_state, u->opcode);
  } else { 
    chprintf(stream, "\r\n%08x --> (seq=%d, state=%d, opcode 0x%x)\r\n", u->netid, u->seq, current_fight_state, u->opcode);
  }
  // Immediately ACK non-ACK packets. We do not support retry on
  // ACK, because we support ARQ just like TCP-IP.  without the ACK,
  // the sender will retransmit automatically.

  if ( ((current_fight_state == IDLE) && (u->opcode >= OP_IMOVED)) && (u->opcode != OP_ACK)) {
    // this is an invalid state, which we should not ACK for.
    // Let the remote client die.
    chprintf(stream, "\r\nRST: rejecting opcode 0x%x beacuse i am idle\r\n", u->opcode);
    sendRST(u);
    // we are idle, do nothing. 
    return;
  }
  
  if (u->opcode != OP_ACK)
      sendACK(u);
    
  switch (current_fight_state) {
  case WAITACK:
    if (u->opcode == OP_RST) {
      screen_alert_draw("CONNECTION LOST");
      end_fight();
      playHardFail();
      chThdSleepMilliseconds(ALERT_DELAY);
      orchardAppRun(orchardAppByName("Badge"));
      return;
    }
    if (u->opcode == OP_ACK) { 
      // if we are waiting for an ack, advance our state.
      // however, if we have no next-state to go to, then we will timeout
      if (next_fight_state != NONE) {
        chprintf(stream, "\r\n%08x --> moving to state %d from state %d due to ACK\n",
                 u->netid, next_fight_state, current_fight_state, next_fight_state);

        current_fight_state = next_fight_state;
      }
    }
    break;
  case IDLE:
  case ENEMY_SELECT:
    if (u->opcode == OP_STARTBATTLE && config->in_combat == 0) {
      /* we can accept a new fight */
      memcpy(&current_enemy, u, sizeof(user));
      ourattack = 0;
      theirattack = 0;
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
      orchardAppTimer(instance.context, 0, false); // shut down the timer
      screen_alert_draw("DENIED.");
      playHardFail();
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
      // i am the attacker and you just ACK'd me. The timer will render this.
      current_fight_state = VS_SCREEN;
    }
    break;
    
  case MOVE_SELECT:
    if (u->opcode == OP_IMOVED) {
      // If I see OP_IMOVED from the other side while we are in
      // MOVE_SELECT, store that move for later. 
      memcpy(&current_enemy, u, sizeof(user));

      theirattack = current_enemy.attack_bitmap;
      chprintf(stream, "\r\nRECV MOVE: (select) %d. our move %d.\r\n", theirattack, ourattack);
    }
    if (u->opcode == OP_TURNOVER) {
      // uh-oh, the turn is over and we haven't moved!
      next_fight_state = POST_MOVE;
      sendGamePacket(OP_TURNOVER);
    }
    break;
  case POST_MOVE: // no-op
    // if we get a turnover packet, or we're out of time, we can calc damage and show results.
    if (u->opcode == OP_IMOVED) {
      memcpy(&current_enemy, u, sizeof(user));

      theirattack = current_enemy.attack_bitmap;
      chprintf(stream, "\r\nRECV MOVE: (postmove) %d. our move %d.\r\n", theirattack, ourattack);      
    }
    
    if ((u->opcode == OP_IMOVED) || (countdown <= 0)) {
      // force to the next state, because now we have both moves.
      next_fight_state = SHOW_RESULTS;
      sendGamePacket(OP_TURNOVER);
      return;
    }

    if (u->opcode == OP_TURNOVER) { 
      // if we are post move, and we get a turnover packet, then just go straight to
      // results, we don't need an ACK at this point.
      current_fight_state = SHOW_RESULTS;
      return;
    }
    break;
  case NONE: // no-op
    break;
  case APPROVAL_DEMAND: // no-op
    break;
  case VS_SCREEN: // no-op
    break;
  case PLAYER_DEAD: // no-op
    break;
  case ENEMY_DEAD: // no-op
    break;
  case SHOW_RESULTS: // no-op
    break;
  }

}

static uint32_t fight_init(OrchardAppContext *context) {
  FightHandles *p;
  userconfig *config = getConfig();
  
  if (context == NULL) {
    /* This should only happen for auto-init */
    radioHandlerSet (&KRADIO1, RADIO_PROTOCOL_FIGHT,
		     fightRadioEventHandler);
  } else {
    p = chHeapAlloc (NULL, sizeof(FightHandles));
    memset(p, 0, sizeof(FightHandles));
    context->priv = p;

    if (config->in_combat != 0) { 
      chprintf(stream, "You were stuck in combat. Fixed.\r\n");
      config->in_combat = 0;
      configSave(config);
    }

  }

  return 0;
}

orchard_app("Fight", APP_FLAG_AUTOINIT, fight_init, fight_start, fight_event, fight_exit);


