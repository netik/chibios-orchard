#ifndef __APP_FIGHT_H__
#define __APP_FIGHT_H__

/* debugging - this protocol can be a real pain in the ass */
#undef DEBUG_FIGHT_TICK        // show the clock tick events, ugfx events, and other misc events
#undef DEBUG_FIGHT_NETWORK    // show all network (radio) traffic
#define DEBUG_FIGHT_STATE      // debug state changes
  
// production use 66666 uS = 15 FPS. Eeeviil...
// testing use 1000000 (1 sec)
#define FRAME_INTERVAL_US 66666

// WAIT_TIMEs are always in system ticks.
// caveat! the system timer is a uint32_t and can roll over! be aware!

#define DEFAULT_WAIT_TIME MS2ST(10000) // how long we wait for the user to respond. MUST BE IN SYSTEM TICKS. 
#define MAX_ACKWAIT MS2ST(1000)         // if no ACK in 500MS, resend the last packet. MUST BE IN SYSTEM TICKS. 
#define MAX_HOLDOFF 100                // we introduce a small delay if we are resending (contention protocol). MUST BE IN mS
#define MAX_RETRIES 4                  // if we do that 3 times, abort.
#define MOVE_WAIT_TIME MS2ST(60000)    // Max game time. MUST BE IN SYSTEM TICKS. If you do nothing, the game ends.

// where the progres bar goes 
#define PROGRESS_BAR_X 60
#define PROGRESS_BAR_Y 210
#define PROGRESS_BAR_W 200
#define PROGRESS_BAR_H 10

// the maximum level you can reach
// if you change this you have to update userconfig.c
#define LEVEL_CAP      10

/* MSB of attackbitmap represents the attack */
#define ATTACK_MASK     0x38
#define ATTACK_ONESHOT  ( 1 << 7 ) 
#define ATTACK_ISCRIT   ( 1 << 6 )

#define ATTACK_HI       ( 1 << 5 )
#define ATTACK_MID      ( 1 << 4 )
#define ATTACK_LOW      ( 1 << 3 )

/* LSB of the attackbitmap -- not currently used */
#define BLOCK_MASK 0x07
#define BLOCK_HI   ( 1 << 2 )
#define BLOCK_MID  ( 1 << 1 )
#define BLOCK_LOW  ( 1 << 0 )

/* user->opcode, the packet type */
#define OP_STARTBATTLE      0x01   /* battle was requested */
#define OP_STARTBATTLE_ACK  0x02   /* battle was accepted */
#define OP_DECLINED         0x04   /* battle was declined by attackee */
#define OP_IMOVED           0x08   /* My turn is done */
#define OP_TURNOVER         0x10   /* The round is over, let's show results */
#define OP_NEXTROUND        0x11   /* Please start the next round */
#define OP_IMDEAD           0x0d   /* I died */
#define OP_YOUDIE           0x0e   /* I kill you */
#define OP_ACK              0xf0   /* Network: I got your message with sequence # acknum */
#define OP_RST              0xff   /* Network: I don't understand this message. Client should reset */

typedef struct _FightHandles {
  GListener glFight;
  GHandle ghExitButton;
  GHandle ghNextEnemy;
  GHandle ghPrevEnemy;
  GHandle ghAttack;
  GHandle ghDeny;
  GHandle ghAccept;

  GHandle ghAttackHi;
  GHandle ghAttackMid;
  GHandle ghAttackLow;

  GHandle ghLevelUpAgl;
  GHandle ghLevelUpMight;
} FightHandles;

// the game state machine

#ifdef DEBUG_FIGHT_STATE
const char* fight_state_name[]  = {
  "NONE"  ,             // 0 - Not started yet. 
  "WAITACK"  ,          // 1 - We are waiting for an ACK to transition to next_fight_state
  "IDLE"  ,             // 2 - Our app isn't running or idle
  "ENEMY_SELECT"  ,     // 3 - Choose enemy screen
  "APPROVAL_DEMAND"  ,  // 4 - I want to fight you!
  "APPROVAL_WAIT"  ,    // 5 - I am waiting to see if you want to fight me
  "VS_SCREEN"  ,        // 6 - I am showing the versus screen.
  "MOVE_SELECT"  ,      // 7 - We are picking a move.
  "POST_MOVE"  ,        // 8 - We have picked one and are waiting on you or the clock
  "SHOW_RESULTS"  ,     // 9 - We are showing results
  "NEXTROUND"  ,        // 10 - reset the round
  "PLAYER_DEAD"  ,      // 11 - I die!
  "ENEMY_DEAD",         // 12 - You're dead.
  "LEVELUP"             // 13 - Show the bonus screen
  };
