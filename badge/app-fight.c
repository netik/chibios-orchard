#include <stdio.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "rand.h"

#include "orchard-app.h"
#include "led.h"
#include "radio_lld.h"
#include "orchard-ui.h"
#include "images.h"

#include "ides_gfx.h"
#include "fontlist.h"
#include "sound.h"

#include "userconfig.h"
#include "app-fight.h"

/* Globals */
static int32_t countdown = DEFAULT_WAIT_TIME; // used to hold a generic timer value. 
static user current_enemy;                    // current enemy we are attacking/talking to 
static user packet;                           // the last packet we sent, for retransmission

static uint8_t started_it = 0;                  // if 1, we started the fight. 
volatile fight_state current_fight_state = IDLE;  // current state 
static fight_state next_fight_state = NONE;     // upon ACK, we will transition to this state. 
static uint8_t current_enemy_idx = 0;
static uint32_t last_ui_time = 0;
static uint32_t last_tick_time = 0;
static uint32_t last_send_time = 0;
static uint8_t ourattack = 0;
static uint8_t theirattack = 0;
static uint32_t lastseq = 0;
static int16_t last_hit = -1;
static int16_t last_damage = -1;
static uint8_t turnover_sent = false;       // make sure we only transmit turnover once. 
static font_t fontSM;
static font_t fontLG;
static font_t fontFF;
static font_t fontXS;
static uint16_t screen_width;
static uint16_t screen_height;
static uint16_t fontsm_height;
static uint16_t fontlg_height;

static void changeState(fight_state nextstate) {
  // call previous state exit
  // so long as we are updating state, no one else gets in.

  if (nextstate == current_fight_state) {
    // do nothing.
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream, "\r\nFIGHT: ignoring state change request, as we are already in state %d: %s...\r\n", nextstate, fight_state_name[nextstate]);
#endif
    return;
  }
#ifdef DEBUG_FIGHT_STATE
  chprintf(stream, "\r\nFIGHT: moving to state %d: %s...\r\n", nextstate, fight_state_name[nextstate]);
#endif
  
  if (fight_funcs[current_fight_state].exit != NULL) {
    fight_funcs[current_fight_state].exit();
  }

  current_fight_state = nextstate;

  // enter the new state
  if (fight_funcs[current_fight_state].enter != NULL) { 
    fight_funcs[current_fight_state].enter();
  }

}

static void screen_alert_draw(char *msg) {
  gdispClear(Black);
  gdispDrawStringBox (0,
		      (gdispGetHeight() / 2) - (gdispGetFontMetric(fontFF, fontHeight) / 2),
		      gdispGetWidth(),
		      gdispGetFontMetric(fontFF, fontHeight),
		      msg,
		      fontFF, Red, justifyCenter);
}

//--------------------- state funcs
static void state_idle_enter(void) {
  userconfig *config = getConfig();

  FightHandles *p;

  // clean up the fight. This will fire a repaint that erases the area
  // under the button.  So, do not enter this state unless you are
  // ready to clear the screen and the fight is over.

  p = instance.context->priv;

  if (p->ghAttackHi != NULL) 
    gwinDestroy (p->ghAttackHi);

  if (p->ghAttackMid != NULL) 
    gwinDestroy (p->ghAttackMid);

  if (p->ghAttackLow != NULL) 
    gwinDestroy (p->ghAttackLow);

  // reset params
  config->in_combat = 0;
  last_hit = -1;
  last_damage = -1;
  ourattack = 0;
  theirattack = 0;
  memset(&current_enemy, 0, sizeof(current_enemy));
  memset(&packet, 0, sizeof(packet));
  
  ledSetProgress(-1);
}

static void state_waitack_enter(void) {
  // entering WAITACK means we've sent a packet.
  last_send_time = chVTGetSystemTime();
}

static void state_waitack_exit(void) {
  // no-op for now
}

static void state_waitack_tick(void) { 
  // transmit/retry logic
#ifdef DEBUG_FIGHT_TICK
  chprintf(stream, "\r\nwaitack tick: %d %d %d\r\n",
           chVTGetSystemTime(),
           last_send_time,
           MAX_ACKWAIT);
#endif /* DEBUG_FIGHT_TICK */
  
  if ( (chVTGetSystemTime() - last_send_time) > MAX_ACKWAIT ) {
    if (packet.ttl > 0)  {
      chprintf(stream, "\r\nResending packet...\r\n");
      resendPacket();
    } else { 
      orchardAppTimer(instance.context, 0, false); // shut down the timer
      screen_alert_draw("OTHER PLAYER WENT AWAY");
      playHardFail();
      chThdSleepMilliseconds(ALERT_DELAY);
      orchardAppRun(orchardAppByName("Badge"));
      return;
    }
  }
}

