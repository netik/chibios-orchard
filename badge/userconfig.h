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
#define CONFIG_VERSION    13
#define CONFIG_NAME_MAXLEN 10

#define CONFIG_LEDSIGN_MAXLEN	124

/* can we display these on top LEDs */

typedef enum _player_type {
  p_notset,
  p_guard,
  p_senator,
  p_gladiatrix,
  p_caesar,
  p_bender
} player_type;

/* if you change the userconfig struct, update CONFIG_VERSION
 * above
 */
typedef struct userconfig {
  uint32_t signature;
  uint32_t version;

  /* unique network ID determined from use of lower 64 bits of
     SIM-UID */
  uint32_t netid;

  /* hw config */
  uint8_t led_pattern;
  
  /* used for solid-color */
  uint8_t led_r;
  uint8_t led_g;
  uint8_t led_b;

  uint8_t led_shift;
  uint8_t sound_enabled;
  uint8_t airplane_mode;
  
  /* touchpad calibration data */
  uint8_t touch_data_present;
  float touch_data[6];

  /* saved LED sign string */
  char led_string[CONFIG_LEDSIGN_MAXLEN];

  /* game */
  player_type current_type;
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
  /* appx 51 bytes, max is 66 (AES limitiation) */

  /* stash this away for future attacks/lookups */
  /* unique network ID determined from use of lower 64 bits of SIM-UID */
  uint32_t netid;         /* 4 */
  uint8_t opcode;         /* 1 - BATTLE_OPCODE */

  /* Network Payload */
  uint16_t seq;           /* 2 */
  uint16_t acknum;        /* 2 - only used during acknowledgements */
  uint8_t ttl;            /* 1 */

  /* clock, if any */
  unsigned long rtc;      /* 4 */ 
  
  /* Player Payload */
  char name[CONFIG_NAME_MAXLEN + 1];  /* 16 */
  player_type p_type;     /* 1 */
  player_type current_type;     /* 1 */
  uint8_t in_combat;      /* 1 */
  uint16_t unlocks;       /* 2 */
  int16_t hp;             /* 2 */
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

/* prototypes */
extern void configStart(void);
extern void configSave(userconfig *);
extern userconfig *getConfig(void);
extern int16_t maxhp(player_type, uint16_t, uint8_t);

extern unsigned long rtc;
extern unsigned long rtc_set_at;

#endif
