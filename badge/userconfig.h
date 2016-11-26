#define CONFIG_SIGNATURE  0xdeadbeef  // duh
#define CONFIG_BLOCK      0x1e000
#define CONFIG_OFFSET     0
#define CONFIG_VERSION    0

#define CONFIG_NAME_MAXLEN   16

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
  
  uint16_t netid;

  /* stats, dunno if we will use */
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

  uint8_t led_shift;
} userconfig;

void configStart(void);

const userconfig *getConfig(void);

void configFlush(void); // call on power-down to flush config state
void configLazyFlush(void);  // call periodically to sync state, but only when dirty
