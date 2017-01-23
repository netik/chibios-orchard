#include "ch.h"
#include "hal.h"
#include "orchard.h"
#include "shell.h"
#include "chprintf.h"

#include "flash.h"
#include "userconfig.h"
#include "orchard-shell.h"
#include <string.h>

static userconfig config_cache;

#ifdef RANDOM_DICE
static int randomint(int max);
#endif

mutex_t config_mutex;

void configSave(userconfig *newConfig) {
  int8_t ret;
  uint8_t *config;
  
  osalMutexLock(&config_mutex);  
  config = (uint8_t *)CONFIG_FLASH_ADDR;
  //  memcpy(config, newConfig, sizeof(userconfig) );

  flashErase(CONFIG_FLASH_SECTOR_BASE,1);
  ret = flashProgram( (uint8_t *) newConfig, config, sizeof(userconfig) );

  if (ret == F_ERR_NOTBLANK) {
    chprintf(stream, "ERROR (%d): Unable to save config to flash.\r\n", ret);
  } else {
    chprintf(stream, "Config saved.\r\n");
  }
  osalMutexUnlock(&config_mutex);
}

#ifdef RANDOM_DICE
static int four_d_six(void) {
  /* this seems stupid, for a computer. */
  int a[5];
  int tot = 0;
  int lowest = 4;
  a[4] = 6;
  
  for (int i=0; i<4; i++)  {
    a[i] = randomint(6);
    if (a[i] < a[lowest]) { lowest = i; };
  }
  
  for (int i=0; i<4; i++)  {
    if (i != lowest) { tot = tot + a[i]; } 
  }
 
  return tot;
}

int randomint(int max) {
  return rand() % max;
}
#endif

static void init_config(userconfig *config) {
  /* please keep these in the same order as userconfig.h
   * so it's easier to maintain.  */
  config->signature = CONFIG_SIGNATURE;
  config->version = CONFIG_VERSION;

  /* this is a very, very naive approach to uniqueness, but it might work. */
  /* an alternate approach would be to store all 80 bits, but then our radio */
  /* packets would be huge. */
  config->netid = SIM->UIDML ^ SIM->UIDL;
  
  config->led_pattern = 8;
  config->led_shift = 4;
  config->sound_enabled = 1;

  config->p_type = p_guard;  
  memset(config->name, 0, CONFIG_NAME_MAXLEN);

  /* reset here */
  config->won = 0;
  config->lost = 0;

  config->lastcombat = 0; // how long since combat started
  config->in_combat = 0;
  
  /* stats, dunno if we will use */
  config->hp = maxhp(1);
  config->xp = 0;
  config->gold = 500;
  config->level = 1;

  config->agl = 1;
  config->luck = 20;
  config->might = 1;
  
  config->won = 0;
  config->lost = 0;

  /* randomly pick a new color */
  config->led_r = rand() % 255;
  config->led_g = rand() % 255;
  config->led_b = rand() % 255;
}

void configStart(void) {
  const userconfig *config;
  osalMutexObjectInit(&config_mutex);
  osalMutexLock(&config_mutex);
  
  config = (const userconfig *) CONFIG_FLASH_ADDR;
  
  if ( config->signature != CONFIG_SIGNATURE ) {
    chprintf(stream, "Config not found, Initializing!\r\n");
    init_config(&config_cache);
    config = &config_cache;
    configSave(&config_cache);
    // write to flash
  } else if ( config->version != CONFIG_VERSION ) {
    chprintf(stream, "Config found, but wrong version.\r\n");
    init_config(&config_cache);
    config = &config_cache;
    configSave(&config_cache);
  } else {
    chprintf(stream, "Config OK!\r\n");
    if (config_cache.in_combat != 0) { 
      chprintf(stream, "You were stuck in combat. Fixed.\r\n");
      config_cache.in_combat = 0;
      configSave(&config_cache);
    }
  }

  memcpy(&config_cache, config, sizeof(userconfig));
  
  osalMutexUnlock(&config_mutex);
}

struct userconfig *getConfig(void) {
  // returns volitale config
  return &config_cache;
}
