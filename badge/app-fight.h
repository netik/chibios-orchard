#ifndef __APP_FIGHT_H__
#define __APP_FIGHT_H__

/* debugging - this protocol can be a real pain in the ass */
#undef DEBUG_FIGHT_TICK        // show the clock tick events, ugfx events, and other misc events
#define DEBUG_FIGHT_NETWORK    // show all network (radio) traffic
#define DEBUG_FIGHT_STATE      // debug state changes
  
// where the progres bar goes 
#define PROGRESS_BAR_X 60
#define PROGRESS_BAR_Y 210
#define PROGRESS_BAR_W 200
#define PROGRESS_BAR_H 10

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
#define OP_BATTLE_REQ       0x01   /* I would like to fight */
#define OP_BATTLE_REQ_ACK   0x02   /* Got it, I am showing the APPROVAL_DEMAND screen, please move to APPROVAL_WAIT */
#define OP_BATTLE_GO        0x04   /* Yes, let's fight. */
#define OP_BATTLE_GO_ACK    0x06   /* I agree with you, transition to the VS Screen */

#define OP_BATTLE_DECLINED  0x08   /* Nope, I decline. */

#define OP_GRANT            0x0a   /* We are granting you a buff */

#define OP_IMDEAD           0x0b   /* I died */
#define OP_YOUDIE           0x0c   /* I kill you */

#define OP_IMOVED           0x10   /* Here is my Move */

#define OP_NEXTROUND        0x14   /* Please start the next round */

#define OP_FIN              0xfe   /* It's over. */
#define OP_FIN_ACK          0xff   /* I agree It's over. */

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

  /* text workspace */
  char tmp[40];

  ProtoHandles *proto;
  
} FightHandles;


typedef struct _DmgLoc {
  /* this structure represents a location and filename based on a 
   * damage bitmap
   */
  int ypos;
  char type;
  char filename[13];
} DmgLoc;

typedef struct _RoundState {
  /* round number */
  uint8_t roundno;

  /* damage state, before results */
  int16_t last_hit;
  int16_t last_damage;
  uint8_t ourattack;
  uint8_t theirattack;

  /* damage state and UI after results */
  color_t p1color;
  color_t p2color;
  int16_t overkill_us;
  int16_t overkill_them;
  char ourdmg_s[10];
  char theirdmg_s[10];

  uint8_t match;
  uint8_t winner;
} RoundState;

extern orchard_app_instance instance;

#endif
