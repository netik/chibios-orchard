/* Ides Of DEF CON
 *
 * app-fight.c
 * John Adams <jna@retina.net>
 * 2017
 *
 * This code implements the roman fighting game. It relies on
 * app-default to handle healing and some game events, as well as
 * proto.c to transmit game data over the radio network.
 *
 */
#include <stdio.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "rand.h"

#include "orchard-app.h"
#include "led.h"

#include "proto.h"
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
#include "gamestate.h"
#include "dec2romanstr.h"

/* Globals */
static int32_t countdown = DEFAULT_WAIT_TIME; // used to hold a generic timer value. 

// if true, we started the fight. We also handle all coordination of the fight. 
static uint8_t fightleader = false;   

static fight_state current_fight_state = IDLE;  // current state 
static uint8_t current_enemy_idx = 0;
static uint32_t last_ui_time = 0;
static uint32_t last_tick_time = 0;

static uint16_t animtick = 1; 

extern struct FXENTRY fxlist[];

extern systime_t char_reset_at;

static uint32_t pending_enemy_netid = 0;
static KW01_PKT pending_enemy_pkt;
static peer current_enemy;                    // current enemy we are attacking/talking to
static RoundState rr;

/* prototypes */
static void fight_ack_callback(KW01_PKT *pkt);
static void fight_recv_callback(KW01_PKT *pkt);
static void fight_timeout_callback(KW01_PKT * pkt);
static void start_fight(OrchardAppContext *context);
static void do_timeout(void);
static void show_damage(void);
static void show_user_death(void);
static void show_opponent_death(void);
static void copyfp_topeer (uint8_t clear, fightpkt *fp, peer *p);
  
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
    if (current_enemy.current_type == p_caesar) {
      factor = factor + 1;
    }
    
    return (80 + ((config->level-1) * 16)) * factor;
  } else {
    // someone killed you, so you get much less XP, but we will give
    // some extra XP if someone higher than you killed you.
    if (factor < 1)
      factor = 1;

    return (10 + ((config->level-1) * 16)) * factor;
  }
}

static void changeState(fight_state nextstate) {
  // call previous state exit

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

  animtick = 0;
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
  p = instance.context->priv;

  // clean up the fight. This will fire a repaint that erases the area
  // under the button.  So, do not enter this state unless you are
  // ready to clear the screen and the fight is over.
  
  if (p->ghAttackHi != NULL) {
    gwinDestroy (p->ghAttackHi);
    p->ghAttackHi = NULL;
  }
  if (p->ghAttackHi != NULL) {
    gwinDestroy (p->ghAttackMid);
    p->ghAttackMid = NULL;
  }
  if (p->ghAttackHi != NULL) {
    gwinDestroy (p->ghAttackLow);
    p->ghAttackLow = NULL;
  }
  
  // reset game state
  memset(&current_enemy, 0, sizeof(current_enemy));
  memset(&pending_enemy_pkt, 0, sizeof(KW01_PKT));
  memset(&rr,0,sizeof(RoundState));

  rr.last_hit = -1;
  rr.last_damage = -1;

  config->in_combat = 0;
  pending_enemy_netid = 0;

  ledSetFunction(fxlist[config->led_pattern].function);
}

static void fight_timeout_callback(KW01_PKT *p) {
  (void)p; // don't care. dispose of this. 
  
  orchardAppTimer(instance.context, 0, false); // shut down the timer
  dacPlay("fight/select3.raw");
  screen_alert_draw(true, "LOST CONNECTION!");
  chThdSleepMilliseconds(ALERT_DELAY);
  orchardAppRun(orchardAppByName("Badge"));
  return;
}

static void state_approval_demand_enter(void) {
  GWidgetInit wi;
  FightHandles *p = instance.context->priv;
  userconfig *config = getConfig();
  int xpos, ypos;

  gdispClear(Black);

  putImageFile(getAvatarImage(current_enemy.current_type, "idla", 1, true),
                              POS_PLAYER2_X, POS_PLAYER2_Y - 20);

  gdispDrawStringBox (0,
		      0,
		      gdispGetWidth(),
		      gdispGetFontMetric(p->fontFF, fontHeight),
		      "YOU ARE BEING ATTACKED!",
		      p->fontFF, Red, justifyCenter);

  ypos = (gdispGetHeight() / 2) - 60;
  xpos = 10;

  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(p->fontLG, fontHeight),
		      current_enemy.name,
		      p->fontLG, Yellow, justifyLeft);
  ypos=ypos+50;
  chsnprintf(p->tmp, sizeof(p->tmp), "LEVEL %s", dec2romanstr(current_enemy.level));

  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(p->fontFF, fontHeight),
		      p->tmp,
		      p->fontFF, Yellow, justifyLeft);

  ypos = ypos + gdispGetFontMetric(p->fontFF, fontHeight);
  chsnprintf(p->tmp, sizeof(p->tmp), "HP %d", current_enemy.hp);
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(p->fontFF, fontHeight),
		      p->tmp,
		      p->fontFF, Yellow, justifyLeft);

  ypos = ypos + gdispGetFontMetric(p->fontFF, fontHeight) + 5;

  // hp bar
  drawProgressBar(xpos,ypos,100,10,
                  maxhp(current_enemy.current_type,
                        current_enemy.unlocks,
                        current_enemy.level),
                  current_enemy.hp, 0, false);
  gwinSetDefaultFont(p->fontFF);
  
  // draw UI
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 30;
  wi.g.y = POS_FLOOR_Y-10;
  wi.g.width = 100;
  wi.g.height = gdispGetFontMetric(p->fontFF, fontHeight) + 2;
  wi.text = "NOPE";
  p->ghDeny = gwinButtonCreate(0, &wi);
  
  gwinSetDefaultStyle(&RedButtonStyle, FALSE);

  wi.g.x = 190;
  wi.text = "FIGHT!";
  p->ghAccept = gwinButtonCreate(0, &wi);

  gwinSetDefaultStyle(&BlackWidgetStyle, FALSE);

  last_tick_time = chVTGetSystemTime();
  countdown=DEFAULT_WAIT_TIME;

  rr.roundno = 0;
  fightleader = false;
  dacStop();
  playAttacked();

  config->lastcombat = chVTGetSystemTime();
  configSave(config);
}

static void state_approval_demand_exit(void) {
  FightHandles *p = instance.context->priv;

  gwinDestroy (p->ghDeny);
  p->ghDeny = NULL;
    
  gwinDestroy (p->ghAccept);
  p->ghAccept = NULL;
}

