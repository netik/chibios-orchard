#define CONFIG_FLASH_ADDR 0x1e000
#define CONFIG_FLASH_SECTOR_BASE 120
#define CONFIG_SIGNATURE  0xdeadbeef  // duh

#define CONFIG_OFFSET     0
#define CONFIG_VERSION    2
#define CONFIG_NAME_MAXLEN 16

typedef enum _player_type {
  p_pleeb,
  p_guard,
  p_senator,
  p_casear
} player_type;

typedef struct userconfig {
  uint32_t  signature;
  uint32_t  version;

  player_type p_type;
  char name[CONFIG_NAME_MAXLEN];
  
  uint8_t in_combat; 
  uint16_t lastcombat; // how long since combat started

  /* unique network ID determined from use of lower 64 bits of SIM-UID */
  uint16_t netid;

  /* todo: determine which stats are relevant to the game (egan) */
  uint16_t hp;
  uint16_t xp;
  uint16_t gold;
  uint8_t level;
  
  uint8_t str;
  uint8_t ac;
  uint8_t dex;
  uint16_t won;
  uint16_t lost;
  
  /* these fields are only used during attack-response */
  uint8_t damage;
  uint8_t is_crit;

  uint8_t led_pattern;
  uint8_t led_shift;
} userconfig;

extern void configStart(void);
extern void configSave(userconfig *);
extern userconfig *getConfig(void);

