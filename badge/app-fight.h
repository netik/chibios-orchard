#ifndef __APP_FIGHT_H__
#define __APP_FIGHT_H__
  
// production use 66666 uS = 15 FPS. Eeeviil...
// testing use 1000000 (1 sec)
#define FRAME_INTERVAL_US 66666

// time to wait for a response from either side (mS)
// WAIT_TIMEs are always in system ticks.
// caveat! the system timer is a uint32_t and can roll over! be aware!

#define DEFAULT_WAIT_TIME MS2ST(20000)
#define MAX_ACKWAIT MS2ST(1000)         // if no ACK in 500MS, resend
#define MAX_RETRIES 3                   // if we try 3 times, abort. 
#define MOVE_WAIT_TIME MS2ST(60000)     // Both of you have 60 seconds to decide. If you do nothing, the game ends. 
#define ALERT_DELAY 1500               // how long alerts stay on the screen.

/* attack profile (attack_bitmap) - high order bits */
#define ATTACK_MASK     0x38
#define ATTACK_ONESHOT  ( 1 << 7 ) 
#define ATTACK_ISCRIT   ( 1 << 6 )

#define ATTACK_HI       ( 1 << 5 )
#define ATTACK_MID      ( 1 << 4 )
#define ATTACK_LOW      ( 1 << 3 )

/* unused in this version */
#define BLOCK_MASK 0x07
#define BLOCK_HI   ( 1 << 2 )
#define BLOCK_MID  ( 1 << 1 )
#define BLOCK_LOW  ( 1 << 0 )

/* user->opcode, the packet type */
#define OP_STARTBATTLE      0x01   /* battle was requested */
#define OP_STARTBATTLE_ACK  0x02   /* battle was accepted */
#define OP_DECLINED         0x04   /* battle was declined or invalid */
#define OP_IMOVED           0x08   /* My turn is done */
#define OP_TURNOVER         0x10   /* The round is over */
#define OP_NEXTROUND        0x11   /* Please start the next round */
#define OP_IMDEAD           0x0d   /* I died */
#define OP_ACK              0xf0   /* Network: I got your message with sequence acknum */
#define OP_RST              0xff   /* Network: I don't understand this message. Client should reset */

// the game state machine
typedef enum _fight_state {
  NONE,             // 0 - Not started yet. 
  WAITACK,          // 1 - We are waiting for an ACK to transition to next_fight_state
  IDLE,             // 2 - Our app isn't running or idle
  ENEMY_SELECT,     // 3 - Choose enemy screen
  APPROVAL_DEMAND,  // 4 - I want to fight you!
  APPROVAL_WAIT,    // 5 - I am waiting to see if you want to fight me
  VS_SCREEN,        // 6 - I am showing the versus screen.
  MOVE_SELECT_INIT, // 7 - I am drawing the move select screen
  MOVE_SELECT,      // 8 - We are picking a move.
  POST_MOVE,        // 9 - We have picked one and are waiting on you or the clock
  SHOW_RESULTS,     // 10 - We are showing results
  NEXTROUND,        // 11 - reset the round
  PLAYER_DEAD,      // 12 - I die!
  ENEMY_DEAD,       // 13 - You're dead.
} fight_state;

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
} FightHandles;

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

/* Function Prototypes */
/* Network */
static void resendPacket(void);
static void sendGamePacket(uint8_t opcode);
static void sendACK(user *inbound);
static void sendRST(user *inbound);

/* Graphics */
static int putImageFile(char *name, int16_t x, int16_t y);
static void drawProgressBar(coord_t x, coord_t y, coord_t width, coord_t height, int32_t maxval, int32_t currentval, uint8_t use_leds, uint8_t reverse);
static void blinkText (coord_t x, coord_t y,coord_t cx, coord_t cy, char *text, font_t font, color_t color, justify_t justify, uint8_t times, int16_t delay);

/* Game state */
static void clearstatus(void);
static uint8_t prevEnemy(void);
static uint8_t nextEnemy(void);
static void updatehp(void);

/* Fight Sequence */
static void screen_select_draw(int8_t);
static void screen_select_close(OrchardAppContext *);
static void screen_waitapproval_draw(void);
static void screen_demand_draw(void);
static void screen_vs_draw(void);
static void show_results(void);
static void draw_attack_buttons(void);

static void end_fight(void);

#endif
