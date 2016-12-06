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
#define CONFIG_VERSION    8
#define CONFIG_NAME_MAXLEN 10

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
  uint32_t signature;
  uint32_t version;

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

} userconfig;

/* attack profile (attack_bitmap) */
#define ATTACK_ONESHOT  ( 1 << 7 ) 
#define ATTACK_ISCRIT   ( 1 << 6 )

#define ATTACK_HI       ( 1 << 5 )
#define ATTACK_MED      ( 1 << 4 )
#define ATTACK_LOW      ( 1 << 3 )

#define BLOCK_HI   ( 1 << 2 )
#define BLOCK_MED  ( 1 << 1 )
#define BLOCK_LOW  ( 1 << 0 )

typedef struct _userpkt {
  /* this is a shortened form of userdata for transmission */
  /* appx 40 bytes, max is 66 (AES limitiation) */

  /* Network Payload */
  uint32_t netid_src;     /* 4 */
  uint32_t netid_dst;     /* 8 */
  uint16_t seq;           /* 2 */
  uint8_t ttl;            /* 1 */

  /* Player Payload */
  char name[CONFIG_NAME_MAXLEN];  /* 16 */
  player_type p_type;       /* 1 */
  uint8_t in_combat;      /* 1 */        
  uint16_t hp;            /* 2 */
  uint8_t level;          /* 1 */

  /* Battle Payload */
  /* A bitwise map indicating the attack and block --  see ATTACK_ and BLOCK_ operators above. */
  uint8_t attack_bitmap;  
  uint8_t damage;         /* 1 */
} user;

extern void configStart(void);
extern void configSave(userconfig *);
extern userconfig *getConfig(void);

#endif
