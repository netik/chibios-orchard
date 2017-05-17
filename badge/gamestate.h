// the game state machine
#ifdef DEBUG_FIGHT_STATE
const char* fight_state_name[]  = {
  "NONE"  ,             // 0 - Not started yet. 
  "IDLE"  ,             // 1 - Our app isn't running or idle
  "GRANT_ACCEPT" ,      // 2 - Show the bonus screen
  "ENEMY_SELECT"  ,     // 3 - Choose enemy screen
  "GRANT_SCREEN"  ,     // 4 - Choose enemy screen
  "APPROVAL_DEMAND"  ,  // 5 - I want to fight you!
  "APPROVAL_WAIT"  ,    // 6 - I am waiting to see if you want to fight me
  "VS_SCREEN"  ,        // 7 - I am showing the versus screen.
  "MOVE_SELECT"  ,      // 8 - We are picking a move.
  "POST_MOVE"  ,        // 9 - We have picked one and are waiting on you or the clock
  "SHOW_RESULTS"  ,     // 10 - We are showing results
  "PLAYER_DEAD"  ,      // 11 - I die!
  "ENEMY_DEAD",         // 12 - You're dead.
  "LEVELUP"             // 13 - Show the bonus screen
  };
#endif /* DEBUG_FIGHT_STATE */

typedef enum _fight_state {
  NONE  ,           // 0 - Not started yet. 
  IDLE,             // 1 - Our app isn't running or idle
  GRANT_ACCEPT,     // 2 - We're getting a grant
  ENEMY_SELECT,     // 3 - Choose enemy screen
  GRANT_SCREEN,     // 4 - We're sending a grant
  APPROVAL_DEMAND,  // 5 - I want to fight you!
  APPROVAL_WAIT,    // 6 - I am waiting to see if you want to fight me
  VS_SCREEN,        // 7 - I am showing the versus screen.
  MOVE_SELECT,      // 8 - We are picking a move.
  POST_MOVE,        // 9 - We have picked one and are waiting on you or the clock
  SHOW_RESULTS,     // 10 - We are showing results, and will wait for an ACK
  PLAYER_DEAD,      // 11 - I die!
  ENEMY_DEAD,       // 12 - You're dead.
  LEVELUP,          // 13 - I am on the bonus screen, leave me alone.
} fight_state;

typedef struct _state {
  void (*enter)(void);
  void (*tick)(void);
  void (*exit)(void);
} state_funcs;

/* Network */
static void sendGamePacket(uint8_t opcode);

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

static void state_enemy_select_enter(void);
static void state_enemy_select_tick(void);
static void state_enemy_select_exit(void);

static void state_grant_accept_enter(void);

static void state_grant_enter(void);
static void state_grant_tick(void);
static void state_grant_exit(void);

static void state_approval_demand_enter(void);
static void state_approval_demand_tick(void);
static void state_approval_demand_exit(void);

static void state_approval_wait_enter(void);

static void state_vs_screen_enter(void);
static void state_vs_screen_tick(void);

static void state_move_select_enter(void);
static void state_move_select_tick(void);
static void state_move_select_exit(void);

static void state_post_move_tick(void);

static void state_show_results_enter(void);
static void state_show_results_tick(void);

static void state_levelup_enter(void);
static void state_levelup_tick(void);
static void state_levelup_exit(void);

static void draw_idle_players(void);

static uint16_t calc_xp_gain(uint8_t);

/* The function state table, a list of pointers to functions to run
 * the game Enter always fires when entering a state, tick fires on
 * the timer, exit is always fired on state exit for cleanups and
 * other housekeeping.
 *
 * When designing routines, minimize the time spent in the enter and
 * exit states and above all else, never ever put the thread to sleep
 * or we will lose radio events.
 *
 */
state_funcs fight_funcs[] = { { // none
                                NULL,
                                NULL,
                                NULL,},
                              { // idle
                                state_idle_enter,
                                NULL,
                                NULL,
                              },
                              { // grant accept
                                NULL,
                                state_grant_accept_enter,
                                NULL,
                              },
                              { // enemy select
                                state_enemy_select_enter,
                                state_enemy_select_tick,
                                state_enemy_select_exit
                              },
                              { // grant screen
                                state_grant_enter,
                                state_grant_tick,
                                state_grant_exit
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
                                state_vs_screen_tick,
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

