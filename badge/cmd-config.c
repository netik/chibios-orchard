/* cmd-config - config dump tool, mainly for debugging */

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ch.h"
#include "shell.h"
#include "chprintf.h"

#include "orchard-shell.h"
#include "orchard-app.h"

#include "userconfig.h"

static void cmd_config_show(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)argv;
  (void)argc;
  userconfig *config = getConfig();

  chprintf(chp, "name       %s (type: %d)\r\n", config->name, config->p_type);
  chprintf(chp, "signature  0x%08x\r\n", config->signature);
  chprintf(chp, "version    %d\r\n", config->version);
  chprintf(chp, "netid      0x%08x\r\n", config->netid);
  chprintf(chp, "unlocks    0x%02x\r\n", config->unlocks);
  chprintf(chp, "sound      %d\r\n", config->sound_enabled);
  chprintf(chp, "lastcombat %d\r\n", config->lastcombat);
  chprintf(chp, "incombat   %d\r\n", config->in_combat);
  chprintf(chp, "led mode   %d dim %d\r\n", config->led_pattern, config->led_shift);
  chprintf(chp, "led color  %d %d %d\r\n",
           config->led_r,
           config->led_g,
           config->led_b);
  
  chprintf(chp, "won/lost   %d/%d\r\n\r\n",
           config->won,
           config->lost);

  chprintf(chp, "lvl %4d agl %4d might %4d luck %d\r\n",
           config->level,
           config->agl,
           config->might,
           config->luck);

  chprintf(chp, "hp  %4d xp  %4d gold  %4d\r\n",
           config->hp,
           config->xp,
           config->gold
           );
}

static void cmd_config_set(BaseSequentialStream *chp, int argc, char *argv[]) {
  /* TODO: allow changing of a few basic settings */
  (void)argv;
  (void)argc;

  userconfig *config = getConfig();

  if (argc != 3) {
    chprintf(chp, "Invalid set command.\r\nUsage: config set var value\r\n");
    return;
  }
  
  if (!strcasecmp(argv[1], "name")) {
    strncpy(config->name, argv[2], CONFIG_NAME_MAXLEN);
    chprintf(chp, "Name set.\r\n");
    return;
  }

  if (!strcasecmp(argv[1], "sound")) {
    if (!strcmp(argv[2], "1")) {
      config->sound_enabled = 1;
    } else {
      config->sound_enabled = 0;
    }
    
    strncpy(config->name, argv[2], CONFIG_NAME_MAXLEN);
    chprintf(chp, "Sound set to %d.\r\n", config->sound_enabled);
    return;
  }

  // remove these before launch!!
  if (!strcasecmp(argv[1], "type")) {
    config->p_type = atoi(argv[2]);
    chprintf(chp, "Type set to %d.\r\n", config->p_type);
    return;
  }

  if (!strcasecmp(argv[1], "unlocks")) {
    config->unlocks = atoi(argv[2]);
    chprintf(chp, "Unlocks set to %d.\r\n", config->unlocks);
    return;
  }
  if (!strcasecmp(argv[1], "hp")) {
    config->hp = atoi(argv[2]);
    chprintf(chp, "HP set to %d.\r\n", config->hp);
    return;
  }
  if (!strcasecmp(argv[1], "xp")) {
    config->xp = atoi(argv[2]);
    chprintf(chp, "XP set to %d.\r\n", config->xp);
    return;
  }
  if (!strcasecmp(argv[1], "agl")) {
    config->agl = atoi(argv[2]);
    chprintf(chp, "agl set to %d.\r\n", config->agl);
    return;
  }

  if (!strcasecmp(argv[1], "luck")) {
    config->luck = atoi(argv[2]);
    chprintf(chp, "luck set to %d.\r\n", config->luck);
    return;
  }

  if (!strcasecmp(argv[1], "might")) {
    config->might = atoi(argv[2]);
    chprintf(chp, "might set to %d.\r\n", config->might);
    return;
  }
  
  chprintf(chp, "Invalid set command.\r\n");
}

static void cmd_config_save(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void)argv;
  (void)argc;

  userconfig *config = getConfig();
  configSave(config);

  chprintf(chp, "Config saved.\r\n");
}


static void cmd_config(BaseSequentialStream *chp, int argc, char *argv[])
{

  if (argc == 0) {
    chprintf(chp, "config commands:\r\n");
    chprintf(chp, "   show             show config\r\n");
    chprintf(chp, "   set nnn yyy      set variable nnn to yyy (vars: name, sound, type)\r\n");
    chprintf(chp, "   save             save config to flash\r\n\r\n");
    chprintf(chp, "warning: if another thread updates the config, your changes could conflict!\r\n");
    chprintf(chp, "         use with caution!\r\n");
    return;
  }

  if (!strcasecmp(argv[0], "show")) {
    cmd_config_show(chp, argc, argv);
    return;
  }
  
  if (!strcasecmp(argv[0], "set")) { 
    cmd_config_set(chp, argc, argv);
    return;
  }

  if (!strcasecmp(argv[0], "save")) {
    cmd_config_save(chp, argc, argv);
    return;
  }

  chprintf(chp, "Unrecognized config command.\r\n");

}

orchard_command("config", cmd_config);
