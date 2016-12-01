#ifndef __USERCONFIG_H__
#define __USERCONFIG_H__
/* userconfig.h
 * 
 * anything that has to do with long term storage of users
 * goes in here 
 */

#define CONFIG_FLASH_ADDR 0x1e000
#define CONFIG_FLASH_SECTOR_BASE 120
#define CONFIG_SIGNATURE  0xdeadbeef  // duh

#define CONFIG_OFFSET     0
#define CONFIG_VERSION    7
#define CONFIG_NAME_MAXLEN 10

#define ENEMIES_INIT_CREDIT 4

typedef enum _player_type {
  p_pleeb,
  p_guard,
  p_senator,
  p_casear
} player_type;

/* if you change the userconfig struct, update CONFIG_VERSION
 * above
 */
typedef struct userconfig {
  uint32_t  signature;
  uint32_t  version;

  /* unique network ID determined from use of lower 64 bits of SIM-UID */
  uint32_t netid;

  /* hw config */
  uint8_t led_pattern;
  uint8_t led_shift;
  uint8_t sound_enabled;
  
  /* game */
  player_type p_type;
  char name[CONFIG_NAME_MAXLEN];
  
  uint16_t lastcombat; // how long since combat started
  uint8_t in_combat; 

  /* todo: determine which stats are relevant to the game (egan) */
  uint16_t hp;
  uint16_t xp;
  uint16_t gold;
  uint8_t level;

  uint8_t spr;
  uint8_t str;
  uint8_t def;
  uint8_t dex;

  /* long-term counters */
  uint16_t won;
  uint16_t lost;
  
  /* these fields are only used during attack-response */
  uint8_t damage;
  uint8_t is_crit;
} userconfig;


typedef struct _user {
  /* this is a shortened form of userdata for transmission */
  /* appx 32 bytes */
  uint8_t priority;
  uint32_t netid;  
  char name[CONFIG_NAME_MAXLEN];
  uint8_t in_combat;
  uint16_t hp;
  uint8_t level;
} user;

extern void configStart(void);
extern void configSave(userconfig *);
extern userconfig *getConfig(void);

#endif
