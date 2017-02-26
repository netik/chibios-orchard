/* cmd-config - config dump tool, mainly for debugging */

#include <stdlib.h>
#include <string.h>

#include "ch.h"
#include "shell.h"
#include "chprintf.h"

#include "orchard-shell.h"
#include "orchard-app.h"

#include "userconfig.h"

void cmd_configdump(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)argv;
  (void)argc;
  userconfig *config = getConfig();

  chprintf(chp, "User Configuration\r\n\r\n");
  chprintf(chp, "name      %s (type: %d)\r\n", config->name, config->p_type);
  chprintf(chp, "signature 0x%08x\r\n", config->signature);
  chprintf(chp, "version   %d\r\n", config->version);
  chprintf(chp, "netid     0x%08x\r\n", config->netid);
  chprintf(chp, "unlocks   0x%02x\r\n\r\n", config->unlocks);
  chprintf(chp, "sound     %d\r\n", config->sound_enabled);

  chprintf(chp, "won/lost  %d/%d\r\n\r\n",
           config->won,
           config->lost);
  
  chprintf(chp, " lastcombat %d\r\n", config->lastcombat);
  chprintf(chp, "   incombat %d\r\n", config->in_combat);

  chprintf(chp, "LED pattern %d dim %d\r\n", config->led_pattern, config->led_shift);
  chprintf(chp, "  LED color %d %d %d\r\n",
           config->led_r,
           config->led_g,
           config->led_b);
  

  chprintf(chp, "hp %d xp %d gold %d level %d agl %d might %d luck %d\r\n",
           config->hp,
           config->xp,
           config->gold,
           config->level,
           config->agl,
           config->might,
           config->luck);


}

orchard_command("config", cmd_configdump);