static void state_move_select_enter() {
  userconfig *config = getConfig();
  FightHandles *p = instance.context->priv;

  animtick = 0;
  rr.ourattack = 0;
  rr.theirattack = 0;
  rr.last_damage = -1;
  rr.last_hit = -1;

  clearstatus();

  updatehp();

  putImageFile(getAvatarImage(config->current_type, "idla", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  
  putImageFile(getAvatarImage(current_enemy.current_type, "idla", 1, true),
               POS_PLAYER2_X, POS_PLAYER2_Y);

  draw_attack_buttons();

  // round N , fight!  note that we only have four of these rounds
  // pre-recorded. After that, we're just going to say 'FIGHT!'
  rr.roundno++;
  
  // remove choose attack message by overprinting in black
  gdispDrawStringBox (0,
                      STATUS_Y+14,
                      p->screen_width,
                      p->fontsm_height,
                      "Choose attack!",
                      p->fontSM, Black, justifyCenter);
  
  last_tick_time = chVTGetSystemTime();
  countdown=MOVE_WAIT_TIME;
}

static void state_move_select_tick() {
  FightHandles *p;
  p = instance.context->priv;

  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y,PROGRESS_BAR_W,PROGRESS_BAR_H,MOVE_WAIT_TIME,countdown, true, false);

  animtick++;

  /* fight sounds */
  if (animtick == 1) { 
    chsnprintf(p->tmp, sizeof(p->tmp), "fight/round%d.raw", rr.roundno);
    dacPlay(p->tmp);
  }

  if (animtick == 15) { 
    if (current_enemy.hp < (maxhp(current_enemy.current_type, current_enemy.unlocks,current_enemy.level) * .2)) {
      if (current_enemy.current_type == p_gladiatrix) { 
        dacPlay("fight/finishf.raw");  // Finish Her!
      } else {
        dacPlay("fight/finishm.raw");  // Finish Him!
      }
    } else { 
      dacPlay("fight/fight.raw");
    }
  }
  
  if ((rr.ourattack & ATTACK_MASK) == 0) {
    // animate arrows
    //
    // Every 66mS our timer fires. Wee'd like a blink rate of 250Ms on/off or so with no sleeping.
    // frames 0-4 on. frames 5-8 off
    if (animtick % 8 < 5) { 
      putImageFile("arrows.rgb",
                   160,
                   59);
    } else { 
      gdispFillArea(160, 59, 50, 140, Black);
    }

    // persist the message so the user knows what to do.
    gdispDrawStringBox (0,
                        STATUS_Y+14,
                        p->screen_width,
                        p->fontsm_height,
                        "Choose attack!",
                        p->fontSM, White, justifyCenter);
  }

  if (countdown <= 0) {
    /* TODO -- handle a move better when the other player gives
       up. right now we kill the round */
    do_timeout();
  }
}

static void state_move_select_exit() {
  //  gdispFillArea(160, POS_PLAYER2_Y, 50,150,Black);
}

static void screen_select_draw(int8_t initial) {
  // enemy selection screen
  // setting initial to TRUE will cause a repaint of the entire
  // scene. We set this to FALSE on next/previous moves to improve
  // redraw performance
  userconfig *config = getConfig();
  peer **enemies = enemiesGet();
  color_t levelcolor;
  FightHandles *p;

  p = instance.context->priv;
  
  // calculate mod/con color, WoW Style
  int leveldelta = enemies[current_enemy_idx]->level - config->level;

  // x/y cursors
  uint16_t xpos = 0; 
  uint16_t ypos = 0; 

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
  
  gdispDrawStringBox (0,
		      ypos,
		      gdispGetWidth(),
		      gdispGetFontMetric(p->fontSM, fontHeight),
		      "Select a Challenger",
		      p->fontSM, Yellow, justifyCenter);

  // slightly above the middle
  ypos = (gdispGetHeight() / 2) - 60;
  xpos = (gdispGetWidth() / 2) + 10;

  // count
  chsnprintf(p->tmp, sizeof(p->tmp), "%d of %d", current_enemy_idx + 1, enemyCount() );
  gdispDrawStringBox (xpos,
		      ypos - 20,
		      gdispGetWidth() - xpos - 30,
		      gdispGetFontMetric(p->fontXS, fontHeight),
		      p->tmp,
		      p->fontXS, White, justifyRight);
  // who 
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(p->fontFF, fontHeight),
		      enemies[current_enemy_idx]->name,
		      p->fontFF, levelcolor, justifyLeft);

  // level
  ypos = ypos + 25;
  chsnprintf(p->tmp, sizeof(p->tmp), "LEVEL %s", dec2romanstr(enemies[current_enemy_idx]->level));
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(p->fontFF, fontHeight),
		      p->tmp,
		      p->fontFF, Yellow, justifyLeft);

  ypos = ypos + 50;
  chsnprintf(p->tmp, sizeof(p->tmp), "HP %d", enemies[current_enemy_idx]->hp);
  gdispDrawStringBox (xpos,
		      ypos,
		      gdispGetWidth() - xpos,
		      gdispGetFontMetric(p->fontFF, fontHeight),
		      p->tmp,
		      p->fontFF, Yellow, justifyLeft);

  ypos = ypos + gdispGetFontMetric(p->fontFF, fontHeight) + 5;
  
  drawProgressBar(xpos,ypos,100,10,
                  maxhp(enemies[current_enemy_idx]->current_type,
                        enemies[current_enemy_idx]->unlocks,
                        enemies[current_enemy_idx]->level),
                  enemies[current_enemy_idx]->hp, 0, false);
}

static void state_enemy_select_enter(void) {
  rr.roundno = 0;
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
  FightHandles *p;
  p = instance.context->priv;
  
  gdispClear(Black);
  putImageFile("grants.rgb", 0, 0);
  
  gdispDrawStringBox (0,
		      110,
		      gdispGetWidth(),
		      gdispGetFontMetric(p->fontSM, fontHeight),
		      current_enemy.name,
		      p->fontSM, Yellow, justifyCenter);
}

static void state_grant_tick(void) {
  if ( ( (chVTGetSystemTime() - last_ui_time) > UI_IDLE_TIME ) && (rr.last_hit == -1) ) {
    orchardAppRun(orchardAppByName("Badge"));
  }

  // else, we are pending on a transmit. 
  if (countdown <= 0) {
    do_timeout();
  }
}

static void state_grant_exit(void) {
}

