#include "ch.h"
#include "hal.h"
#include "orchard.h"

#include "flash.h"
#include "userconfig.h"

#include <string.h>

static userconfig config_cache;

void configSave(void) {
}

static void init_config(struct userconfig *config) {

  config->signature = CONFIG_SIGNATURE;
  config->version = CONFIG_VERSION;

  memset(config->name, 0, CONFIG_NAME_MAXLEN);
  /* reset here */
  config->won = 0;
  config->lost = 0;
  config->p_type = p_pleeb;
  config->in_combat = 0;
  config->lastcombat = 0; // how long since combat started

  // TODO: config->netid determination
  // we need a unique identifier per device. How?

  /* stats, dunno if we will use */
  config->level = 1;
  config->hp = 1000;
  config->xp = 0;
  config->gold = 500;
  
  config->str = 0;
  config->ac = 0;
  config->dex = 0;
  config->won = 0;
  config->lost = 0;
  
  /* these fields are only used during attack-response */
  config->damage = 0;
  config->is_crit = 0;

  config->led_shift = 0;
}

void configStart(void) {
  const struct userconfig *config;

  config = (const struct userconfig *) CONFIG_BLOCK;

  if ( config->signature != CONFIG_SIGNATURE ) {
    init_config(&config_cache);
    config = &config_cache;
    // write to flash
  } else if ( config->version != CONFIG_VERSION ) {
    init_config(&config_cache);
    config = &config_cache;
    // reset and write to flash
  }

  memcpy( &config_cache, config, sizeof(userconfig) ); // copy configuration to volatile cache
}

const struct userconfig *getConfig(void) {
  return &config_cache;
}
