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
#include "ides_config.h"
#include "fontlist.h"
#include "sound.h"
#include "dac_lld.h"

#include "userconfig.h"
#include "unlocks.h"
#include "app-fight.h"

#include "dec2romanstr.h"

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
static uint8_t roundno = 0;
static uint8_t animtick = 1;
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
extern struct FXENTRY fxlist[];

static uint16_t calc_xp_gain(uint8_t won) {
  userconfig *config = getConfig();
  float factor = 1;

  int8_t leveldelta;

  leveldelta = current_enemy.level - config->level; 
  // range:
  //
  //  >=  3 = 2x XP     RED
  //  >=  2 = 1.50x XP  ORG
  //  >=  1 = 1.25x XP  ORG/YEL
  //  ==  0 = 1x XP     GREEN
  //  <= -1 = .75x XP   GREY
  //  <= -2 = .25x XP   GREY
  //  <= -3 = 0 XP      GREY
  
  // you killed someone harder than you
  if (leveldelta >= 3)
    factor = 2;
  else
    if (leveldelta >= 2)
      factor = 1.50;
    else
      if (leveldelta >= 1)
        factor = 1.25;

  // you killed someone weaker than you
  if (leveldelta <= -3)
    factor = 0;
  else
    if (leveldelta <= -2)
      factor = .25;
    else
      if (leveldelta == -1)
        factor = .75;

  if (won) {
    /*
     * XP is calculated as follows
     * 
     * XP = (Base XP) * (1 - (Char Level - Mob Level)/ZD )
     */
    return (80 + ((config->level-1) * 16)) * factor;
  } else {
    /* Death does not factor in the multiplier */
    return (10 + ((config->level-1) * 16));
  }
}

static void changeState(fight_state nextstate) {
  // call previous state exit
  // so long as we are updating state, no one else gets in.

  if (nextstate == current_fight_state) {
    // do nothing.
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream, "FIGHT: ignoring state change request, as we are already in state %d: %s...\r\n", nextstate, fight_state_name[nextstate]);
#endif
    return;
  }
#ifdef DEBUG_FIGHT_STATE
  chprintf(stream, "FIGHT: moving to state %d: %s...\r\n", nextstate, fight_state_name[nextstate]);
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
  
  ledSetFunction(fxlist[config->led_pattern].function);
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
  chprintf(stream, "\r\nwaitack t:%d lastsend:%d max:%d nextstate:%d\r\n",
           chVTGetSystemTime(),
           last_send_time,
           MAX_ACKWAIT,
           next_fight_state);
  ;
#endif /* DEBUG_FIGHT_TICK */
  
  if ( (chVTGetSystemTime() - last_send_time) > MAX_ACKWAIT ) {
    if (packet.ttl > 0)  {
      chprintf(stream, "Resending packet ...\r\n");
      resendPacket();
    } else { 
      orchardAppTimer(instance.context, 0, false); // shut down the timer
      dacPlay("fight/select3.raw");
      screen_alert_draw(true, "OTHER PLAYER WENT AWAY");
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
  userconfig *config = getConfig();
  int xpos, ypos;

  gdispClear(Black);

  putImageFile(getAvatarImage(current_enemy.current_type, "idla", 1, true),
                              POS_PLAYER2_X, POS_PLAYER2_Y - 20);

  gdispDrawStringBox (0,
		      0,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontFF, fontHeight),
		      "YOU ARE BEING ATTACKED!",
		      fontFF, Red, justifyCenter);

  ypos = (gdispGetHeight() / 2) - 60;
  xpos = 10;

  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(fontLG, fontHeight),
		      current_enemy.name,
		      fontLG, Yellow, justifyLeft);
  ypos=ypos+50;
  chsnprintf(tmp, sizeof(tmp), "LEVEL %s", dec2romanstr(current_enemy.level));

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
  drawProgressBar(xpos,ypos,100,10,
                  maxhp(current_enemy.unlocks,
                        current_enemy.level),
                  current_enemy.hp, 0, false);
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

  wi.g.x = 190;
#ifdef notdef
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.y = POS_FLOOR_Y-10;
  wi.g.width = 100;
  wi.g.height = gdispGetFontMetric(fontFF, fontHeight) + 2;
#endif
  wi.text = "FIGHT!";
  p->ghAccept = gwinButtonCreate(0, &wi);

  gwinSetDefaultStyle(&BlackWidgetStyle, FALSE);

  last_tick_time = chVTGetSystemTime();
  countdown=DEFAULT_WAIT_TIME;

  roundno = 0;
  started_it = 0;
  dacStop();
  playAttacked();

  config->lastcombat = chVTGetSystemTime();
  configSave(config);
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
  chprintf(stream, "Fight: Starting new round!\r\n");
#endif
  changeState(MOVE_SELECT);
}