static void draw_idle_players() {
  userconfig *config = getConfig();
  FightHandles *p;
  p = instance.context->priv;
  
  gdispClear(Black);

  putImageFile(getAvatarImage(config->current_type, "idla", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  putImageFile(getAvatarImage(current_enemy.current_type, "idla", 1, true),
               POS_PLAYER2_X, POS_PLAYER2_Y);
 
  gdispDrawStringBox (0,
		      0,
		      p->screen_width,
		      p->fontsm_height,
		      config->name,
		      p->fontSM, Lime, justifyLeft);

  gdispDrawStringBox (0,
		      0,
		      p->screen_width,
		      p->fontsm_height,
                      current_enemy.name,
		      p->fontSM, Red, justifyRight);

  putImageFile(IMG_GROUND, 0, POS_FLOOR_Y);
  
}
  
static void state_vs_screen_enter() {
  // versus screen!
  userconfig *config = getConfig();
  FightHandles *p;
  animtick = 0;
  p = instance.context->priv;
    
  gdispClear(Black);
  draw_idle_players();
  chsnprintf(p->tmp, sizeof(p->tmp), "LEVEL %s", dec2romanstr(config->level));
  
  // levels
  gdispDrawStringBox (0,
		      STATUS_Y,
		      p->screen_width,
		      p->fontsm_height,
		      p->tmp,
		      p->fontSM, Lime, justifyLeft);

  chsnprintf(p->tmp, sizeof(p->tmp), "LEVEL %s", dec2romanstr(current_enemy.level));
  gdispDrawStringBox (0,
		      STATUS_Y,
		      p->screen_width,
		      p->fontsm_height,
                      p->tmp,
		      p->fontSM, Red, justifyRight);  
}


static void state_vs_screen_tick(void) {
  userconfig *config = getConfig();
  FightHandles *p;
  p = instance.context->priv;
  
  animtick++;

  if (animtick < 40) { 
    if (animtick % 6 < 3) { // blink duty cycle 
      gdispDrawStringBox (0,110,
                          p->screen_width, p->fontlg_height,
                          "VS",
                          p->fontLG, White, justifyCenter);
    } else {
      gdispDrawStringBox (0,110,
                          p->screen_width, p->fontlg_height,
                          "VS",
                          p->fontLG, Black, justifyCenter);
    }
  }

  if (animtick == 40) { 
    // animate characters a bit 
    gdispDrawStringBox (0,
                        STATUS_Y,
                        p->screen_width,
                        p->fontsm_height,
                        "GET READY!",
                        p->fontSM, White, justifyCenter);
    dacPlay("fight/vs.raw");
  }

  if (animtick > 40) {
    if (animtick % 4 < 2) { 
      putImageFile(getAvatarImage(config->current_type, "atth", 1, false),
                   POS_PLAYER1_X, POS_PLAYER1_Y);
      putImageFile(getAvatarImage(current_enemy.current_type, "atth", 1, true),
                   POS_PLAYER2_X, POS_PLAYER2_Y);
    } else { 
      putImageFile(getAvatarImage(config->current_type, "atth", 2, false),
                   POS_PLAYER1_X, POS_PLAYER1_Y);
      putImageFile(getAvatarImage(current_enemy.current_type, "atth", 2, true),
                   POS_PLAYER2_X, POS_PLAYER2_Y);
    }
  }

  if (animtick > 50) {
    changeState(MOVE_SELECT);
  }
}

static void do_timeout(void) {
  // A timeout occurs when someone doesn't move or the game times out.
  // not a network problem.
  orchardAppTimer(instance.context, 0, false); // shut down the timer
  screen_alert_draw(true, "TIMED OUT!");
  dacPlay("fight/select3.raw");
  chThdSleepMilliseconds(ALERT_DELAY);
  orchardAppRun(orchardAppByName("Badge"));
}

static void countdown_tick() {
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y,
                  PROGRESS_BAR_W,PROGRESS_BAR_H,
                  DEFAULT_WAIT_TIME,
                  countdown, true, false);
  
  if (countdown <= 0) {
    do_timeout();
  }
}

static void state_approval_demand_tick() {
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y+10,PROGRESS_BAR_W,PROGRESS_BAR_H,DEFAULT_WAIT_TIME,countdown, true, false);
  
  if (countdown <= 0) {
    do_timeout();
  }
}


static void state_post_move_tick() {
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y,PROGRESS_BAR_W,PROGRESS_BAR_H,MOVE_WAIT_TIME,countdown, true, false);
  if (countdown <= 0) {
    do_timeout();
  }

}

static void state_show_results_enter() {
  /* This state shows animations and results for the given round.  at
   * the end of the round, if fightmaster==true we transmit
   * OP_NEXTROUND at the end of our animations. an ACK for this packet
   * moves us back to MOVE_SELECT
   *
   * If we are not the fightmaster, we start a countdown in tick(),
   * waiting for OP_NEXTROUND to come in. 
   */
  animtick = 0;

  /* we should be out of this screen in five seconds or less. We will
   * change this timer in frame 30.
   */
  last_tick_time = chVTGetSystemTime();
  countdown = MS2ST(10000);

  // if they didn't go and we didn't go, abort, we shouldn't be here.
  if ((rr.ourattack <= 0) && (rr.theirattack <= 0)) {
    do_timeout();
    return;
  }
  
  // clear status bar, remove arrows, redraw ground
  clearstatus();
  gdispFillArea(160, POS_PLAYER2_Y, 50, 150,Black);
  putImageFile(IMG_GROUND, 0, POS_FLOOR_Y);
}

static void getDamageLoc(DmgLoc *dloc, uint8_t attack) {
  /* given an attackbitmap, return a dmgloc with a 
   * filename and screen y position to show the attack graphics
   */
  dloc->type=' ';
  
  // get the right filename for the attack
  if (attack & ATTACK_HI) {
    dloc->type='h';
    dloc->ypos = 40;
  }

  if (attack & ATTACK_MID) {
    dloc->type='m';
    dloc->ypos = 120;
  }
  
  if (attack & ATTACK_LOW) {
    dloc->type='l';
    dloc->ypos = 160;
  }

  if (dloc->type == ' ') 
    strcpy(dloc->filename, "idla");
  else 
    chsnprintf(dloc->filename, sizeof(dloc->filename), "att%c", dloc->type);

  return;
}

static void calc_damage(RoundState *r) {
  /* given the current game state calculdate the damage to both players
   * and the appropriate colors needed to show results.
   */
  userconfig *config = getConfig();
  
  // if we didn't get a hit at all, they failed to do any damage. 
  if (rr.last_damage == -1) rr.last_damage = 0;

  // The default hit color is Red.
  r->p1color = Red;
  r->p2color = Red;

  // do we have the DEF unlock?
  if (config->unlocks & UL_PLUSDEF) {
    rr.last_damage = (int)(rr.last_damage * 0.9);
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream,"FIGHT: Our damage reduced due to UL_PLUSDEF now:%d\r\n", rr.last_damage);
#endif
  }

  // do they have the DEF unlock?
  if (current_enemy.unlocks & UL_PLUSDEF) {
    rr.last_hit = (int)(rr.last_hit * 0.9);
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream,"FIGHT: Our Hit reduced due to UL_PLUSDEF now:%d\r\n", rr.last_hit);
#endif
  }
  
  // If hits are the same, discount the damage 20%, unless
  // someone had a crit, crits override a match
  
  if ( ((rr.theirattack & ATTACK_MASK) > 0) &&
       ((rr.ourattack & ATTACK_MASK) > 0) &&
       ((rr.theirattack & ATTACK_ISCRIT) == 0) &&
       ((rr.ourattack & ATTACK_ISCRIT) == 0) &&
       (rr.ourattack == rr.theirattack) ) {
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream,"FIGHT: Damage reduced due to match us:%d them:%d\r\n", rr.last_damage, rr.last_hit);
#endif
    rr.last_damage = (int)(rr.last_damage * 0.8);
    rr.last_hit    = (int)(rr.last_hit * 0.8);

    // hits are yellow because of match
    r->p1color = Yellow;
    r->p2color = Yellow;

    // we will defer sound processing to the tick() function, but remember
    // if this is a match or not here
    r->match = true;
  } else {
    r->match = false;
  }

  // However, if this is crit, we override the color.
  if (rr.theirattack & ATTACK_ISCRIT) { r->p1color = Purple; }
  if (rr.ourattack & ATTACK_ISCRIT) { r->p2color = Purple; }

  config->hp = config->hp - rr.last_damage;
  if (config->hp < 0) {
    r->overkill_us = -config->hp;
    config->hp = 0;
    r->winner = 2;
  }

  current_enemy.hp = current_enemy.hp - rr.last_hit;
  if (current_enemy.hp < 0) {
    r->overkill_them = -current_enemy.hp;
    current_enemy.hp = 0;
    r->winner = 1;
  }

  // the damage is now finally calculated, update our strings.
  chsnprintf (r->ourdmg_s, sizeof(r->ourdmg_s), "-%d", rr.last_damage );
  chsnprintf (r->theirdmg_s, sizeof(r->theirdmg_s), "-%d", rr.last_hit );

  // handle tie:
  if ((current_enemy.hp == 0) && (config->hp == 0)) {
    /* both are dead, so whomever has greatest overkill wins.
     *
     * but that's the same, then the guy who started it wins.
     */
    
    if (r->overkill_us == r->overkill_them) {
      /* Oh, come on. Another tie? Give the win to the initiator */
      if (fightleader == true) {
        config->hp = 1;
        r->winner = 1;
      } else {
        current_enemy.hp = 1;
        r->winner = 2;
      }
    } else {
      if (r->overkill_us > r->overkill_them) {
        /* we lose */
        current_enemy.hp = 1;
        r->winner = 2;
      } else {
        config->hp = 1;
        r->winner = 1;
      }
    }
  }
}