static void state_approval_demand_enter(void) {
  GWidgetInit wi;
  FightHandles *p = instance.context->priv;
  char tmp[40];
  int xpos, ypos;

  gdispClear(Black);

  putImageFile(getAvatarImage(current_enemy.p_type, "idla", 1, true),
                              POS_PLAYER2_X, POS_PLAYER2_Y - 20);

  gdispDrawStringBox (0,
		      0,
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

  // hp bar
  drawProgressBar(xpos,ypos,100,10,maxhp(current_enemy.level), current_enemy.hp, 0, false);
  gwinSetDefaultFont(fontFF);
  
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

  last_tick_time = chVTGetSystemTime();
  countdown=DEFAULT_WAIT_TIME;

  started_it = 0;
  playAttacked();
  
}

static void state_approval_demand_exit(void) {
  FightHandles *p = instance.context->priv;

  gwinDestroy (p->ghDeny);
  gwinDestroy (p->ghAccept);
  p->ghDeny = NULL;
  p->ghAccept = NULL;
}

static void state_nextround_enter() { 
#ifdef DEBUG_FIGHT_TICK
  chprintf(stream, "\r\nFight: Starting new round!\r\n");
#endif
  changeState(MOVE_SELECT);
}

static void state_move_select_enter() {
  FightHandles *p;
  userconfig *config = getConfig();
  
  p = instance.context->priv;

  ourattack = 0;
  theirattack = 0;
  last_damage = -1;
  last_hit = -1;
  turnover_sent = false;

  clearstatus();

  updatehp();

  putImageFile(getAvatarImage(config->p_type, "idla", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  
  putImageFile(getAvatarImage(current_enemy.p_type, "idla", 1, true),
               POS_PLAYER2_X, POS_PLAYER2_Y);

  putImageFile("ar50.rgb",
               160,
               POS_PLAYER2_Y);
  putImageFile("ar50.rgb",
               160,
               POS_PLAYER2_Y+50);
  putImageFile("ar50.rgb",
               160,
               POS_PLAYER2_Y+100);

  gdispDrawStringBox (0,
                      0,
                      screen_width,
                      fontsm_height,
                      "Choose attack!",
                      fontSM, White, justifyCenter);
  
  // don't redraw if we don't have to.
  if (p->ghAttackLow == NULL) 
    draw_attack_buttons();
  
  last_tick_time = chVTGetSystemTime();
  countdown=MOVE_WAIT_TIME;

}

static void state_move_select_tick() {
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y,PROGRESS_BAR_W,PROGRESS_BAR_H,MOVE_WAIT_TIME,countdown, true, false);
  
  if (countdown <= 0) {
    // transmit my move
#ifdef DEBUG_FIGHT_NETWORK
    chprintf(stream, "\r\nTIMEOUT in select, sending turnover\r\n");
#endif /* DEBUG_FIGHT_NETWORK */
    
    // we're in MOVE_SELECT, so this tells us that we haven't moved at all.
    // we'll now tell our opponent, that our turn is over.
    last_hit=0;
    next_fight_state = POST_MOVE;
    sendGamePacket(OP_TURNOVER);
  }
}

static void state_move_select_exit() {

}

static void screen_select_draw(int8_t initial) {
  // enemy selection screen
  // setting initial to TRUE will cause a repaint of the entire
  // scene. We set this to FALSE on next/previous moves to improve
  // redraw performance
  user **enemies = enemiesGet();

  if (initial == TRUE) { 
    gdispClear(Black);
    putImageFile(IMG_GROUND_BCK, 0, POS_FLOOR_Y);
  } else { 
    // blank out the center
    gdispFillArea(31,22,260,POS_FLOOR_Y-22,Black);
  }
  
  putImageFile(getAvatarImage(enemies[current_enemy_idx]->p_type, "idla", 1, false),
               POS_PCENTER_X, POS_PCENTER_Y);
  
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
  
  drawProgressBar(xpos,ypos,100,10,maxhp(enemies[current_enemy_idx]->level), enemies[current_enemy_idx]->hp, 0, false);

}

static void state_enemy_select_enter(void) {
  screen_select_draw(TRUE);
  draw_select_buttons();
}

static void state_enemy_select_tick(void) {
  if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
    orchardAppRun(orchardAppByName("Badge"));
  } 
}

static void state_enemy_select_exit() { 
  // shut down the select screen 

  FightHandles *p = instance.context->priv;

  gdispClear(Black);  

  gwinDestroy (p->ghAttack);
  gwinDestroy (p->ghExitButton);
  gwinDestroy (p->ghNextEnemy);
  gwinDestroy (p->ghPrevEnemy);

  p->ghAttack = NULL;
  p->ghExitButton = NULL;
  p->ghNextEnemy = NULL;
  p->ghPrevEnemy = NULL;
}

static void draw_idle_players() {
  uint16_t ypos = 0; // cursor, so if we move or add things we don't have to rethink this
  userconfig *config = getConfig();

  gdispClear(Black);

  putImageFile(getAvatarImage(config->p_type, "idla", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  putImageFile(getAvatarImage(current_enemy.p_type, "idla", 1, true),
               POS_PLAYER2_X, POS_PLAYER2_Y);
 
  gdispDrawStringBox (0,
		      ypos,
		      screen_width,
		      fontsm_height,
		      config->name,
		      fontSM, Lime, justifyLeft);

  gdispDrawStringBox (0,
		      ypos,
		      screen_width,
		      fontsm_height,
                      current_enemy.name,
		      fontSM, Red, justifyRight);

  ypos = ypos + gdispGetFontMetric(fontSM, fontHeight);

  putImageFile(IMG_GROUND_BCK, 0, POS_FLOOR_Y);
  
}
  
static void state_vs_screen_enter() {
  // versus screen!
  userconfig *config = getConfig();
  
  gdispClear(Black);
  draw_idle_players();
  
  blinkText(0,
            110,
            screen_width,
            fontlg_height,
            "VS",
            fontLG,
            White,
            justifyCenter,
            10,
            200);

  // animate them 
  // we always say we're waiting.
  gdispDrawStringBox (0,
                      STATUS_Y,
                      screen_width,
                      fontsm_height,
                      "GET READY!",
                      fontSM, White, justifyCenter);
  
  for (uint8_t i=0; i < 3; i++) {
    putImageFile(getAvatarImage(config->p_type, "atth", 1, false),
                 POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(getAvatarImage(current_enemy.p_type, "atth", 1, true),
                 POS_PLAYER2_X, POS_PLAYER2_Y);
    chThdSleepMilliseconds(200);
    putImageFile(getAvatarImage(config->p_type, "atth", 2, false),
                 POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(getAvatarImage(current_enemy.p_type, "atth", 2, true),
                 POS_PLAYER2_X, POS_PLAYER2_Y);
    chThdSleepMilliseconds(200);
  }
    
  changeState(MOVE_SELECT);
}

static void countdown_tick() {
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y,PROGRESS_BAR_W,PROGRESS_BAR_H,DEFAULT_WAIT_TIME,countdown, true, false);
  
  if (countdown <= 0) {
    orchardAppTimer(instance.context, 0, false); // shut down the timer
    screen_alert_draw("TIMED OUT!");
    playHardFail();
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
  }
}

static void state_approval_demand_tick() {
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y+10,PROGRESS_BAR_W,PROGRESS_BAR_H,DEFAULT_WAIT_TIME,countdown, true, false);
  
  if (countdown <= 0) {
    orchardAppTimer(instance.context, 0, false); // shut down the timer
    screen_alert_draw("TIMED OUT!");
    playHardFail();
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
  }
}


static void state_post_move_tick() {
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y,PROGRESS_BAR_W,PROGRESS_BAR_H,MOVE_WAIT_TIME,countdown, true, false);
  
  // if we have a move and they have a move, then we go straight to show results
  // and we send them a op_turnover packet to push them along
  
  if ( ( ((theirattack & ATTACK_MASK) > 0) &&
         ((ourattack & ATTACK_MASK) > 0)) ||
       (countdown <=0 )) {
    
    if (turnover_sent == false) {  // only send it once.
      next_fight_state = SHOW_RESULTS;
#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "\r\nwe are post-move, sending turnover\r\n");
#endif
      sendGamePacket(OP_TURNOVER);
      turnover_sent = true;
    }
  }
}

static void state_show_results_enter() {
  clearstatus();
  gdispFillArea(0,gdispGetHeight() - 20,gdispGetWidth(),20,Black);
  show_results();
}

static void state_show_results_tick() { 
}    

//--------------------- END state funcs

static void clearstatus(void) {
  // clear instruction area
  gdispFillArea(0,
		STATUS_Y,
		gdispGetWidth(),
		23,
		Black);
}


static uint8_t calc_level(uint16_t xp) {
  if(xp >= 7600) {
    return 10;
  }
  if(xp >= 6480) {
    return 9;
  }
  if(xp >= 5440) {
    return 8;
  }
  if(xp >= 4480) {
    return 7;
  }
  if(xp >= 3600) {
    return 6;
  }
  if(xp >= 2800) {
    return 5;
  }
  if(xp >= 2080) {
    return 4;
  }
  if(xp >= 1440) {
    return 3;
  }
  if(xp >= 880) {
    return 2;
  }
  return 1;
}

uint16_t calc_hit(userconfig *config, user *current_enemy) {
  uint8_t basedmg;
  uint8_t basemult;

  // 15-20
  basemult = (rand() % 5) + 15;
  
  // base multiplier is between 15 and 20 
  basedmg = (basemult * config->level) + config->might - current_enemy->agl;

  // did we crit?
  if ( (int)rand() % 100 > config->luck ) { 
    ourattack |= ATTACK_ISCRIT;
    basedmg = basedmg * 2;
  }
  
  return basedmg;
  
}

static void sendAttack(void) {
  // animate the attack on our screen and send it to the the
  // challenger
  userconfig *config = getConfig();
  
  clearstatus();

  // clear middle
  //  gdispFillArea(140,70,40,110,Black);

  if (ourattack & ATTACK_HI) {
    putImageFile(getAvatarImage(config->p_type, "atth", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  }

  if (ourattack & ATTACK_MID) {
    putImageFile(getAvatarImage(config->p_type, "attm", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  }
  
  if (ourattack & ATTACK_LOW) {
    putImageFile(getAvatarImage(config->p_type, "attl", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  }

  // we always say we're waiting. 
  gdispDrawStringBox (0,
                      STATUS_Y,
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
  
static void draw_select_buttons(void) { 
  GWidgetInit wi;
  coord_t totalheight = gdispGetHeight();
  FightHandles *p = instance.context->priv;

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
  gwinSetDefaultFont(fontFF);
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 85;
  wi.g.y = POS_FLOOR_Y+10;
  wi.g.width = 150;
  wi.g.height = gdispGetFontMetric(fontFF, fontHeight) + 2;
  wi.text = "ATTACK";
  p->ghAttack = gwinButtonCreate(0, &wi);
  gwinSetDefaultStyle(&BlackWidgetStyle, FALSE);
  
  // Exit
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.width = 60;
  wi.g.height = 60;
  wi.g.y = totalheight - 60;
  wi.g.x = 0;
  wi.text = "";
  wi.customDraw = noRender;
  p->ghExitButton = gwinButtonCreate(NULL, &wi);

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
    orchardAppRun(orchardAppByName("Badge"));
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
    orchardAppRun(orchardAppByName("Badge"));
    return FALSE;
  }
}
static void state_levelup_enter(void) {
  gdispClear(Black);
}

static void state_levelup_tick(void) {
}

static void state_levelup_exit(void) {
}

static void show_results(void) {
  // remove the progress bar and status bar
  char c=' ';
  char attackfn1[13];
  char attackfn2[13];
  char ourdmg_s[10];
  char theirdmg_s[10];
  uint16_t textx, text_p1_y, text_p2_y;
  color_t p1color, p2color;
  userconfig *config = getConfig();
  
  // get the right filename for the attack
  if (ourattack & ATTACK_HI) {
    c='h';
    text_p2_y = 40;
  }

  if (ourattack & ATTACK_MID) {
    c='m';
    text_p2_y = 120;
  }
  
  if (ourattack & ATTACK_LOW) {
    c='l';
    text_p2_y = 180;
  }

  if (c == ' ') 
    strcpy(attackfn1, IMG_GIDLA1);
  else 
    chsnprintf(attackfn1, sizeof(attackfn1), "gatt%c1.rgb", c);

  c = ' ';
  textx = 100;
  if (theirattack & ATTACK_HI) {
    c='h';
    text_p1_y = 40;
  }
  
  if (theirattack & ATTACK_MID) {
    c='m';
    text_p1_y = 120;
  }
  
  if (theirattack & ATTACK_LOW) {
    c='l';
    text_p1_y = 180;
  }
  
  if (c == ' ') 
    strcpy(attackfn2, IMG_GIDLA1R);
  else 
    chsnprintf(attackfn2, sizeof(attackfn2), "gatt%c1r.rgb", c);

  // compute damages
  // Both sides have sent damage based on stats.
  // If the hits were the same, deduct some damage

  // if we didn't get a hit at all, they failed to do any damage. 
  if (last_damage == -1) last_damage = 0;
  
  config->hp = config->hp - last_damage;
  if (config->hp < 0) { config->hp = 0; }

  current_enemy.hp = current_enemy.hp - last_hit;
  if (current_enemy.hp < 0) { current_enemy.hp = 0; }

  // animate the characters
  chsnprintf (ourdmg_s, sizeof(ourdmg_s), "-%d", last_damage );
  chsnprintf (theirdmg_s, sizeof(theirdmg_s), "-%d", last_hit );

#ifdef DEBUG_FIGHT_STATE
  chprintf(stream,"FIGHT: Damage report! Us: %d Them: %d", last_damage, last_hit);
#endif
  // 45 down. 
  // 140 | -40- | 140
  for (uint8_t i=0; i < 2; i++) { 
    putImageFile(attackfn1, POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(attackfn2, POS_PLAYER2_X, POS_PLAYER2_Y);

    p1color = Red;
    p2color = Red;

    if (theirattack & ATTACK_ISCRIT) { p1color = Green; }
    if (ourattack & ATTACK_ISCRIT) { p2color = Green; }
    
    // you attacking us
    gdispDrawStringBox (textx,text_p1_y,50,50,ourdmg_s,fontFF,p1color,justifyLeft);
    
    // us attacking you
    gdispDrawStringBox (textx+60,text_p2_y,50,50,theirdmg_s,fontFF,p2color,justifyRight);

    if (theirattack & ATTACK_ISCRIT || ourattack & ATTACK_ISCRIT) {
      playHit();      
    }

    playHit();

    chThdSleepMilliseconds(300);
    gdispDrawStringBox (textx,text_p1_y,50,50,ourdmg_s,fontFF,Black,justifyLeft);
    gdispDrawStringBox (textx+60,text_p2_y,50,50,theirdmg_s,fontFF,Black,justifyRight);

    putImageFile(IMG_GUARD_IDLE_L, POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(IMG_GUARD_IDLE_R, POS_PLAYER2_X, POS_PLAYER2_Y);
    chThdSleepMilliseconds(100);
  }

  // update the health bars
  updatehp();
  
  /* if I kill you, then I will show victory and head back to the badge screen */

  if (current_enemy.hp == 0 && config->hp == 0) {
    /* both are dead, so whomever started the fight, wins */
    if (started_it == 1) {
      config->hp = 1;
    } else {
      current_enemy.hp = 1;
    }
  }
  
  if (current_enemy.hp == 0) {
    changeState(ENEMY_DEAD);
    playVictory();
    screen_alert_draw("VICTORY!");
    config->xp += (80 + ((config->level-1) * 16));
    config->won++;
    configSave(config);
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
  } else {
    if (config->hp == 0) {
      changeState(PLAYER_DEAD);
      /* if you are dead, then you will do the same */
      playDefeat();
      screen_alert_draw("YOU ARE DEFEATED.");
      config->xp += (10 + ((config->level-1) * 16));
      config->lost++;
      configSave(config);
      chThdSleepMilliseconds(ALERT_DELAY);
      orchardAppRun(orchardAppByName("Badge"));
    }
  }  

  if (config->hp == 0 || current_enemy.hp == 0) {
    // check for level UP
    if ((config->level != calc_level(config->xp)) && (config->level != 10)) {
      config->level++;
      configSave(config);
      changeState(LEVELUP);
    }

    return;
  }

  next_fight_state = NEXTROUND;
  sendGamePacket(OP_NEXTROUND);
}

static void updatehp(void)  {
  // update the hit point counters at the top of the screen
  userconfig *config = getConfig();
  char tmp[20];
  
  // center KO 
  gdispDrawStringBox (1, 16, gdispGetWidth()-1,
		      gdispGetFontMetric(fontSM, fontHeight),
		      "KO",
		      fontSM, Yellow, justifyCenter);

  gdispDrawStringBox (0, 15, gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      "KO",
		      fontSM, Red, justifyCenter);
  // hp bars
  drawProgressBar(35,22,100,12,maxhp(config->level),config->hp,false,true);
  drawProgressBar(185,22,100,12,maxhp(current_enemy.level),current_enemy.hp,false,false);

  // numeric hp indicators
  gdispFillArea( 0, 23,
                 35, gdispGetFontMetric(fontXS, fontHeight),
                 Black );

  gdispFillArea( 289, 23,
                 30, gdispGetFontMetric(fontXS, fontHeight),
                 Black );

  chsnprintf(tmp, sizeof(tmp), "%d", config->hp);
  gdispDrawStringBox (0,
		      23,
		      35,
		      gdispGetFontMetric(fontXS, fontHeight),
		      tmp,
		      fontXS, White, justifyRight);

  chsnprintf(tmp, sizeof(tmp), "%d", current_enemy.hp);

  gdispDrawStringBox (289,
		      23,
		      29,
		      gdispGetFontMetric(fontXS, fontHeight),
		      tmp,
		      fontXS, White, justifyLeft);
  
}

static void fight_start(OrchardAppContext *context) {
  FightHandles *p = context->priv;
  user **enemies = enemiesGet();

  p = chHeapAlloc (NULL, sizeof(FightHandles));
  memset(p, 0, sizeof(FightHandles));
  context->priv = p;

  last_ui_time = chVTGetSystemTime();
  orchardAppTimer(context, FRAME_INTERVAL_US, true);

  geventListenerInit(&p->glFight);
  gwinAttachListener(&p->glFight);
  geventRegisterCallback (&p->glFight, orchardAppUgfxCallback, &p->glFight);

  // are we entering a fight?
  chprintf(stream, "\r\nFIGHT: entering with enemy %08x state %d\r\n", current_enemy.netid, current_fight_state);
  last_tick_time = chVTGetSystemTime();
    
  if (current_enemy.netid != 0 && current_fight_state != APPROVAL_DEMAND) {
    changeState(APPROVAL_DEMAND);
  }

  if (current_fight_state == IDLE) {
    if (enemyCount() > 0) {
      changeState( ENEMY_SELECT );
      
      if (enemies[current_enemy_idx] == NULL) {
	nextEnemy();
      }
    } else {
      // punt if no enemies
      screen_alert_draw("NO ENEMIES NEARBY!");
      chThdSleepMilliseconds(ALERT_DELAY);
      orchardAppRun(orchardAppByName("Badge"));
      return;
    }
  }
}

static void state_approval_wait_enter(void) {
  draw_idle_players();

  // progress bar
  last_tick_time = chVTGetSystemTime(); 
  countdown = DEFAULT_WAIT_TIME; // used to hold a generic timer value.
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y,PROGRESS_BAR_W,PROGRESS_BAR_H,DEFAULT_WAIT_TIME,countdown, true, false);

  gdispDrawStringBox (0,
                      STATUS_Y,
                      screen_width,
                      fontsm_height,
                      "Waiting for enemy to accept!",
                      fontSM, White, justifyCenter);

}

static void sendACK(user *inbound) {
  // acknowlege an inbound packet.
  userconfig *config;
  config = getConfig();

  user ackpacket;
  memset (&ackpacket, 0, sizeof(ackpacket));
  
  ackpacket.seq = lastseq++;
  ackpacket.netid = config->netid; // netid is id of sender, always.
  ackpacket.acknum = inbound->seq;
  ackpacket.ttl = 4;
  ackpacket.opcode = OP_ACK;
#ifdef DEBUG_FIGHT_NETWORK
  chprintf(stream, "\r\ntransmit ACK (to=%08x for seq %d): ourstate=%d, ttl=%d, myseq=%d, theiropcode=0x%x\r\n",
           inbound->netid,
           inbound->seq,
           current_fight_state,
           inbound->ttl,
           ackpacket.seq,
           inbound->opcode);
#endif /* DEBUG_FIGHT_NETWORK */
  radioSend (&KRADIO1, inbound->netid, RADIO_PROTOCOL_FIGHT, sizeof(ackpacket), &ackpacket);
}

static void sendRST(user *inbound) {
  // do the minimal amount of work to send a reset packet based on an
  // inbound packet.
  userconfig *config;
  config = getConfig();
  user rstpacket;
  
  memset (&rstpacket, 0, sizeof(rstpacket)); 
  rstpacket.seq = ++lastseq;
  rstpacket.netid = config->netid; // netid is id of sender, always.
  rstpacket.acknum = inbound->seq; 
  rstpacket.ttl = 4;
  rstpacket.opcode = OP_RST;

#ifdef DEBUG_FIGHT_NETWORK
  chprintf(stream, "\r\nTransmit RST (to=%08x for seq %d): currentstate=%d, ttl=%d, myseq=%d, opcode=0x%x\r\n",
           inbound->netid,
           current_fight_state,
           inbound->seq,
           rstpacket.ttl,
           rstpacket.seq,
           rstpacket.opcode);
#endif /* DEBUG_FIGHT_NETWORK */
  radioSend (&KRADIO1, inbound->netid, RADIO_PROTOCOL_FIGHT, sizeof(rstpacket), &rstpacket);
}

static void resendPacket(void) {
  // resend the last sent packet, pausing a random amount of time to prevent collisions
  packet.ttl--; // deduct TTL
  chThdSleepMilliseconds(rand() % MAX_HOLDOFF);

#ifdef DEBUG_FIGHT_NETWORK
  chprintf(stream, "\r\n%ld Transmit (to=%08x): currentstate=%d, ttl=%d, seq=%d, opcode=0x%x\r\n", chVTGetSystemTime()  , current_enemy.netid, current_fight_state, packet.ttl, packet.seq, packet.opcode);
#endif /* DEBUG_FIGHT_NETWORK */
  last_send_time = chVTGetSystemTime();
  radioSend (&KRADIO1, current_enemy.netid, RADIO_PROTOCOL_FIGHT, sizeof(packet), &packet);
  changeState(WAITACK);
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

  packet.agl = config->agl;
  packet.luck = config->luck;
  packet.might = config->might;

  /* we'll always send -1, unless the user has made a move, then we'll calculate the hit. */
  if (last_hit == -1 && ourattack != 0) {
    // if this is a turn-over packet, then we also transmit damage (to
    // them). calc_hit will also set ATTACK_ISCRIT in ourattack if
    // need be.
    last_hit = calc_hit(config, &current_enemy);
  }

  packet.damage = last_hit;
  packet.attack_bitmap = ourattack;
  
  resendPacket();
}

static void fight_event(OrchardAppContext *context,
			const OrchardAppEvent *event) {

  FightHandles * p = context->priv;
  GEvent * pe;
  user **enemies = enemiesGet();
  userconfig *config = getConfig();

  if (event->type == radioEvent && event->radio.pPkt != NULL) {
    radio_event_do(event->radio.pPkt);
    return;
  }
  
  // the timerEvent fires on the frame interval. It will always call
  // the current state's tick() method. tick() is expected to handle
  // animations or otherwise.
  
  if (event->type == timerEvent) {
#ifdef DEBUG_FIGHT_TICK
    chprintf(stream, "tick @ %d mystate is %d, countdown is %d, last tick %d, ourattack %d, theirattack %d, delta %d\r\n", chVTGetSystemTime(),
             current_fight_state,
             countdown,
             last_tick_time,
             ourattack,
             theirattack,
             (chVTGetSystemTime() - last_tick_time));
#endif /* DEBUG_FIGHT_TICK */

    if (fight_funcs[current_fight_state].tick != NULL) // some states lack a tick call
      fight_funcs[current_fight_state].tick();
    
    // update the clock regardless of state.
    if (countdown > 0) {
      // our time reference is based on elapsed time. We will init if need be.
      if (last_tick_time != 0) { 
        countdown = countdown - ( chVTGetSystemTime() - last_tick_time );
      }

      last_tick_time = chVTGetSystemTime();
    }

  }

  // handle UGFX events.
  if (event->type == ugfxEvent) {
    pe = event->ugfx.pEvent;

#ifdef DEBUG_FIGHT_STATE
    chprintf(stream, "ugfxevent (type %d) during state %d\r\n", pe->type, current_fight_state);
#endif

    // we handle all of the buttons on all of the possible screens
    // in this event handler, along with our countdowns
    switch(pe->type) {
    case GEVENT_GWIN_BUTTON:
      last_ui_time = chVTGetSystemTime();

      if ( ((GEventGWinButton*)pe)->gwin == p->ghExitButton) { 
        chprintf(stream, "\r\nFIGHT: e switch app\r\n");
        orchardAppRun(orchardAppByName("Badge"));
        return;
      }
      if ( ((GEventGWinButton*)pe)->gwin == p->ghNextEnemy) { 
        if (nextEnemy()) {
          screen_select_draw(FALSE);
        }
        return;
      }
      if ( ((GEventGWinButton*)pe)->gwin == p->ghAttack) { 
        // we are attacking.
        started_it = 1;
        memcpy(&current_enemy, enemies[current_enemy_idx], sizeof(user));
        config->in_combat = true;

        // We're going to preemptively call this before changing state
        // so the screen doesn't go black while we wait for an ACK.
        next_fight_state = APPROVAL_WAIT;
        screen_alert_draw("Connecting...");
        sendGamePacket(OP_STARTBATTLE);
        return;
      }

      if ( ((GEventGWinButton*)pe)->gwin == p->ghDeny) { 
        // "war is sweet to those who have not experienced it."
        // Pindar, 522-443 BC.
        // do our cleanups now so that the screen doesn't redraw while exiting
        orchardAppTimer(context, 0, false); // shut down the timer
        
        next_fight_state = IDLE;
        sendGamePacket(OP_DECLINED);
        screen_alert_draw("Dulce bellum inexpertis.");
        playHardFail();
        chThdSleepMilliseconds(ALERT_DELAY);
        orchardAppRun(orchardAppByName("Badge"));
        return;
      }
      if ( ((GEventGWinButton*)pe)->gwin == p->ghAccept) {
        config->in_combat = true;
        next_fight_state = VS_SCREEN;
        sendGamePacket(OP_STARTBATTLE_ACK);
        return;
      }
      
      if ( ((GEventGWinButton*)pe)->gwin == p->ghPrevEnemy) { 
        if (prevEnemy()) {
          screen_select_draw(FALSE);
        }
        return;
      }
      
      if ((ourattack & ATTACK_MASK) == 0) {
        // TODO -- modify for three attacks per Egan's spec.
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

  chprintf(stream, "\r\nFIGHT: fight_exit\r\n");

  // don't change back to idle state from any other function. Let fight_exit take care of it.
  changeState(IDLE);
  
  geventDetachSource (&p->glFight, NULL);
  geventRegisterCallback (&p->glFight, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;

  config->in_combat = 0;

  gdispCloseFont(fontLG);
  gdispCloseFont(fontSM);
  gdispCloseFont(fontFF);
  gdispCloseFont(fontXS);
  return;
}

static void fightRadioEventHandler(KW01_PKT * pkt) {
  user *u;
  userconfig *config = getConfig();

  /* this code runs in the MAIN thread, it runs along side our thread */
  /* everything else runs in the app's thread */
  
  u = (user *)pkt->kw01_payload; 

  if (u->opcode == OP_STARTBATTLE && config->in_combat == 0) {
    /* we can accept a new fight */
    memcpy(&current_enemy, u, sizeof(user));
    ourattack = 0;
    theirattack = 0;

    if (instance.app != orchardAppByName("Fight")) {
      // not in app
      orchardAppRun(orchardAppByName("Fight"));
    }
  }
  
  orchardAppRadioCallback (pkt);
  return;
}

static void radio_event_do(KW01_PKT * pkt)
{
  /* this is the state machine that handles transitions for the game via the radio */
  user *u;
  u = (user *)pkt->kw01_payload; 

  if (pkt->kw01_hdr.kw01_prot != RADIO_PROTOCOL_FIGHT ) {
    /* the event hander seems to broadcast events to everyone. Ignore
       the ones that are not for us */
    return;
  }

  /* does this payload contain damage information? */
  if (u->damage != -1 && u->opcode != OP_ACK && u->opcode != OP_RST) { 
    theirattack = u->attack_bitmap;
    last_damage = u->damage;
  }

  
  if (u->opcode == 0) {
#ifdef DEBUG_FIGHT_NETWORK
    chprintf(stream, "\r\n%08x --> RECV Invalid opcode. (seq=%d) Ignored (%08x to %08x proto %x)\r\n", u->netid, u->seq, pkt->kw01_hdr.kw01_src, pkt->kw01_hdr.kw01_dst, pkt->kw01_hdr.kw01_prot);
#endif /* DEBUG_FIGHT_NETWORK */
    return;
  }

  // DEBUG
#ifdef DEBUG_FIGHT_NETWORK
  if (u->opcode == OP_ACK) {
    chprintf(stream, "\r\n%08x --> RECV ACK (seq=%d, acknum=%d, mystate=%d, opcode 0x%x)\r\n", u->netid, u->seq, u->acknum, current_fight_state, next_fight_state, u->opcode);
  } else { 
    chprintf(stream, "\r\n%08x --> RECV OP  (proto=0x%x, seq=%d, mystate=%d, opcode 0x%x)\r\n", u->netid, pkt->kw01_hdr.kw01_prot, u->seq, current_fight_state, u->opcode);
  }
#endif /* DEBUG_FIGHT_NETWORK */
  
  if ( ((current_fight_state == IDLE) && (u->opcode >= OP_IMOVED)) && (u->opcode != OP_ACK)) {
    // this is an invalid state, which we should not ACK for.
    // Let the remote client die.
#ifdef DEBUG_FIGHT_NETWORK
    chprintf(stream, "\r\n%08x --> SEND RST: rejecting opcode 0x%x beacuse i am idle\r\n", u->netid, u->opcode);
#endif /* DEBUG_FIGHT_NETWORK */
    sendRST(u);
    // we are idle, do nothing. 
    return;
  }

  // Immediately ACK non-ACK / RST packets. We do not support retry on
  // ACK, because we support ARQ just like TCP/IP.  without the ACK,
  // the sender will retransmit automatically.
  if (u->opcode != OP_ACK && u->opcode != OP_RST)
      sendACK(u);
    
  switch (current_fight_state) {
  case ENEMY_SELECT:
    if (u->opcode == OP_STARTBATTLE) {
      memcpy(&current_enemy, u, sizeof(user));
      ourattack = 0;
      theirattack = 0;
      changeState(APPROVAL_DEMAND);
      return;
    }
    break;
  case WAITACK:
    if (u->opcode == OP_RST) {
      orchardAppTimer(instance.context, 0, false); // shut down the timer
      screen_alert_draw("CONNECTION LOST");
      playHardFail();
      chThdSleepMilliseconds(ALERT_DELAY);
      orchardAppRun(orchardAppByName("Badge"));
      return;
    }

    if (u->opcode == OP_ACK) { 
      // if we are waiting for an ack, advance our state.
      // however, if we have no next-state to go to, then we will timeout
      if (next_fight_state != NONE) {
#ifdef DEBUG_FIGHT_NETWORK
        chprintf(stream, "\r\n%08x --> moving to state %d from state %d due to ACK\n",
                 u->netid, next_fight_state, current_fight_state, next_fight_state);
#endif /* DEBUG_FIGHT_NETWORK */
      changeState(next_fight_state);
      return;
      }
    }

    if (u->opcode == OP_TURNOVER) { 
      // there's an edge case where we have sent turnover, and they have
      // sent turn over, but, we are waiting on an ACK.
      // so, act as if we got the ACK and move on.
      // we will have already ACK'd their packet anyway. 
#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "\r\n%08x --> got turnover in wait_ack? nextstate=0x%x, currentstate=0x%x, damage=%d\r\n    We're waiting on seq %d opcode %d\r\n",
               u->netid,
               next_fight_state,
               current_fight_state,
               u->damage,
               packet.seq,
               packet.opcode);
#endif /* DEBUG_FIGHT_NETWORK */

      if (next_fight_state == SHOW_RESULTS) {
        // force state change
        changeState(SHOW_RESULTS);
        return;
      }
    }

    if (u->opcode == OP_NEXTROUND) {
#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "\r\n%08x --> got nextround in wait_ack? nextstate=0x%x, currentstate=0x%x\r\n    We're waiting on seq %d opcode %d",
               u->netid,
               next_fight_state,
               current_fight_state,
               packet.seq,
               packet.opcode);
#endif /* DEBUG_FIGHT_NETWORK */
      
      if (next_fight_state == NEXTROUND) {
        // edge-case/packet pile up. Just advance the round and forget about the ACK. 
        changeState(MOVE_SELECT);
        return;
      }
    }
    break;
  case APPROVAL_WAIT:
    if (u->opcode == OP_DECLINED) {
      orchardAppTimer(instance.context, 0, false); // shut down the timer
      screen_alert_draw("DENIED.");
      playHardFail();
      ledSetProgress(-1);
      chThdSleepMilliseconds(ALERT_DELAY);
      changeState(ENEMY_SELECT);
      orchardAppTimer(instance.context, FRAME_INTERVAL_US, true);
    }

    if (u->opcode == OP_STARTBATTLE_ACK) {
      // i am the attacker and you just ACK'd me. The timer will render this.
      changeState(VS_SCREEN);
      return;
    }
    break;
    
  case MOVE_SELECT:
    if (u->opcode == OP_IMOVED) {
      // If I see OP_IMOVED from the other side while we are in
      // MOVE_SELECT, store that move for later. 
      
#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "\r\nRECV MOVE: (select) %d. our move %d.\r\n", theirattack, ourattack);
#endif /* DEBUG_FIGHT_NETWORK */

    }
    if (u->opcode == OP_TURNOVER) {
      // uh-oh, the turn is over and we haven't moved!

      next_fight_state = POST_MOVE;
#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "\r\nmove_select/imoved, sending turnover\r\n");
#endif /* DEBUG_FIGHT_NETWORK */
      
      sendGamePacket(OP_TURNOVER);
    }
    break;
  case POST_MOVE: // no-op
    // if we get a turnover packet, or we're out of time, we can calc damage and show results.
    if (u->opcode == OP_IMOVED) {

#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "\r\nRECV MOVE: (postmove) %d. our move %d.\r\n", theirattack, ourattack);
#endif /* DEBUG_FIGHT_NETWORK */
      return;
    }
    
    if (u->opcode == OP_TURNOVER) { 
      // if we are post move, and we get a turnover packet, then just go straight to
      // results, we don't need an ACK at this point.
      changeState(SHOW_RESULTS);
      return;
    }
    break;
  case SHOW_RESULTS:
    // we may have already switched into the show results state when
    // the turnover packet comes in. record the damage. 
    if (u->opcode == OP_TURNOVER) {
      if (last_damage == -1) 
        last_damage = u->damage;
    }
    break;

    if (u->opcode == OP_NEXTROUND) {
      changeState(NEXTROUND);
      return;
    }
  default:
    // this shouldn't fire, but log it if it does.
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream, "\r\n%08x --> RECV %x - no handler - (seq=%d, mystate=%d %s)\r\n",
             pkt->kw01_hdr.kw01_src, u->opcode, u->seq,
             current_fight_state, fight_state_name[current_fight_state]);
#else
    chprintf(stream, "\r\n%08x --> RECV %x - no handler - (seq=%d, mystate=%d)\r\n",
             pkt->kw01_hdr.kw01_src, u->opcode, u->seq,
             current_fight_state);
#endif

  }
}

static uint32_t fight_init(OrchardAppContext *context) {
  userconfig *config = getConfig();

  fontSM = gdispOpenFont(FONT_SM);
  fontLG = gdispOpenFont(FONT_LG);
  fontXS = gdispOpenFont (FONT_XS);
  fontFF = gdispOpenFont (FONT_FIXED);
  screen_width = gdispGetWidth();
  screen_height = gdispGetHeight();
  
  fontsm_height = gdispGetFontMetric(fontSM, fontHeight);
  fontlg_height = gdispGetFontMetric(fontLG, fontHeight);

  if (context == NULL) {
    /* This should only happen for auto-init */
    radioHandlerSet (&KRADIO1, RADIO_PROTOCOL_FIGHT,
		     fightRadioEventHandler);
  } else {
    if (config->in_combat != 0) { 
      chprintf(stream, "You were stuck in combat. Fixed.\r\n");
      config->in_combat = 0;
      configSave(config);
    }
    
  }

  // we don't want any stack space added to us. 
  return 0;
}

orchard_app("Fight", APP_FLAG_AUTOINIT,
            fight_init, fight_start, fight_event, fight_exit);