static void state_move_select_enter() {
  FightHandles *p;
  userconfig *config = getConfig();
  char tmp[25];
  
  p = instance.context->priv;

  ourattack = 0;
  theirattack = 0;
  last_damage = -1;
  last_hit = -1;
  turnover_sent = false;

  clearstatus();

  updatehp();

  putImageFile(getAvatarImage(config->current_type, "idla", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  
  putImageFile(getAvatarImage(current_enemy.current_type, "idla", 1, true),
               POS_PLAYER2_X, POS_PLAYER2_Y);

  gdispDrawStringBox (0,
                      STATUS_Y+14,
                      screen_width,
                      fontsm_height,
                      "Choose attack!",
                      fontSM, White, justifyCenter);
  
  // don't redraw if we don't have to.
  if (p->ghAttackLow == NULL) 
    draw_attack_buttons();

  // round N , fight!  note that we only have four of these, so after
  // that we're just going to get file-not-found. oh well, add more
  // later!
  roundno++;
  if (roundno < 5) { 
    chsnprintf(tmp, sizeof(tmp), "fight/round%d.raw", roundno);
    dacPlay(tmp);
    chThdSleepMilliseconds(1000);
  }

  if (current_enemy.hp < (maxhp(current_enemy.unlocks,current_enemy.level) * .2)) {
    if (current_enemy.current_type == p_gladiatrix) { 
      dacPlay("fight/finishf.raw");
    } else {
      dacPlay("fight/finishm.raw");
    }
  } else { 
    dacPlay("fight/fight.raw");
  }
  chThdSleepMilliseconds(1000);

  // remove choose attack message 
  gdispFillArea(0,
		STATUS_Y+14,
		gdispGetWidth(),
                fontsm_height,
		Black);
    
  last_tick_time = chVTGetSystemTime();
  countdown=MOVE_WAIT_TIME;
}

static void state_move_select_tick() {
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y,PROGRESS_BAR_W,PROGRESS_BAR_H,MOVE_WAIT_TIME,countdown, true, false);

  // animate arrows
  gdispFillArea(160, POS_PLAYER2_Y, 50,150,Black);
  putImageFile("ar50.rgb",
               160,
               POS_PLAYER2_Y + (50*animtick));
  animtick++;
  if (animtick > 2) animtick = 0;
  
  if (countdown <= 0) {
    // transmit my move
#ifdef DEBUG_FIGHT_NETWORK
    chprintf(stream, "TIMEOUT in select, sending turnover\r\n");
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
  userconfig *config = getConfig();
  user **enemies = enemiesGet();
  color_t levelcolor;

  // calculate mod/con color, WoW Style
  int leveldelta = enemies[current_enemy_idx]->level - config->level;

  levelcolor = Lime;

  // harder than you 
  if (leveldelta >= 3)
    levelcolor = Red;
  else
    if (leveldelta == 2)
      levelcolor = Orange;
    else
      if (leveldelta == 1)
        levelcolor = Yellow;

  // weaker than you
  if (leveldelta <= -3)
    levelcolor = Grey;
  else
    if (leveldelta == -2)
      levelcolor = Green;
    else
      if (leveldelta == -1)  
        levelcolor = Lime;
  
  if (initial == TRUE) { 
    gdispClear(Black);
    putImageFile(IMG_GROUND_BCK, 0, POS_FLOOR_Y);
  } else { 
    // blank out the center
    gdispFillArea(31,22,260,POS_FLOOR_Y-22,Black);
  }
  
  putImageFile(getAvatarImage(enemies[current_enemy_idx]->current_type, "idla", 1, false),
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
		      fontFF, levelcolor, justifyLeft);

  // level
  ypos = ypos + 25;
  chsnprintf(tmp, sizeof(tmp), "LEVEL %s", dec2romanstr(enemies[current_enemy_idx]->level));
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
  
  drawProgressBar(xpos,ypos,100,10,
                  maxhp(enemies[current_enemy_idx]->unlocks,
                        enemies[current_enemy_idx]->level),
                  enemies[current_enemy_idx]->hp, 0, false);
}

static void state_enemy_select_enter(void) {
  roundno = 0;
  screen_select_draw(TRUE);
  draw_select_buttons();
}

static void state_enemy_select_tick(void) {
  if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
    orchardAppRun(orchardAppByName("Badge"));
  } 
}

static void state_enemy_select_exit() {
  // shut down the select screen, stop music. 
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

static void state_grant_enter(void) {
  gdispClear(Black);
  putImageFile("grants.rgb", 0, 0);
  
  gdispDrawStringBox (0,
		      110,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontSM, fontHeight),
		      current_enemy.name,
		      fontSM, Yellow, justifyCenter);
}

static void state_grant_tick(void) {
  if( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) {
    orchardAppRun(orchardAppByName("Badge"));
  }
}

static void state_grant_exit(void) {
}

static void draw_idle_players() {
  uint16_t ypos = 0; // cursor, so if we move or add things we don't have to rethink this
  userconfig *config = getConfig();

  gdispClear(Black);

  putImageFile(getAvatarImage(config->current_type, "idla", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  putImageFile(getAvatarImage(current_enemy.current_type, "idla", 1, true),
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

  putImageFile(IMG_GROUND, 0, POS_FLOOR_Y);
  
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
  dacPlay("fight/vs.raw");
  
  for (uint8_t i=0; i < 3; i++) {
    putImageFile(getAvatarImage(config->current_type, "atth", 1, false),
                 POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(getAvatarImage(current_enemy.current_type, "atth", 1, true),
                 POS_PLAYER2_X, POS_PLAYER2_Y);
    chThdSleepMilliseconds(200);
    putImageFile(getAvatarImage(config->current_type, "atth", 2, false),
                 POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(getAvatarImage(current_enemy.current_type, "atth", 2, true),
                 POS_PLAYER2_X, POS_PLAYER2_Y);
    chThdSleepMilliseconds(200);
  }
    
  changeState(MOVE_SELECT);
}

static void countdown_tick() {
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y,PROGRESS_BAR_W,PROGRESS_BAR_H,DEFAULT_WAIT_TIME,countdown, true, false);
  
  if (countdown <= 0) {
    orchardAppTimer(instance.context, 0, false); // shut down the timer
    screen_alert_draw(true, "TIMED OUT!");
    dacPlay("fight/select3.raw");
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
  }
}

static void state_approval_demand_tick() {
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y+10,PROGRESS_BAR_W,PROGRESS_BAR_H,DEFAULT_WAIT_TIME,countdown, true, false);
  
  if (countdown <= 0) {
    orchardAppTimer(instance.context, 0, false); // shut down the timer
    screen_alert_draw(true, "TIMED OUT!");
    dacPlay("fight/select3.raw");
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
      chprintf(stream, "we are post-move, sending turnover\r\n");
#endif
      sendGamePacket(OP_TURNOVER);
      turnover_sent = true;
    }
  }
}

static void state_show_results_enter() {
  // There is no tick() for this state; we iterate through and run an
  // animation and then exit this state.
  char c=' ';
  char attackfn1[13];
  char attackfn2[13];
  char ourdmg_s[10];
  char theirdmg_s[10];
  char tmp[35];
  
  uint16_t textx, text_p1_y, text_p2_y;
  color_t p1color, p2color;
  userconfig *config = getConfig();

  int16_t overkill_us, overkill_them;
  
  // clear status bar, remove arrows, redraw ground
  clearstatus();
  gdispFillArea(160, POS_PLAYER2_Y, 50,150,Black);
  putImageFile(IMG_GROUND, 0, POS_FLOOR_Y);

  // if they didn't go and we didn't go, abort.
  if ((ourattack <= 0) && (theirattack <= 0)) { 
    orchardAppTimer(instance.context, 0, false); // shut down the timer
    screen_alert_draw(true, "NO MOVE. TIMED OUT!");
    dacPlay("fight/select3.raw");
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
    return;
  }
  
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
    text_p2_y = 160;
  }

  if (c == ' ') 
    strcpy(attackfn1, "idla");
  else 
    chsnprintf(attackfn1, sizeof(attackfn1), "att%c", c);

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
    text_p1_y = 160;
  }
  
  if (c == ' ') 
    strcpy(attackfn2, "idla");
  else 
    chsnprintf(attackfn2, sizeof(attackfn2), "att%c", c);

  // compute damages
  // Both sides have sent damage based on stats.
  // If the hits were the same, deduct some damage

  // update the health bars
  updatehp();

  // if we didn't get a hit at all, they failed to do any damage. 
  if (last_damage == -1) last_damage = 0;

  // If hits are the same, discount the damage 20%, unless
  // someone had a crit, crits override a match
  
  p1color = Red;
  p2color = Red;

  // do we have the DEF unlock?
  if (config->unlocks & UL_PLUSDEF) {
    last_damage = (int)(last_damage * 0.9);
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream,"FIGHT: Our damage reduced due to UL_PLUSDEF now:%d\r\n", last_damage);
#endif
  }
  // do they have the DEF unlock?
  if (current_enemy.unlocks & UL_PLUSDEF) {
    last_hit = (int)(last_hit * 0.9);
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream,"FIGHT: Our Hit reduced due to UL_PLUSDEF now:%d\r\n", last_hit);
#endif
  }
  
  if ( ((theirattack & ATTACK_MASK) > 0) &&
       ((ourattack & ATTACK_MASK) > 0) &&
       ((theirattack & ATTACK_ISCRIT) == 0) &&
       ((ourattack & ATTACK_ISCRIT) == 0) &&
       (ourattack == theirattack) ) {
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream,"FIGHT: Damage reduced due to match us:%d them:%d\r\n", last_damage, last_hit);
#endif

    last_damage = (int)(last_damage * 0.8);
    last_hit    = (int)(last_hit * 0.8);

    // hits are yellow because of match
    p1color = Yellow;
    p2color = Yellow;

    // clank!
    dacPlay("fight/clank1.raw");
  } else { 
    playHit();
  }

  config->hp = config->hp - last_damage;
  if (config->hp < 0) {
    overkill_us = -config->hp;
    config->hp = 0;
  }

  current_enemy.hp = current_enemy.hp - last_hit;
  if (current_enemy.hp < 0) {
    overkill_them = -config->hp;
    current_enemy.hp = 0;
  }
  
  // animate the characters
  chsnprintf (ourdmg_s, sizeof(ourdmg_s), "-%d", last_damage );
  chsnprintf (theirdmg_s, sizeof(theirdmg_s), "-%d", last_hit );

