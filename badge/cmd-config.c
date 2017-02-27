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

  if (!strcasecmp(argv[1], "name") && (argc == 3)) {
    strncpy(config->name, argv[2], CONFIG_NAME_MAXLEN);
    chprintf(chp, "Name set.\r\n");
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
    chprintf(chp, "   set              set variable\r\n");
    chprintf(chp, "   save             save config to flash\r\n\r\n");
    chprintf(chp, "warning: if another thread calls Save, your changes will get saved. Oh well.\r\n");
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