#endif /* DEBUG_FIGHT_STATE */

typedef enum _fight_state {
  NONE  ,             // 0 - Not started yet. 
  WAITACK,          // 1 - We are waiting for an ACK to transition to next_fight_state
  IDLE,             // 2 - Our app isn't running or idle
  ENEMY_SELECT,     // 3 - Choose enemy screen
  APPROVAL_DEMAND,  // 4 - I want to fight you!
  APPROVAL_WAIT,    // 5 - I am waiting to see if you want to fight me
  VS_SCREEN,        // 6 - I am showing the versus screen.
  MOVE_SELECT,      // 7 - We are picking a move.
  POST_MOVE,        // 8 - We have picked one and are waiting on you or the clock
  SHOW_RESULTS,     // 9 - We are showing results
  NEXTROUND,        // 10 - reset the round
  PLAYER_DEAD,      // 11 - I die!
  ENEMY_DEAD,       // 12 - You're dead.
  LEVELUP,          // 12 - You're dead.
} fight_state;

typedef struct _state {
  void (*enter)(void);
  void (*tick)(void);
  void (*exit)(void);
} state_funcs;

extern orchard_app_instance instance;

/* Network */
static void resendPacket(void);
static void sendGamePacket(uint8_t opcode);
static void sendACK(user *inbound);
static void sendRST(user *inbound);

/* Game state */
static uint8_t calc_level(uint16_t xp);
static uint16_t xp_for_level(uint8_t level);
static void changeState(fight_state);
static void clearstatus(void);
static uint8_t prevEnemy(void);
static uint8_t nextEnemy(void);
static void updatehp(void);
  
/* Fight Sequence (OLD) */
static void screen_select_draw(int8_t);
static void draw_attack_buttons(void);
static void draw_select_buttons(void);
  
/* Fight Sequence (new)
 * 
 * I imagine most of these will be NULL soon.
 */
static void countdown_tick(void);

static void state_idle_enter(void);

static void state_waitack_enter(void);
static void state_waitack_tick(void);
static void state_waitack_exit(void);

static void state_enemy_select_enter(void);
static void state_enemy_select_tick(void);
static void state_enemy_select_exit(void);

static void state_approval_demand_enter(void);
static void state_approval_demand_tick(void);
static void state_approval_demand_exit(void);

static void state_approval_wait_enter(void);

static void state_vs_screen_enter(void);

static void state_move_select_enter(void);
static void state_move_select_tick(void);
static void state_move_select_exit(void);

static void state_post_move_tick(void);

static void state_show_results_enter(void);
static void state_show_results_tick(void);

static void state_nextround_enter(void);

static void state_levelup_enter(void);
static void state_levelup_tick(void);
static void state_levelup_exit(void);

static void radio_event_do(KW01_PKT * pkt);
static void draw_idle_players(void);

static uint16_t calc_xp_gain(uint8_t);

/* the function state table, a list of pointers to functions to run the game */
state_funcs fight_funcs[] = { { // none
                                 NULL,
                                 NULL,
                                 NULL},
                              { // waitack
                                state_waitack_enter,
                                state_waitack_tick,
                                state_waitack_exit
                              }, 
                              { // idle
                                state_idle_enter,
                                NULL,
                                NULL,
                              },
                              { // enemy select
                                state_enemy_select_enter,
                                state_enemy_select_tick,
                                state_enemy_select_exit
                              },
                              { // approval_demand
                                state_approval_demand_enter,
                                state_approval_demand_tick,
                                state_approval_demand_exit
                              },
                              {  // approval_wait
                                state_approval_wait_enter,
                                countdown_tick,
                                NULL,
                              },
                              {  // vs_screen
                                state_vs_screen_enter,
                                NULL,
                                NULL,
                              },
                              {  // move_select
                                state_move_select_enter,
                                state_move_select_tick,
                                state_move_select_exit
                              },
                              {  // post_move
                                NULL,
                                state_post_move_tick,
                                NULL,
                              },
                              {  // show_results
                                state_show_results_enter,
                                state_show_results_tick,
                                NULL
                              },
                              {  // nextround
                                state_nextround_enter,
                                NULL,
                                NULL,
                              },
                              { // player_dead
                                NULL,
                                NULL,
                                NULL
                              },
                              { // enemy dead 
                                NULL,
                                NULL,
                                NULL
                              },
                              { // levelup
                                state_levelup_enter,
                                state_levelup_tick,
                                state_levelup_exit,
                              }
};

#endif