static void show_damage(void) {
  DmgLoc d;
  userconfig *config = getConfig();
  FightHandles *p;

  p = instance.context->priv;

  // update the health bars
  updatehp();

  // you attacking us
  getDamageLoc(&d, rr.ourattack);
  putImageFile(getAvatarImage(config->current_type, d.filename, 2, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  gdispDrawStringBox (100,d.ypos,
                      50,50,
                      rr.ourdmg_s,
                      p->fontFF,
                      rr.p1color,
                      justifyLeft);

  // us attacking you
  getDamageLoc(&d, rr.theirattack);  
  putImageFile(getAvatarImage(current_enemy.current_type, d.filename, 2, true),
               POS_PLAYER2_X, POS_PLAYER2_Y);
  gdispDrawStringBox (160,d.ypos,
                      50,50,
                      rr.theirdmg_s,
                      p->fontFF,
                      rr.p2color,
                      justifyRight);

  // finally 
  updatehp();
}

static void show_opponent_death() {
  FightHandles *p = instance.context->priv;
  userconfig *config = getConfig();
  
  dacPlay("fight/drop.raw");
  
  putImageFile(getAvatarImage(current_enemy.current_type, "deth", 1, false),
               POS_PLAYER2_X, POS_PLAYER2_Y);
  chThdSleepMilliseconds(250);
  putImageFile(getAvatarImage(current_enemy.current_type, "deth", 2, false),
               POS_PLAYER2_X, POS_PLAYER2_Y);
  chThdSleepMilliseconds(250);
  if (current_enemy.current_type == p_caesar) { 
    chsnprintf(p->tmp, sizeof(p->tmp), "ET TU, %s? (+%dXP)", config->name, calc_xp_gain(TRUE));
  } else {
    chsnprintf(p->tmp, sizeof(p->tmp), "VICTORY!  (+%dXP)", calc_xp_gain(TRUE));
  }
  
  screen_alert_draw(false, p->tmp);
  
  if (rr.roundno == 1) {
    // that was TOO easy, let's tell them about it
    dacWait();
    
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
}

static void show_user_death() {
  userconfig *config = getConfig();
  FightHandles *p = instance.context->priv;
  
  /* if you are dead, then you will do the same */
  dacPlay("fight/defrmix.raw");
  
  putImageFile(getAvatarImage(config->current_type, "deth", 1, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  chThdSleepMilliseconds(250);
  putImageFile(getAvatarImage(config->current_type, "deth", 2, false),
               POS_PLAYER1_X, POS_PLAYER1_Y);
  chThdSleepMilliseconds(250);
  
  if (config->current_type == p_caesar) { 
    chsnprintf(p->tmp, sizeof(p->tmp), "CAESAR HAS DIED (+%dXP)", calc_xp_gain(FALSE));
  } else { 
    chsnprintf(p->tmp, sizeof(p->tmp), "YOU WERE DEFEATED (+%dXP)", calc_xp_gain(FALSE));
  }
  
  screen_alert_draw(false, p->tmp);
  
  // Insult the user if they lose in the 1st round.
  if (rr.roundno == 1) {
    dacWait(); 
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
    case p_bender: /* KILL ALL HUMANS! */
      dacPlay("bwin.raw");
      break;
    case p_notset:
      break;
    }
  }
}

static void *vary_play(char *fn, char *prefix, int max) {
  /* given a file name and maximum index, return back a random audio
     file. Indexes begin at 1. */
  int i = rand() % max + 1;
  int l;

  l = strlen(prefix);
  strcpy(fn, prefix);
  fn[l] = (char)i + 48;
  fn[l+1] = '\0';
  strcat(fn, ".raw");

  return(fn);
}

static void state_show_results_tick() {
  userconfig *config = getConfig();
  DmgLoc d;
  FightHandles *p;

  p = instance.context->priv;
  
  // Animate the attack results. First, advance the tick counter.
  animtick++;

  // frame 0, calculate damage and populate the RoundResult structure
  // which will be used by subsequent frames
  if (animtick == 1) { 
    // calculate damage and colors
    calc_damage(&rr);
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream,"FIGHT: Damage report! Us: %d Them: %d\r\n", rr.last_damage, rr.last_hit);
#endif

    getDamageLoc(&d, rr.ourattack);
    putImageFile(getAvatarImage(config->current_type, d.filename, 1, false),
                 POS_PLAYER1_X, POS_PLAYER1_Y);
    
    getDamageLoc(&d, rr.theirattack);
    putImageFile(getAvatarImage(current_enemy.current_type, d.filename, 1, true),
                 POS_PLAYER2_X, POS_PLAYER2_Y);

    show_damage();
    
    // play one sound in frame 1, either a hard clank, soft clank, or random hit sound
    if ((rr.theirattack & ATTACK_ISCRIT) || (rr.ourattack & ATTACK_ISCRIT)) {
      // crit
      dacPlay("fight/clank2.raw");
    } else { 
      if (rr.match == true) { 
        dacPlay("fight/clank1.raw");
      } else {
        vary_play(p->tmp, "fight/hit", 3);
        dacPlay(p->tmp);
      }
    }
  }

  // "frame 2", 500 mS later reset our dudes
  if (animtick == 13) { // ~460mS or so, remove the damage indicators and reset the chars
    DmgLoc d;

    getDamageLoc(&d, rr.ourattack);
    gdispDrawStringBox (100, d.ypos,
                        50, 50,
                        rr.ourdmg_s,
                        p->fontFF,
                        Black,
                        justifyLeft);

    getDamageLoc(&d, rr.theirattack);
    gdispDrawStringBox (160, d.ypos,
                        50, 50,
                        rr.theirdmg_s,
                        p->fontFF,
                        Black,
                        justifyRight);

    putImageFile(getAvatarImage(config->current_type, "idla", 1, false),
                 POS_PLAYER1_X, POS_PLAYER1_Y);
    putImageFile(getAvatarImage(current_enemy.current_type, "idla", 1, true),
                 POS_PLAYER2_X, POS_PLAYER2_Y);
  }

  // if dying... 
  // part 4 -  animate deth1
  if (animtick == 18) { 
    if (current_enemy.hp == 0) {
      show_opponent_death();
      // reward XP and exit 
      config->xp += calc_xp_gain(TRUE);
      config->won++;
      configSave(config);
    } else {
      if (config->hp == 0) {
        show_user_death();
        // reward (some) XP and exit 
        config->xp += calc_xp_gain(FALSE);
        config->lost++;
        
        // if you were caesar, put you back!
        if (config->current_type == p_caesar) {
          config->current_type = config->p_type;
          char_reset_at = 0;
        }
        configSave(config);
      }
    }
  }
  
  // END ANIMATIONS
  if (animtick == 30) {
    if (rr.winner == 0) {
      // no one is dead yet, go on!
      if (fightleader == true) {
        countdown = MS2ST(10000);
        sendGamePacket(OP_NEXTROUND);
        // we will transition out on ACK 
      } else { 
        /* else, we'll spin, waiting in state_show_results_tick() for a next round 
         * packet. if we do not get on in ten seconds, we abort.
         */
        last_tick_time = chVTGetSystemTime();
        countdown = MS2ST(10000);
      }
    }
  }

  if (animtick == 80) { // we give some time here to let the game finish up
    // END OF GAME
    if (rr.winner != 0) { 
      // check for level UP
      if ((config->level != calc_level(config->xp)) && (config->level != LEVEL_CAP)) {
        changeState(LEVELUP);
        return;
      }
      
      // someone died, time to exit.  -- TODO WAIT FOR FIN!!
      orchardAppRun(orchardAppByName("Badge"));
      return;
    }
  }

  if (countdown <= 0) { 
    orchardAppTimer(instance.context, 0, false); // shut down the timer
    screen_alert_draw(true, "NO NEXTROUND PKT?");
    dacPlay("fight/select3.raw");
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
  }
}
//--------------------- END state funcs

static void clearstatus(void) {
  // clear instruction area
  FightHandles *p;

  p = instance.context->priv;

  gdispFillArea(0,
		STATUS_Y,
		gdispGetWidth(),
		p->fontsm_height,
		Black);

  // clear the area where 'KO' is, if any
  gdispFillArea(140,
		15,
		40,
		30,
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

static uint16_t calc_hit(userconfig *config, peer *current_enemy) {
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
  if ((current_enemy->current_type == p_caesar) && (config->current_type == p_senator)) { 
    basemult *= 2;
  }

  // factor in agl/might
  basedmg = basemult + (2*config->might) - (3*current_enemy->agl);
  
  // did we crit?
  if ( (int)rand() % 100 <= config->luck ) { 
    rr.ourattack |= ATTACK_ISCRIT;
    basedmg = basedmg * 2;
  }

  return basedmg;
  
}

static void sendAttack(void) {
  // animate the attack on our screen and send it to the the
  // challenger
  FightHandles *p;
  p = instance.context->priv;

  clearstatus();

  // clear middle to wipe arrow animations
  gdispFillArea(160, POS_PLAYER2_Y, 50,150,Black);
  
  if (rr.ourattack & ATTACK_HI) {
    putImageFile("ar50.rgb",
                 160,
                 POS_PLAYER2_Y);
  }

  if (rr.ourattack & ATTACK_MID) {
    putImageFile("ar50.rgb",
                 160,
                 POS_PLAYER2_Y + 50);
    
  }
  
  if (rr.ourattack & ATTACK_LOW) {
    putImageFile("ar50.rgb",
                 160,
                 POS_PLAYER2_Y + 100);
  }

  // remove the old message
  gdispFillArea (0,
                 STATUS_Y+12,
                 p->screen_width,
                 p->fontsm_height,
                 Black);
  
  gdispDrawStringBox (0,
                      STATUS_Y,
                      gdispGetWidth(),
                      gdispGetFontMetric(p->fontSM, fontHeight),
                      "WAITING FOR CHALLENGER'S MOVE",
                      p->fontSM, White, justifyCenter);

  sendGamePacket(OP_IMOVED);
  changeState(POST_MOVE);
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
  p->ghAttackMid = gwinButtonCreate(0, &wi);
  
  // create button widget: ghAttackLow
  wi.g.x = 180;
  wi.g.y = 160;
  wi.text = "6";
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
  gwinSetDefaultFont(p->fontFF);
  gwinWidgetClearInit(&wi);
  wi.g.show = TRUE;
  wi.g.x = 85;
  wi.g.y = POS_FLOOR_Y+10;
  wi.g.width = 150;
  wi.g.height = gdispGetFontMetric(p->fontFF, fontHeight) + 2;
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
  peer **enemies = enemiesGet();
  int8_t ce = current_enemy_idx;
  uint8_t distance = 0;
  
  do { 
    ce++;
    distance++;
    if (ce > MAX_ENEMIES-1) {
      ce = 0;
    }
  } while ( ( (enemies[ce] == NULL) ||
              (enemies[ce]->in_combat == 1) ||
              (enemies[ce]->p_type == p_notset)) &&
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
  peer **enemies = enemiesGet();
  int8_t ce = current_enemy_idx;
  uint8_t distance = 0;
  
  do { 
    ce--;
    distance++;
    if (ce < 0) {
      ce = MAX_ENEMIES-1;
    }
  } while ( (
             (enemies[ce] == NULL) ||
             (enemies[ce]->in_combat == 1) ||
             (enemies[ce]->p_type == p_notset)
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

  if (p->ghAttackHi != NULL) {
    gwinDestroy (p->ghAttackHi);
    p->ghAttackHi = NULL;
  }
  if (p->ghAttackHi != NULL) {
    gwinDestroy (p->ghAttackMid);
    p->ghAttackMid = NULL;
  }
  if (p->ghAttackHi != NULL) {
    gwinDestroy (p->ghAttackLow);
    p->ghAttackLow = NULL;
  }

  gdispClear(Black);

  chsnprintf(p->tmp, sizeof(p->tmp), "LEVEL %s", dec2romanstr(config->level+1));

  putImageFile(getAvatarImage(config->p_type, "idla", 1, false),
               0,0);

  gdispDrawStringBox (0,
		      0,
                      320,
                      p->fontlg_height,
		      "LEVEL UP!",
		      p->fontLG, Yellow, justifyRight);

  gdispDrawStringBox (0,
		      p->fontlg_height,
                      320,
                      p->fontlg_height,
		      p->tmp,
		      p->fontLG, Yellow, justifyRight);
  
  gdispDrawThickLine(160,p->fontlg_height*2,
                     320, p->fontlg_height*2,
                     Red, 2, FALSE);

  if (config->level+1 <= 9) {
    chsnprintf(p->tmp, sizeof(p->tmp), "NEXT LEVEL AT %d XP",
               xp_for_level(config->level+2));
    
    gdispDrawStringBox (0,
                        (p->fontlg_height*2) + 40,
                        320,
                        p->fontsm_height,
                        p->tmp,
                        p->fontSM, Pink, justifyRight);
  }  
  
  chsnprintf(p->tmp, sizeof(p->tmp), "MAX HP NOW %d (+%d)",
             maxhp(config->p_type, config->unlocks, config->level+1),
             maxhp(config->p_type, config->unlocks, config->level+1) -
             maxhp(config->current_type, config->unlocks, config->level));
  
  gdispDrawStringBox (0,
		      (p->fontlg_height*2) + 20,
                      320,
                      p->fontsm_height,
		      p->tmp,
		      p->fontSM, Pink, justifyRight);

  dacPlay("fight/leveiup.raw");

  gdispDrawStringBox (0,
		      180,
		      gdispGetWidth(),
		      gdispGetFontMetric(p->fontFF, fontHeight),
		      "Upgrade Available",
		      p->fontFF, White, justifyCenter);
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
  FightHandles *p = instance.context->priv;
  
  // center KO 
  gdispDrawStringBox (1, 16, gdispGetWidth()-1,
		      gdispGetFontMetric(p->fontSM, fontHeight),
		      "KO",
		      p->fontSM, Yellow, justifyCenter);

  gdispDrawStringBox (0, 15, gdispGetWidth(),
		      gdispGetFontMetric(p->fontSM, fontHeight),
		      "KO",
		      p->fontSM, Red, justifyCenter);
  // hp bars
  drawProgressBar(35,22,100,12,
                  maxhp(config->current_type, config->unlocks, config->level),
                  config->hp,
                  false,
                  true);
  
  drawProgressBar(185,22,100,12,
                  maxhp(current_enemy.current_type, current_enemy.unlocks, current_enemy.level),
                  current_enemy.hp,
                  false,
                  false);

  // numeric hp indicators
  gdispFillArea( 0, 23,
                 35, gdispGetFontMetric(p->fontXS, fontHeight),
                 Black );

  gdispFillArea( 289, 23,
                 30, gdispGetFontMetric(p->fontXS, fontHeight),
                 Black );

  chsnprintf(p->tmp, sizeof(p->tmp), "%d", config->hp);
  gdispDrawStringBox (0,
		      23,
		      35,
		      gdispGetFontMetric(p->fontXS, fontHeight),
		      p->tmp,
		      p->fontXS, White, justifyRight);

  chsnprintf(p->tmp, sizeof(p->tmp), "%d", current_enemy.hp);

  gdispDrawStringBox (289,
		      23,
		      29,
		      gdispGetFontMetric(p->fontXS, fontHeight),
		      p->tmp,
		      p->fontXS, White, justifyLeft);
  
}

static void fight_start(OrchardAppContext *context) {
  FightHandles *p = context->priv;
  peer **enemies = enemiesGet();
  userconfig *config = getConfig();
  
  // gtfo if in airplane mode.
  if (config->airplane_mode) {
    screen_alert_draw(true, "TURN OFF AIRPLANE MODE!");
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
    return;
  }

  // allocate our context
  p = chHeapAlloc (NULL, sizeof(FightHandles));  
  memset(p, 0, sizeof(FightHandles));
  p->proto = chHeapAlloc (NULL, sizeof(ProtoHandles));
  memset(p->proto, 0, sizeof(ProtoHandles));
  context->priv = p;

  // init fonts, font metrics and store some data about the viewport
  p->fontSM = gdispOpenFont(FONT_SM);
  p->fontLG = gdispOpenFont(FONT_LG);
  p->fontXS = gdispOpenFont (FONT_XS);
  p->fontFF = gdispOpenFont (FONT_FIXED);
  p->screen_width = gdispGetWidth();
  p->screen_height = gdispGetHeight();
  p->fontsm_height = gdispGetFontMetric(p->fontSM, fontHeight);
  p->fontlg_height = gdispGetFontMetric(p->fontLG, fontHeight);

  protoInit(context);

  /* set callbacks */
  p->proto->cb_timeout = fight_timeout_callback;
  p->proto->cb_recv = fight_recv_callback;
  p->proto->cb_ack = fight_ack_callback;

  /* set packet size */
  p->proto->mtu = sizeof(fightpkt);
  
  last_ui_time = chVTGetSystemTime();
  orchardAppTimer(context, FRAME_INTERVAL_US, true);

  /* enable our listeners for ugfx */
  geventListenerInit(&p->glFight);
  gwinAttachListener(&p->glFight);
  geventRegisterCallback (&p->glFight, orchardAppUgfxCallback, &p->glFight);
  
  /* are we entering a fight? */
#ifdef DEBUG_FIGHT_STATE
  chprintf(stream, "FIGHT: entering with enemy %08x state %d\r\n", current_enemy.netid, current_fight_state);
  chprintf(stream, "pending enemy id %x\r\n", pending_enemy_netid);      
#endif
  last_tick_time = chVTGetSystemTime();
  
  if (pending_enemy_netid != 0 && current_fight_state != APPROVAL_DEMAND) {
    /* if a packet has come in before we launched, we will process it
       like normal, and let the event handler take care of it */

    /* tell the RX handler we care about this netid */
    p->proto->netid = pending_enemy_netid;
    rxHandle(context, &pending_enemy_pkt);
    return;
  }

  // we are IDLE, so show the select screen or abort.
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
  FightHandles *p;
  p = instance.context->priv;

  draw_idle_players();

  // progress bar
  last_tick_time = chVTGetSystemTime(); 
  countdown = DEFAULT_WAIT_TIME; // used to hold a generic timer value.
  drawProgressBar(PROGRESS_BAR_X,PROGRESS_BAR_Y,PROGRESS_BAR_W,PROGRESS_BAR_H,DEFAULT_WAIT_TIME,countdown, true, false);

  gdispDrawStringBox (0,
                      STATUS_Y,
                      p->screen_width,
                      p->fontsm_height,
                      "Waiting for enemy to accept!",
                      p->fontSM, White, justifyCenter);
  dacPlay("fight/loop1.raw");
}

static void sendGamePacket(uint8_t opcode) {
  // sends a game packet to current_enemy
  userconfig *config;
  OrchardAppContext *context = instance.context;
  FightHandles *p = instance.context->priv;
  fightpkt packet;
  int res;
  
  config = getConfig();
  memset (&packet, 0, sizeof(fightpkt));

  p->proto->netid = current_enemy.netid;
  packet.opcode = opcode;
  strncpy(packet.name, config->name, CONFIG_NAME_MAXLEN);
  packet.in_combat = config->in_combat;
  packet.unlocks = config->unlocks;
  packet.hp = config->hp;
  packet.level = config->level;
  packet.current_type = config->current_type;
  packet.won = config->won;
  packet.lost = config->lost;
  packet.xp = config->xp;
  packet.agl = config->agl;
  packet.luck = config->luck;
  packet.might = config->might;

  /* we'll always send -1, unless the user has made a move, then we'll calculate the hit. */
  if (rr.last_hit == -1 && rr.ourattack != 0) {
    rr.last_hit = calc_hit(config, &current_enemy);
  }

  packet.damage = rr.last_hit;
  packet.attack_bitmap = rr.ourattack;

#ifdef DEBUG_FIGHT_NETWORK
  chprintf(stream, "%ld Transmit (to=%08x): currentstate=%d %s, opcode=0x%x, size=%d\r\n", chVTGetSystemTime()  , current_enemy.netid, current_fight_state, fight_state_name[current_fight_state],  packet.opcode,sizeof(packet));
#endif /* DEBUG_FIGHT_NETWORK */
  res = msgSend(context, &packet);

  if (res == -1) {
    chprintf(stream, "transmit fail. packet too big / xmit busy?\r\n");
  }

}

static void start_fight(OrchardAppContext *context) {
  FightHandles * p = context->priv;
  userconfig *config = getConfig();
  peer **enemies = enemiesGet();

  // hide the buttons so they can't be hit again
  gwinHide (p->ghAttack);
  gwinHide (p->ghExitButton);
  gwinHide (p->ghNextEnemy);
  gwinHide (p->ghPrevEnemy);
  
  dacPlay("fight/select.raw");
  fightleader = true;
  memcpy(&current_enemy, enemies[current_enemy_idx], sizeof(peer));
  config->in_combat = true;
  config->lastcombat = chVTGetSystemTime();
  configSave(config);
  
  // We're going to preemptively call this before changing state
  // so the screen doesn't go black while we wait for an ACK.
  screen_alert_draw(true, "Connecting...");
  sendGamePacket(OP_BATTLE_REQ);
}

static void fight_event(OrchardAppContext *context,
			const OrchardAppEvent *event) {

  FightHandles * p = context->priv;
  GEvent * pe;
  peer **enemies = enemiesGet();
  userconfig *config = getConfig();

  if (event->type == radioEvent && event->radio.pPkt != NULL) {
    rxHandle(context, event->radio.pPkt);
    return;
  }
  
  // the timerEvent fires on the frame interval. It will always call
  // the current state's tick() method. tick() is expected to handle
  // animations or otherwise.
  
  if (event->type == timerEvent) {
#ifdef DEBUG_FIGHT_TICK
    chprintf(stream, "tick @ %d mystate is %d, countdown is %d, last tick %d, rr.ourattack %d, rr.theirattack %d, delta %d\r\n",
             chVTGetSystemTime(),
             current_fight_state,
             countdown,
             last_tick_time,
             rr.ourattack,
             rr.theirattack,
             (chVTGetSystemTime() - last_tick_time));
#endif /* DEBUG_FIGHT_TICK */

    if (fight_funcs[current_fight_state].tick != NULL) // some states lack a tick call
      fight_funcs[current_fight_state].tick();

    tickHandle(context);
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
        memcpy(&current_enemy, enemies[current_enemy_idx], sizeof(peer));
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
        start_fight(context);
        return;
      }
      break;
    case MOVE_SELECT:
      // TODO: cleanup -This code duplicates code from the button
      // handler, but just a couple lines per button.
      if ((rr.ourattack & ATTACK_MASK) == 0) {
        if ( (event->key.code == keyDown) &&
             (event->key.flags == keyPress) )  {
          rr.ourattack |= ATTACK_LOW;
          sendAttack();
        }
        if ( (event->key.code == keyRight) &&
             (event->key.flags == keyPress) )  {
          rr.ourattack |= ATTACK_MID;
          sendAttack();
        }
        if ( (event->key.code == keyUp) &&
           (event->key.flags == keyPress) )  {
          rr.ourattack |= ATTACK_HI;
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
          rr.last_hit = UL_PLUSDEF;
          sendGamePacket(OP_GRANT);
          break;
        case keyDown:
          rr.last_hit = UL_LUCK;
          sendGamePacket(OP_GRANT);
          break;
        case keyLeft:
          rr.last_hit = UL_HEAL;
          sendGamePacket(OP_GRANT);
          break;
        case keyRight:
          rr.last_hit = UL_PLUSHP;
          sendGamePacket(OP_GRANT);
          break;
        default:
          rr.last_hit = 0;
          break;
        }

        if (rr.last_hit != 0) { 
          screen_alert_draw(true, "SENDING...");
          dacPlay("fight/buff.raw");
          last_tick_time = chVTGetSystemTime();
          countdown=DEFAULT_WAIT_TIME;
          sendGamePacket(OP_GRANT);
        } else {
          screen_alert_draw(true, "CANCELLED");
          dacPlay("fight/drop.raw");
          chThdSleepMilliseconds(ALERT_DELAY);
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
#ifdef DEBUG_FIGHT_STATE        
        chprintf(stream, "FIGHT: exitapp\r\n");
#endif
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
        start_fight(context);
        return;
      }

      /* level up */
      if ( ((GEventGWinButton*)pe)->gwin == p->ghLevelUpMight) {
        config->might++;
        config->level++;

        if (config->current_type != p_caesar)
          config->hp = maxhp(config->current_type, config->unlocks, config->level); // restore hp
        
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

        if (config->current_type != p_caesar)
          config->hp = maxhp(config->current_type, config->unlocks, config->level); // restore hp

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
        gwinDestroy (p->ghDeny);
        p->ghDeny = NULL;
        gwinDestroy (p->ghAccept);
        p->ghAccept = NULL;
        
        sendGamePacket(OP_BATTLE_DECLINED);
        screen_alert_draw(true, "Dulce bellum inexpertis.");
        dacPlay("fight/drop.raw");
        chThdSleepMilliseconds(ALERT_DELAY);
        orchardAppRun(orchardAppByName("Badge"));
        return;
      }
      if ( ((GEventGWinButton*)pe)->gwin == p->ghAccept) {
        gwinHide(p->ghDeny);
        gwinHide(p->ghAccept);

        config->in_combat = true;
        configSave(config);
        screen_alert_draw(false, "SYNCING...");
        dacPlay("fight/excellnt.raw");
        dacWait();
        sendGamePacket(OP_BATTLE_GO);
        return;
      }
      
      if ( ((GEventGWinButton*)pe)->gwin == p->ghPrevEnemy) { 
        if (prevEnemy()) {
          screen_select_draw(FALSE);
        }
        return;
      }

      if ((current_fight_state == POST_MOVE) || (current_fight_state == MOVE_SELECT)) { 
        if ((rr.ourattack & ATTACK_MASK) == 0) {
          if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackLow) {
            rr.ourattack |= ATTACK_LOW;
            sendAttack();
          }
          if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackMid) {
            rr.ourattack |= ATTACK_MID;
            sendAttack();
          }
          if ( ((GEventGWinButton*)pe)->gwin == p->ghAttackHi) {
            rr.ourattack |= ATTACK_HI;
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
}

static void fight_exit(OrchardAppContext *context) {
  FightHandles * p;
  userconfig *config = getConfig();
  p = context->priv;
  dacStop();

#ifdef DEBUG_FIGHT_STATE
  chprintf(stream, "FIGHT: fight_exit\r\n");
#endif
  
  // don't change back to idle state from any other function. Let fight_exit take care of it.
  changeState(IDLE);
    
  gdispCloseFont(p->fontLG);
  gdispCloseFont(p->fontSM);
  gdispCloseFont(p->fontFF);
  gdispCloseFont(p->fontXS);

  geventDetachSource (&p->glFight, NULL);
  geventRegisterCallback (&p->glFight, NULL, NULL);

  chHeapFree (((FightHandles *)context->priv)->proto);
  chHeapFree (context->priv);
  context->priv = NULL;
  memset(&current_enemy, 0, sizeof(current_enemy));

  config->in_combat = 0;
  configSave(config);
  return;
}

static void fightRadioEventHandler(KW01_PKT * pkt) {
  fightpkt *fp;
  userconfig *config = getConfig();
  PACKET * proto;
  
  /* this code runs in the MAIN thread, it runs along side our thread */
  /* everything else runs in the app's thread */

  /* peek inside the packet and see if it's bound for us */
  proto = (PACKET *)&pkt->kw01_payload;
  fp = (fightpkt *)proto->prot_payload;

  /* There are a few apps which we will not respond to events during,
   * because they consume the cpu. Ignore the radio requests when
   * these apps are running.
   */

  if ((instance.app == orchardAppByName("Play Videos")) ||
      (instance.app == orchardAppByName("LED Sign")) ||
      (instance.app == orchardAppByName("Credits")) ||
      (instance.app == orchardAppByName("E-mail"))) {
    return;
  }

  if (pkt->kw01_hdr.kw01_prot == RADIO_PROTOCOL_FIGHT) {   
    if (( (fp->opcode == OP_BATTLE_REQ) || (fp->opcode == OP_GRANT) ) &&
        config->in_combat == 0) {
      // stash this packet away for replay.
      pending_enemy_netid = pkt->kw01_hdr.kw01_src;  
      memcpy(&pending_enemy_pkt, proto, sizeof(KW01_PKT));

      if (instance.app != orchardAppByName("Fight")) {
        // not in app - switch over. 
        orchardAppRun(orchardAppByName("Fight"));
      } else {
        FightHandles *p = instance.context->priv;
        // set the netid so we can handle this immediately
        p->proto->netid = pkt->kw01_hdr.kw01_src;
      }

    }
    orchardAppRadioCallback (pkt);
  }
  
  return;
}

static void fight_ack_callback(KW01_PKT *pkt) {
  /* this is called when one of our packets is ACK'd state changes, if
   * they are based on the network should only happen here !
   */
  fightpkt u;
  PACKET * proto; 
  proto = (PACKET *)&pkt->kw01_payload;
  
  /* unpack the packet */
  memcpy(&u, proto->prot_payload, sizeof(fightpkt));

  /* what kind of packet was ACK'd? */
  switch(u.opcode) {
  case OP_GRANT:
    screen_alert_draw(true, "SENT.");
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
    break;
  case OP_BATTLE_REQ:
    // we are ack'd, let's go.
    if (current_fight_state == ENEMY_SELECT) { 
      changeState(APPROVAL_WAIT);
    }
    break;
  case OP_BATTLE_GO_ACK:
    if (current_fight_state == APPROVAL_WAIT) { 
      changeState(VS_SCREEN);
    }
    break;
  case OP_IMOVED:
    if (current_fight_state == POST_MOVE && rr.last_damage != -1) { 
      // if you ACK my move, and I am in POST_MOVE and I have your
      // move, we can show results
      changeState(SHOW_RESULTS);
    }
    break;
  case OP_NEXTROUND:
    if (current_fight_state == SHOW_RESULTS) {
      changeState(MOVE_SELECT);
    }
    break;
  }

}  

static void copyfp_topeer(uint8_t clear, fightpkt *fp, peer *p) {
  /* copy (or update) the peer based on a fightpkt 
   * does not copy rtc, ttl, attack bitmap, the netid, or damage
   */
  if (clear)
    memset(p, 0, sizeof(peer));
  
  strncpy(p->name, fp->name, CONFIG_NAME_MAXLEN);
  p->p_type = fp->p_type;
  p->current_type = fp->current_type;
  p->in_combat = fp->in_combat;
  p->unlocks = fp->unlocks;
  p->hp = fp->hp;
  p->xp = fp->xp;
  p->level = fp->level;
  p->agl = fp->agl;
  p->might = fp->might;
  p->luck = fp->luck;
  p->won = fp->won;
  p->lost = fp->lost;
}      

static void fight_recv_callback(KW01_PKT *pkt)
{
  /* this is called by our protocol code when we receive a packet */
  fightpkt u;
  PACKET * proto; 
  proto = (PACKET *)&pkt->kw01_payload;
  char tmp[40];
  userconfig *config = getConfig();
  
  /* unpack the packet */
  memcpy(&u, proto->prot_payload, sizeof(fightpkt));
  
  /* does this payload contain damage information? */
  if (u.damage != -1) {
    rr.theirattack = u.attack_bitmap;
    rr.last_damage = u.damage;
  }
  
  // DEBUG
#ifdef DEBUG_FIGHT_NETWORK
  chprintf(stream, "%08x %d --> RECV OP  (mystate=%d %s, opcode 0x%x)\r\n", pkt->kw01_hdr.kw01_src, proto->prot_seq, current_fight_state, fight_state_name[current_fight_state],u.opcode);
#endif /* DEBUG_FIGHT_NETWORK */
  
  /* act upon the packet */
  switch (current_fight_state) {
  case IDLE:
  case ENEMY_SELECT:
    // handle grants
    if (u.opcode == OP_GRANT) { 
      strcpy(tmp, "YOU RECEIVED ");
      switch (u.damage) {
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

      config->unlocks |= u.damage;
      dacPlay("fight/buff.raw");
      screen_alert_draw(true, tmp);

      /* merge the two */
      configSave(config);
      memset(&current_enemy, 0, sizeof(current_enemy));
      pending_enemy_netid = 0;
      chThdSleepMilliseconds(ALERT_DELAY);
      orchardAppRun(orchardAppByName("Badge"));
      return;
    }
    
    if (u.opcode == OP_BATTLE_REQ) {
      /* copy the data over to current_enemy */
      copyfp_topeer(true, &u, &current_enemy);
      current_enemy.netid = pending_enemy_netid;

      rr.ourattack = 0;
      rr.theirattack = 0;

      changeState(APPROVAL_DEMAND);
      return;
    }
    break;
  case APPROVAL_WAIT:
    if (u.opcode == OP_BATTLE_GO) {
      copyfp_topeer(false, &u, &current_enemy);
      sendGamePacket(OP_BATTLE_GO_ACK);
    }
    if (u.opcode == OP_BATTLE_DECLINED) {
      orchardAppTimer(instance.context, 0, false);
      screen_alert_draw(true, "DENIED.");
      dacPlay("fight/select3.raw");
      ledSetFunction(fxlist[config->led_pattern].function);
      chThdSleepMilliseconds(ALERT_DELAY);
      orchardAppRun(orchardAppByName("Badge"));
      return;
    }
    break;
  case APPROVAL_DEMAND:
    if (u.opcode == OP_BATTLE_GO_ACK) {
      copyfp_topeer(false, &u, &current_enemy);
      changeState(VS_SCREEN);
    }
    break;
  case MOVE_SELECT:
    if (u.opcode == OP_IMOVED) {
      // If I see OP_IMOVED from the other side while we are in
      // MOVE_SELECT, store that move for later. 
#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "RECV MOVE: (select) %d. our move %d.\r\n", rr.theirattack, rr.ourattack);
#endif /* DEBUG_FIGHT_NETWORK */
      // make sure we're in absolute agreement on hp by copying the data again.
      copyfp_topeer(false, &u, &current_enemy);
    }
    break;
  case POST_MOVE: // no-op
    if (u.opcode == OP_IMOVED) {
#ifdef DEBUG_FIGHT_NETWORK
      chprintf(stream, "RECV MOVE: (postmove) %d. our move %d. - TURNOVER!\r\n", rr.theirattack, rr.ourattack);
#endif /* DEBUG_FIGHT_NETWORK */
      changeState(SHOW_RESULTS);
      // make sure we're in absolute agreement on hp by copying the data again.
      copyfp_topeer(false, &u, &current_enemy);
      return;
    }
    break;
  case SHOW_RESULTS:
    if ((u.opcode == OP_NEXTROUND) && (fightleader == false)) {
      changeState(MOVE_SELECT);
      return;
    }
    break;

  default:
    // this shouldn't fire, but log it if it does.
#ifdef DEBUG_FIGHT_STATE
    chprintf(stream, "%08x %d --> RECV %x - no handler (mystate=%d %s)\r\n",
             pkt->kw01_hdr.kw01_src, proto->prot_seq, u.opcode, 
             current_fight_state, fight_state_name[current_fight_state]);
#else
    chprintf(stream, "%08x %d --> RECV %x - no handler (mystate=%d)\r\n",
             pkt->kw01_hdr.kw01_src, proto->prot_seq, u.opcode, 
             current_fight_state);
#endif
  }
}

static uint32_t fight_init(OrchardAppContext *context) {
  userconfig *config = getConfig();

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