#ifdef DEBUG_FIGHT_STATE
  chprintf(stream,"FIGHT: Damage report! Us: %d Them: %d\r\n", last_damage, last_hit);
#endif
  // 45 down. 
  // 140 | -40- | 140
  putImageFile(getAvatarImage(config->current_type, attackfn1, 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  
  putImageFile(getAvatarImage(current_enemy.current_type, attackfn2, 1, true),
               POS_PLAYER2_X, POS_PLAYER2_Y);
  chThdSleepMilliseconds(250);
  putImageFile(getAvatarImage(config->current_type, attackfn1, 2, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  
  putImageFile(getAvatarImage(current_enemy.current_type, attackfn2, 2, true),
               POS_PLAYER2_X, POS_PLAYER2_Y);
  
  if (theirattack & ATTACK_ISCRIT) { p1color = Purple; }
  if (ourattack & ATTACK_ISCRIT) { p2color = Purple; }
  
  // you attacking us
  gdispDrawStringBox (textx,text_p1_y,50,50,ourdmg_s,fontFF,p1color,justifyLeft);
  
  // us attacking you
  gdispDrawStringBox (textx+60,text_p2_y,50,50,theirdmg_s,fontFF,p2color,justifyRight);
  
  if (theirattack & ATTACK_ISCRIT || ourattack & ATTACK_ISCRIT) {
    dacPlay("fight/hit1.raw");
    chThdSleepMilliseconds(500); // sleep to finish playback -- two hit sounds = crit
  }
  
  updatehp();
  
  chThdSleepMilliseconds(500);
  gdispDrawStringBox (textx,text_p1_y,50,50,ourdmg_s,fontFF,Black,justifyLeft);
  gdispDrawStringBox (textx+60,text_p2_y,50,50,theirdmg_s,fontFF,Black,justifyRight);
  
  putImageFile(getAvatarImage(config->current_type, "idla", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  
  putImageFile(getAvatarImage(current_enemy.current_type, "idla", 1, true),
               POS_PLAYER2_X, POS_PLAYER2_Y);
  
  chThdSleepMilliseconds(100);
  
  /* if I kill you, then I will show victory and head back to the badge screen */

  // handle tie:
  if (current_enemy.hp == 0 && config->hp == 0) {
    /* both are dead, so whomever has greatest overkill wins.
     *
     * but that's the same, then the guy who started it wins.
     */

    if (overkill_us == overkill_them) {
      /* Oh, come on. Another tie? Give the win to the initiator */
      if (started_it == 1) {
        config->hp = 1;
      } else {
        current_enemy.hp = 1;
      }
    } else {
      if (overkill_us > overkill_them) {
        /* we lose */
        current_enemy.hp = 1;
      } else {
        config->hp = 1;
      }
    }
   
  }
  
  if (current_enemy.hp == 0) {
    changeState(ENEMY_DEAD);
    dacPlay("fight/drop.raw");
    putImageFile(getAvatarImage(current_enemy.current_type, "deth", 1, false),
                 POS_PLAYER2_X, POS_PLAYER2_Y);
    chThdSleepMilliseconds(250);
    putImageFile(getAvatarImage(current_enemy.current_type, "deth", 2, false),
                 POS_PLAYER2_X, POS_PLAYER2_Y);
    chThdSleepMilliseconds(250);
    chsnprintf(tmp, sizeof(tmp), "VICTORY!  (+%dXP)", calc_xp_gain(TRUE));
    screen_alert_draw(false, tmp);

    if (roundno == 1) {
      // that was TOO easy, let's tell them about it
      
      // sleep to let drop.raw finish
      chThdSleepMilliseconds(1500);
      switch(rand() % 2 +1) {
      case 1:
        dacPlay("fight/outstnd.raw");
        break;
      default:
        dacPlay("fight/flawless.raw");
        break;
      }
    } else {
      // victory!
      dacWait();
      dacPlay("yourv.raw");
    }
    
    // reward XP and exit 
    config->xp += calc_xp_gain(TRUE);
    config->won++;
    configSave(config);
    
    chThdSleepMilliseconds(ALERT_DELAY);
  } else {
    if (config->hp == 0) {
      changeState(PLAYER_DEAD);
      /* if you are dead, then you will do the same */
      dacPlay("fight/defrmix.raw");

      putImageFile(getAvatarImage(config->current_type, "deth", 1, false),
                   POS_PLAYER1_X, POS_PLAYER1_Y);
      chThdSleepMilliseconds(250);
      putImageFile(getAvatarImage(config->current_type, "deth", 2, false),
                   POS_PLAYER1_X, POS_PLAYER1_Y);
      chThdSleepMilliseconds(250);
      chsnprintf(tmp, sizeof(tmp), "YOU WERE DEFEATED (+%dXP)", calc_xp_gain(FALSE));
      screen_alert_draw(false, tmp);

      // Insult the user if they lose in the 1st round.
      if (roundno == 1) {
        chThdSleepMilliseconds(1500);
        switch(rand() % 3 + 1) {
        case 1:
          dacPlay("fight/pathtic.raw");
          break;
        case 2:
          dacPlay("fight/nvrwin.raw");
          break;
        default:
          dacPlay("fight/usuck.raw");
          break;
        }
      } else {
        // you lose, but let's be fun about it.
        dacWait();
        switch(current_enemy.current_type) {
        case p_guard:
          dacPlay("gwin.raw");
          break;
        case p_senator:
          dacPlay("swin.raw");
          break;
        case p_caesar:
          dacPlay("cwin.raw");
          break;
        case p_gladiatrix:
          dacPlay("xwin.raw");
          break;
        case p_bender:
          dacPlay("bwin.raw");
          break;
        case p_notset:
          break;
          /* bender goes here */
        }
      }
      
      // reward (some) XP and exit 
      config->xp += calc_xp_gain(FALSE);
      config->lost++;
      configSave(config);
      
      chThdSleepMilliseconds(ALERT_DELAY);
      chThdSleepMilliseconds(ALERT_DELAY);
    }
  }  

  if (config->hp == 0 || current_enemy.hp == 0) {
    // check for level UP
    if ((config->level != calc_level(config->xp)) && (config->level != LEVEL_CAP)) {
      // actual level up will be performed in-state
      changeState(LEVELUP);
      return;
    }

    // someone died, time to exit. 
    orchardAppRun(orchardAppByName("Badge"));
    return;
  }

  // no one is dead yet, go on! 
  next_fight_state = NEXTROUND;
  sendGamePacket(OP_NEXTROUND);
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
  if(xp >= 6480) {
    return 10;
  }
  if(xp >= 5440) {
    return 9;
  }
  if(xp >= 4480) {
    return 8;
  }
  if(xp >= 3600) {
    return 7;
  }
  if(xp >= 2800) {
    return 6;
  }
  if(xp >= 2080) {
    return 5;
  }
  if(xp >= 1440) {
    return 4;
  }
  if(xp >= 880) {
    return 3;
  }
  if(xp >= 400) {
    return 2;
  }
  return 1;
}

static uint16_t xp_for_level(uint8_t level) {
  // return the required amount of XP for a given level
  uint16_t xp_req[] = {
 // 1    2    3    4    5    6    7   8     9   10   
    0, 400, 880, 1440,2080,2800,3600,4480,5440, 6480
  };
  
  if ((level <= 10) && (level >= 1)) {
    return xp_req[level-1];
  } else {
    return 0;
  }
  
}

static uint16_t calc_hit(userconfig *config, user *current_enemy) {
  uint16_t basedmg;
  uint16_t basemult;
  
  // This code calculates base hit and luck only for transmitting to
  // other badge. Do not implement buffs here, implement them in
  // show_results.

  // power table (x^0.3) to avoid using pow() math library
  float exps[] = { 1.000000, 1.231144, 1.390389, 1.515717, 1.620657,
                   1.711770, 1.792790, 1.866066, 1.933182, 1.995262  };
  
  // the max damage you can do
  basemult = (25*(exps[config->level - 1]));

  // base multiplier is always rand 75% to 100%
  basemult = (rand() % basemult) + (basemult * .50);

  // caesar bonus if you are a senator! 
  if ((current_enemy->current_type == p_caesar) && (config->current_type = p_senator)) { 
    basemult *= 2;
  }

  // factor in agl/might
  basedmg = basemult + (2*config->might) - (3*current_enemy->agl);
  
  // did we crit?
  if ( (int)rand() % 100 <= config->luck ) { 
    ourattack |= ATTACK_ISCRIT;
    basedmg = basedmg * 2;
  }

  return basedmg;
  
}

static void sendAttack(void) {
  // animate the attack on our screen and send it to the the
  // challenger
  clearstatus();

  // clear middle to wipe arrow animations
  gdispFillArea(160, POS_PLAYER2_Y, 50,150,Black);
  
  if (ourattack & ATTACK_HI) {
    putImageFile("ar50.rgb",
                 160,
                 POS_PLAYER2_Y);
  }

  if (ourattack & ATTACK_MID) {
    putImageFile("ar50.rgb",
                 160,
                 POS_PLAYER2_Y + 50);
    
  }
  
  if (ourattack & ATTACK_LOW) {
    //    putImageFile(getAvatarImage(config->current_type, "attl", 1, false),
    //               POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile("ar50.rgb",
                 160,
                 POS_PLAYER2_Y + 100);
  }

  // remove the old message
  gdispFillArea (0,
                 STATUS_Y+12,
                 screen_width,
                 fontsm_height,
                 Black);
  
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
  wi.g.x = 180;
  wi.g.y = 110;
  wi.text = "5";
#ifdef notdef
  wi.g.show = TRUE;
  wi.g.width = 140;
  wi.g.height = 56;
  wi.customDraw = noRender;
  wi.customParam = 0;
  wi.customStyle = 0;
#endif
  p->ghAttackMid = gwinButtonCreate(0, &wi);
  
  // create button widget: ghAttackLow
  wi.g.x = 180;
  wi.g.y = 160;
  wi.text = "6";
#ifdef notdef
  wi.g.show = TRUE;
  wi.g.width = 140;
  wi.g.height = 56;
  wi.customDraw = noRender;
  wi.customParam = 0;
  wi.customStyle = 0;
#endif
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
  } while ( ( (enemies[ce] == NULL) || (enemies[ce]->in_combat == 1)) &&
            (distance < MAX_ENEMIES));
		   
  if (enemies[ce] != NULL) { 
    current_enemy_idx = ce;
    return TRUE;
  } else {
    // we failed, so time to die
    screen_alert_draw(true, "NO ENEMIES NEARBY!");
    dacPlay("fight/select3.raw");
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
  } while ( (
             (enemies[ce] == NULL) || (enemies[ce]->in_combat == 1)
             ) &&
             (distance < MAX_ENEMIES)
            );
		   
  if (enemies[ce] != NULL) {
    current_enemy_idx = ce;
    return TRUE;
  } else {
    // we failed, so time to die.
    screen_alert_draw(true, "NO ENEMIES NEARBY!");
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
    return FALSE;
  }
}
static void state_levelup_enter(void) {
  GWidgetInit wi;
  FightHandles *p = instance.context->priv;
  userconfig *config = getConfig();
  char tmp[34];

  chsnprintf(tmp, sizeof(tmp), "LEVEL %s", dec2romanstr(config->level+1));
  gdispClear(Black);

  putImageFile(getAvatarImage(config->current_type, "idla", 1, false),
               0,0);

  gdispDrawStringBox (0,
		      0,
                      320,
                      fontlg_height,
		      "LEVEL UP!",
		      fontLG, Yellow, justifyRight);

  gdispDrawStringBox (0,
		      fontlg_height,
                      320,
                      fontlg_height,
		      tmp,
		      fontLG, Yellow, justifyRight);
  
  gdispDrawThickLine(160,fontlg_height*2, 320, fontlg_height*2, Red, 2, FALSE);

  if (config->level+1 <= 9) {
    chsnprintf(tmp, sizeof(tmp), "NEXT LEVEL AT %d XP", xp_for_level(config->level+2));
     gdispDrawStringBox (0,
		      (fontlg_height*2) + 40,
                      320,
                      fontsm_height,
		      tmp,
		      fontSM, Pink, justifyRight);
  }  

  chsnprintf(tmp, sizeof(tmp), "MAX HP NOW %d (+%d)",
             maxhp(config->unlocks, config->level+1),
             maxhp(config->unlocks, config->level+1) - maxhp(config->unlocks, config->level));
  gdispDrawStringBox (0,
		      (fontlg_height*2) + 20,
                      320,
                      fontsm_height,
		      tmp,
		      fontSM, Pink, justifyRight);
  dacPlay("fight/leveiup.raw");

  gdispDrawStringBox (0,
		      180,
		      gdispGetWidth(),
		      gdispGetFontMetric(fontFF, fontHeight),
		      "Upgrade Available",
		      fontFF, White, justifyCenter);
  // draw UI 
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 0;
  wi.g.y = 210;
  wi.g.width = 150;
  wi.g.height = 30;
  wi.text = "AGILITY +1";
  p->ghLevelUpAgl = gwinButtonCreate(0, &wi);
  
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);
  gwinWidgetClearInit(&wi);
  wi.g.x = 170;
  wi.g.show = TRUE;
  wi.g.y = 210;
  wi.g.width = 150;
  wi.g.height = 30;
  wi.text = "MIGHT +1";
  p->ghLevelUpMight = gwinButtonCreate(0, &wi);
  // note: actual level upgrade occurs at button-push in fightEvent

  gwinSetDefaultStyle(&BlackWidgetStyle, FALSE);
}

static void state_levelup_tick(void) {
  /* no-op we will hold the levelup screen indefinately until the user
   * chooses. the user is also in-combat and cannot accept new fights
   */
}

static void state_levelup_exit(void) {
  FightHandles *p = instance.context->priv;

  if (p->ghLevelUpMight != NULL) 
    gwinDestroy (p->ghLevelUpMight);
  
  if (p->ghLevelUpAgl != NULL) 
    gwinDestroy (p->ghLevelUpAgl);

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
  drawProgressBar(35,22,100,12,
                  maxhp(config->unlocks, config->level),
                  config->hp,
                  false,
                  true);
  
  drawProgressBar(185,22,100,12,
                  maxhp(current_enemy.unlocks, current_enemy.level),
                  current_enemy.hp,
                  false,
                  false);

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
  userconfig *config = getConfig();
  char tmp[40];
  
  p = chHeapAlloc (NULL, sizeof(FightHandles));
  memset(p, 0, sizeof(FightHandles));
  context->priv = p;

  if (config->airplane_mode) {
    screen_alert_draw(true, "TURN OFF AIRPLANE MODE!");
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
    return;
  }

  last_ui_time = chVTGetSystemTime();
  orchardAppTimer(context, FRAME_INTERVAL_US, true);

  geventListenerInit(&p->glFight);
  gwinAttachListener(&p->glFight);
  geventRegisterCallback (&p->glFight, orchardAppUgfxCallback, &p->glFight);
  
  // are we entering a fight?
  chprintf(stream, "FIGHT: entering with enemy %08x state %d\r\n", current_enemy.netid, current_fight_state);
  last_tick_time = chVTGetSystemTime();

  // are we processing a grant?
  if ( (current_enemy.netid != 0) && current_enemy.opcode == OP_GRANT) {
    strcpy(tmp, "YOU RECEIVED ");
    switch (current_enemy.damage) {
    case UL_HEAL:
      strcat(tmp,"2X HEAL!");
      break;
    case UL_PLUSDEF:
      strcat(tmp,"EFF DEF.!");
      break;
    case UL_LUCK:
      strcat(tmp,"+20% LUCK!");
      config->luck = 40;
      break;
    case UL_PLUSHP:
      strcat(tmp,"+10% HP!");
      break;
    default:
      strcat(tmp,"AN UNLOCK!");
    }
    screen_alert_draw(true, tmp);
    /* merge the two */
    config->unlocks |= current_enemy.damage;
    configSave(config);
    memset(&current_enemy, 0, sizeof(current_enemy));
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
    return;
  }
  
  if (current_enemy.netid != 0 && current_fight_state != APPROVAL_DEMAND) {
    changeState(APPROVAL_DEMAND);
  }

  if (current_fight_state == IDLE) {
    if (enemyCount() > 0) {
      changeState( ENEMY_SELECT );
      dacPlay("fight/chsmix.raw");
      if (enemies[current_enemy_idx] == NULL) {
	nextEnemy();
      }
    } else {
      // punt if no enemies
      screen_alert_draw(true, "NO ENEMIES NEARBY!");
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

  dacPlay("fight/loop1.raw");
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
  chprintf(stream, "transmit ACK (to=%08x for seq %d): ourstate=%d, ttl=%d, myseq=%d, theiropcode=0x%x\r\n",
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
  chprintf(stream, "Transmit RST (to=%08x for seq %d): currentstate=%d, ttl=%d, myseq=%d, opcode=0x%x\r\n",
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
  chThdSleepMilliseconds((rand() % MAX_HOLDOFF) + 1);

#ifdef DEBUG_FIGHT_NETWORK
  chprintf(stream, "%ld Transmit (to=%08x): currentstate=%d, ttl=%d, seq=%d, opcode=0x%x\r\n", chVTGetSystemTime()  , current_enemy.netid, current_fight_state, packet.ttl, packet.seq, packet.opcode);
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
  packet.unlocks = config->unlocks;
  
  packet.hp = config->hp;
  packet.level = config->level;
  packet.current_type = config->current_type;

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

  if (event->type == keyEvent) {
    last_ui_time = chVTGetSystemTime();
    
    switch(current_fight_state) { 
    case ENEMY_SELECT:
      if ( (event->key.code == keyUp) &&     /* shhh! tell no one */
           (event->key.flags == keyPress) &&
           (config->unlocks & UL_GOD) )  {
        // remember who this is so we can issue the grant
        memcpy(&current_enemy, enemies[current_enemy_idx], sizeof(user));
        changeState(GRANT_SCREEN);
        return;
      }
      if ( (event->key.code == keyRight) &&
           (event->key.flags == keyPress) )  {
        if (nextEnemy()) {
          screen_select_draw(FALSE);
        }
        return;
      }
      if ( (event->key.code == keyLeft) &&
           (event->key.flags == keyPress) )  {
        if (prevEnemy()) {
          screen_select_draw(FALSE);
        }
        return;
      }
      if ( (event->key.code == keySelect) &&
           (event->key.flags == keyPress) )  {
        // we are attacking.
        // TODO: this code duplicates the button based start code. DRY IT UP
        dacPlay("fight/select.raw");
        started_it = 1;
        memcpy(&current_enemy, enemies[current_enemy_idx], sizeof(user));
        config->in_combat = true;
        config->lastcombat = chVTGetSystemTime();
        configSave(config);

        // We're going to preemptively call this before changing state
        // so the screen doesn't go black while we wait for an ACK.
        next_fight_state = APPROVAL_WAIT;
        screen_alert_draw(true, "Connecting...");
        sendGamePacket(OP_STARTBATTLE);
        return;
      }
      break;
    case MOVE_SELECT:
      // TODO: cleanup -This code duplicates code from the button
      // handler, but just a couple lines per button.
      if ((ourattack & ATTACK_MASK) == 0) {
        if ( (event->key.code == keyDown) &&
             (event->key.flags == keyPress) )  {
          ourattack |= ATTACK_LOW;
          sendAttack();
        }
        if ( (event->key.code == keyRight) &&
             (event->key.flags == keyPress) )  {
          ourattack |= ATTACK_MID;
          sendAttack();
        }
        if ( (event->key.code == keyUp) &&
           (event->key.flags == keyPress) )  {
          ourattack |= ATTACK_HI;
          sendAttack();
        }
      } else {
        // can't move.
        chprintf(stream, "rejecting key event -- already went\r\n");
        playNope();
      }
      break;
    case GRANT_SCREEN:
      if (event->key.flags == keyPress)  {
        switch (event->key.code) {
          /* we're going to overload the damage value here 
           * because we need to send a 16 bit number and don't want to 
           * extend the structure any further 
           */
        case keyUp:
          last_hit = UL_PLUSDEF;
          sendGamePacket(OP_GRANT);
          break;
        case keyDown:
          last_hit = UL_LUCK;
          sendGamePacket(OP_GRANT);
          break;
        case keyLeft:
          last_hit = UL_HEAL;
          sendGamePacket(OP_GRANT);
          break;
        case keyRight:
          last_hit = UL_PLUSHP;
          break;
        default:
          last_hit = 0;
          break;
        }

        if (last_hit != 0) { 
          screen_alert_draw(true, "GRANT SENT");
          dacPlay("fight/leveiup.raw");
          chThdSleepMilliseconds(ALERT_DELAY);
          orchardAppRun(orchardAppByName("Badge"));
          next_fight_state = IDLE;
          sendGamePacket(OP_GRANT);
        } else {
          screen_alert_draw(true, "CANCELLED");
          dacPlay("fight/drop.raw");
          chThdSleepMilliseconds(ALERT_DELAY);
          next_fight_state = IDLE;
          orchardAppRun(orchardAppByName("Badge"));
        }
      }
      break;
    default:
      break;
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
        chprintf(stream, "FIGHT: exitapp\r\n");
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
        dacPlay("fight/select.raw");
        started_it = 1;
        memcpy(&current_enemy, enemies[current_enemy_idx], sizeof(user));
        config->in_combat = true;
        config->lastcombat = chVTGetSystemTime();
        configSave(config);

        // We're going to preemptively call this before changing state
        // so the screen doesn't go black while we wait for an ACK.
        next_fight_state = APPROVAL_WAIT;
        screen_alert_draw(true, "Connecting...");
        sendGamePacket(OP_STARTBATTLE);
        return;
      }

      /* level up */
      if ( ((GEventGWinButton*)pe)->gwin == p->ghLevelUpMight) {
        config->might++;
        config->level++;
        config->hp = maxhp(config->unlocks, config->level); // restore hp
        configSave(config);
        dacPlay("fight/select.raw");
        screen_alert_draw(false, "Might Upgraded!");
        chThdSleepMilliseconds(ALERT_DELAY);
        orchardAppRun(orchardAppByName("Badge"));
        return;
      }
      if ( ((GEventGWinButton*)pe)->gwin == p->ghLevelUpAgl) {
        config->agl++;
        config->level++;
        config->hp = maxhp(config->unlocks, config->level); // restore hp
        configSave(config);

        screen_alert_draw(false, "Agility Upgraded!");
        dacPlay("fight/select.raw");
        chThdSleepMilliseconds(ALERT_DELAY);
        orchardAppRun(orchardAppByName("Badge"));
        return;
      }

      /* accept-deny for the being attacked page */
      if ( ((GEventGWinButton*)pe)->gwin == p->ghDeny) { 
        // "war is sweet to those who have not experienced it."
        // Pindar, 522-443 BC.
        // do our cleanups now so that the screen doesn't redraw while exiting
        orchardAppTimer(context, 0, false); // shut down the timer
        
        next_fight_state = IDLE;
        sendGamePacket(OP_DECLINED);
        screen_alert_draw(true, "Dulce bellum inexpertis.");
        dacPlay("fight/drop.raw");
        chThdSleepMilliseconds(ALERT_DELAY);
        orchardAppRun(orchardAppByName("Badge"));
        return;
      }
      if ( ((GEventGWinButton*)pe)->gwin == p->ghAccept) {
        config->in_combat = true;
        configSave(config);
        dacPlay("fight/excellnt.raw");
        dacWait();
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
        if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackLow) {
          ourattack |= ATTACK_LOW;
          sendAttack();
        }
        if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackMid) {
          ourattack |= ATTACK_MID;
          sendAttack();
        }
        if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackHi) {
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
  dacStop();
  
  chprintf(stream, "FIGHT: fight_exit\r\n");

  // don't change back to idle state from any other function. Let fight_exit take care of it.
  changeState(IDLE);
  
  geventDetachSource (&p->glFight, NULL);
  geventRegisterCallback (&p->glFight, NULL, NULL);

  chHeapFree (context->priv);
  context->priv = NULL;
  memset(&current_enemy, 0, sizeof(current_enemy));
  config->in_combat = 0;
  configSave(config);
  
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

  if (( (u->opcode == OP_STARTBATTLE) || (u->opcode == OP_GRANT) ) &&
      config->in_combat == 0) {
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
  userconfig *config = getConfig();
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
    chprintf(stream, "%08x --> RECV Invalid opcode. (seq=%d) Ignored (%08x to %08x proto %x)\r\n", u->netid, u->seq, pkt->kw01_hdr.kw01_src, pkt->kw01_hdr.kw01_dst, pkt->kw01_hdr.kw01_prot);
#endif /* DEBUG_FIGHT_NETWORK */
    return;
  }

  // DEBUG
#ifdef DEBUG_FIGHT_NETWORK
  if (u->opcode == OP_ACK) {
    chprintf(stream, "%08x --> RECV ACK (seq=%d, acknum=%d, mystate=%d, opcode 0x%x)\r\n", u->netid, u->seq, u->acknum, current_fight_state, next_fight_state, u->opcode);
  } else { 
    chprintf(stream, "%08x --> RECV OP  (proto=0x%x, seq=%d, mystate=%d, opcode 0x%x)\r\n", u->netid, pkt->kw01_hdr.kw01_prot, u->seq, current_fight_state, u->opcode);
  }
#endif /* DEBUG_FIGHT_NETWORK */
  
  if ( ((current_fight_state == IDLE) && (u->opcode >= OP_IMOVED)) && (u->opcode != OP_ACK)) {
    // this is an invalid state, which we should not ACK for.
    // Let the remote client die.
#ifdef DEBUG_FIGHT_NETWORK
    chprintf(stream, "%08x --> SEND RST: rejecting opcode 0x%x beacuse i am idle\r\n", u->netid, u->opcode);
#endif /* DEBUG_FIGHT_NETWORK */
    sendRST(u);
    // we are idle, do nothing. 
    return;
  }

  // Immediately ACK non-ACK / RST packets. We do not support retry on
  // ACK, because we support ARQ just like TCP/IP.  without the ACK,
  // the sender will retransmit automatically.
  if (u->opcode != OP_ACK && u->opcode != OP_RST) { 
      sendACK(u);
  }
    
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
      screen_alert_draw(true, "CONNECTION LOST");
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
        chprintf(stream, "%08x --> moving to state %d from state %d due to ACK\r\n",
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
      chprintf(stream, "%08x --> got turnover in wait_ack? nextstate=0x%x, currentstate=0x%x, damage=%d\r\nWe're waiting on seq %d opcode %d\r\n",
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

    if (u->opcode == OP_STARTBATTLE_ACK && next_fight_state == MOVE_SELECT) {
      // if the user hits accept _before_ the ACK has been sent by
      // their badge, we have a race condition. break the race by changing state.
      #ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "\r\n%08x --> got startbattleack in wait_ack? nextstate=0x%x, currentstate=0x%x\r\n    We're waiting on seq %d opcode %d",
               u->netid,
               next_fight_state,
               current_fight_state,
               packet.seq,
               packet.opcode);
#endif /* DEBUG_FIGHT_NETWORK */

      changeState(MOVE_SELECT);
      return;
    }
    
    if (u->opcode == OP_NEXTROUND) {
#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "%08x --> got nextround in wait_ack? nextstate=0x%x, currentstate=0x%x\r\nWe're waiting on seq %d opcode %d",
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
      screen_alert_draw(true, "DENIED.");
      dacPlay("fight/select3.raw");
      ledSetFunction(fxlist[config->led_pattern].function);
      chThdSleepMilliseconds(ALERT_DELAY);
      changeState(ENEMY_SELECT);
      orchardAppTimer(instance.context, FRAME_INTERVAL_US, true);
    }

    if (u->opcode == OP_STARTBATTLE_ACK) {
      // i am the attacker and you just ACK'd me. The timer will render this.
      changeState(VS_SCREEN);
      return;
    }

    // edge case: if I am in approval wait but you are sending me an
    // approval demand, we'll take that too.
    if (u->opcode == OP_STARTBATTLE) {
      if (u->netid == current_enemy.netid) {
#ifdef DEBUG_FIGHT_STATE
        chprintf(stream, "collision: forcing start\r\n");
#endif
        next_fight_state = VS_SCREEN;
        sendGamePacket(OP_STARTBATTLE_ACK);
        return;
      }
    }
    
    break;
    
  case MOVE_SELECT:
    if (u->opcode == OP_IMOVED) {
      // If I see OP_IMOVED from the other side while we are in
      // MOVE_SELECT, store that move for later. 
      
#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "RECV MOVE: (select) %d. our move %d.\r\n", theirattack, ourattack);
#endif /* DEBUG_FIGHT_NETWORK */

    }
    if (u->opcode == OP_TURNOVER) {
      // uh-oh, the turn is over and we haven't moved!

      next_fight_state = POST_MOVE;
#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "move_select/imoved, sending turnover\r\n");
#endif /* DEBUG_FIGHT_NETWORK */
      
      sendGamePacket(OP_TURNOVER);
    }
    break;
  case POST_MOVE: // no-op
    // if we get a turnover packet, or we're out of time, we can calc damage and show results.
    if (u->opcode == OP_IMOVED) {

#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "RECV MOVE: (postmove) %d. our move %d.\r\n", theirattack, ourattack);
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
    chprintf(stream, "%08x --> RECV %x - no handler - (seq=%d, mystate=%d %s)\r\n",
             pkt->kw01_hdr.kw01_src, u->opcode, u->seq,
             current_fight_state, fight_state_name[current_fight_state]);
#else
    chprintf(stream, "%08x --> RECV %x - no handler - (seq=%d, mystate=%d)\r\n",
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

orchard_app("Fight", "fight.rgb", APP_FLAG_AUTOINIT,
            fight_init, fight_start, fight_event, fight_exit, 1);
