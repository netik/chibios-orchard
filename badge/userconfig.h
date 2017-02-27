#ifndef __USERCONFIG_H__
#define __USERCONFIG_H__
/* userconfig.h
 * 
 * anything that has to do with long term storage of users
 * goes in here 
 */

#define CONFIG_FLASH_ADDR 0x1fc00
#define CONFIG_FLASH_SECTOR_BASE 127
#define CONFIG_SIGNATURE  0xdeadbeef  // duh

#define CONFIG_OFFSET     0
#define CONFIG_VERSION    99
#define CONFIG_NAME_MAXLEN 10

/* bit patterns for unlocks - stored in config, */
/* char unlock_codes[] in app-unlock.c */
#define UL_EFF        ( 1 << 0 ) // +10% Defense, EFF logo on badge screen '1EFF1'
#define UL_HP         ( 1 << 1 ) // +10% HP  'DEFAD'
#define UL_MIGHT      ( 1 << 2 ) // +1 might 'A7A7A'
#define UL_LUCK       ( 1 << 3 ) // +20% additional luck '77007'
#define UL_HEALZ      ( 1 << 4 ) // 1/2 heal time  'AED17'
#define UL_LEDZ       ( 1 << 5 ) // unlocks additional LED patterns '01AC0'
#define UL_CAESAR     ( 1 << 6 ) // always caesar, can heal as caesar 'BAEAC'
#define UL_SENATOR    ( 1 << 7 ) // always senator (adds 20% to all attacks) 'DE01A'
#define UL_TIMELORD   ( 1 << 8 ) // can transmit time '90046' - the dr#'s number

/* can we display these on top LEDs */


#define maxhp(level)       (50+(20*(level-1)))

typedef enum _player_type {
  p_guard,
  p_senator,
  p_casear
} player_type;

/* if you change the userconfig struct, update CONFIG_VERSION
 * above
 */
typedef struct userconfig {
  uint32_t signature;
  uint32_t version;

  /* unique network ID determined from use of lower 64 bits of SIM-UID */
  uint32_t netid;

  /* hw config */
  uint8_t led_pattern;
  
  /* used for solid-color */
  uint8_t led_r;
  uint8_t led_g;
  uint8_t led_b;
  
  uint8_t led_shift;
  uint8_t sound_enabled;
  
  /* game */
  player_type p_type;
  char name[CONFIG_NAME_MAXLEN];
  
  uint16_t lastcombat; // how long since combat started
  uint8_t in_combat; 
  uint16_t unlocks;

  /* todo: determine which stats are relevant to the game (egan) */
  int16_t hp;
  uint16_t xp;
  uint16_t gold;
  uint8_t level;

  uint8_t agl;
  uint8_t might;
  uint8_t luck;
  
  /* long-term counters */
  uint16_t won;
  uint16_t lost;

} userconfig;

typedef struct _userpkt {
  /* this is a shortened form of userdata for transmission */
  /* appx 52 bytes, max is 66 (AES limitiation) */

  /* stash this away for future attacks/lookups */
  /* unique network ID determined from use of lower 64 bits of SIM-UID */
  uint32_t netid;         /* 4 */
  uint8_t opcode;         /* 1 - BATTLE_OPCODE */

  /* Network Payload */
  uint16_t seq;           /* 2 */
  uint16_t acknum;        /* 2 - only used during acknowledgements */
  uint8_t ttl;            /* 1 */

  /* Player Payload */
  char name[CONFIG_NAME_MAXLEN];  /* 16 */
  player_type p_type;     /* 1 */
  uint8_t in_combat;      /* 1 */        
  int16_t hp;            /* 2 */
  uint16_t xp;            /* 2 */  
  uint16_t gold;          /* 2 */

  uint8_t level;          /* 1 */

  uint8_t agl;            /* 1 */
  uint8_t might;          /* 1 */
  uint8_t luck;           /* 1 */

  /* Player stats, used in ping packet */
  uint16_t won;           /* 2 */
  uint16_t lost;          /* 2 */

  /* Battle Payload */
  /* A bitwise map indicating the attack type */
  uint8_t attack_bitmap;  /* 1 */
  int16_t damage;         /* 2 */

} user;

extern void configStart(void);
extern void configSave(userconfig *);
extern userconfig *getConfig(void);

#endif
