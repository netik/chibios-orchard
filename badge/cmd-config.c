/* cmd-config - config dump tool, mainly for debugging */

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "ch.h"
#include "shell.h"
#include "chprintf.h"
#include "led.h"
#include "datetime.h"

#include "orchard-shell.h"
#include "orchard-app.h"
#include "userconfig.h"

/* prototypes */
static void cmd_config_show(BaseSequentialStream *chp, int argc, char *argv[]);
static void cmd_config_set(BaseSequentialStream *chp, int argc, char *argv[]);
static void cmd_config_save(BaseSequentialStream *chp, int argc, char *argv[]);
static void cmd_config(BaseSequentialStream *chp, int argc, char *argv[]);
static void cmd_config_led_stop(BaseSequentialStream *chp);
static void cmd_config_led_run(BaseSequentialStream *chp, int argc, char *argv[]);
static void cmd_config_led_all(BaseSequentialStream *chp, int argc, char *argv[]);
static void cmd_config_led_dim(BaseSequentialStream *chp, int argc, char *argv[]);
static void cmd_config_led_list(BaseSequentialStream *chp);

/* end prototypes */
                
static void cmd_config_show(BaseSequentialStream *chp, int argc, char *argv[])
{
  tmElements_t dt;
  unsigned long curtime;
  unsigned long delta;
  userconfig *config = getConfig();
  (void)argv;
  (void)argc;

  if (rtc != 0) {
    delta = ST2S((chVTGetSystemTime() - rtc_set_at));
    curtime = rtc + delta;
    breakTime(curtime, &dt);
    
    chprintf(chp, "%02d/%02d/%02d %02d:%02d:%02d\r\n", 1970+dt.Year, dt.Month, dt.Day, dt.Hour, dt.Minute, dt.Second);
  }
  
  chprintf(chp, "name       %s (class now:%d / perm:%d)\r\n", config->name, config->current_type, config->p_type);
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
  if (!strcasecmp(argv[1], "ctype")) {
    config->current_type = strtoul (argv[2], NULL, 0);
    chprintf(chp, "current class set to %d.\r\n", config->current_type);
    return;
  }
  if (!strcasecmp(argv[1], "ptype")) {
    config->p_type = strtoul (argv[2], NULL, 0);
    chprintf(chp, "perm class set to %d.\r\n", config->p_type);
    return;
  }
  if (!strcasecmp(argv[1], "level")) {
    config->level = strtoul (argv[2], NULL, 0);
    chprintf(chp, "level set to %d.\r\n", config->level);
    return;
  }
  if (!strcasecmp(argv[1], "unlocks")) {
    config->unlocks = strtoul (argv[2], NULL, 0);
    chprintf(chp, "Unlocks set to %d.\r\n", config->unlocks);
    return;
  }
  if (!strcasecmp(argv[1], "hp")) {
    config->hp = strtoul (argv[2], NULL, 0);
    chprintf(chp, "HP set to %d.\r\n", config->hp);
    return;
  }
  if (!strcasecmp(argv[1], "xp")) {
    config->xp = strtoul (argv[2], NULL, 0);
    chprintf(chp, "XP set to %d.\r\n", config->xp);
    return;
  }
  if (!strcasecmp(argv[1], "agl")) {
    config->agl = strtoul (argv[2], NULL, 0);
    chprintf(chp, "agl set to %d.\r\n", config->agl);
    return;
  }

  if (!strcasecmp(argv[1], "luck")) {
    config->luck = strtoul (argv[2], NULL, 0);
    chprintf(chp, "luck set to %d.\r\n", config->luck);
    return;
  }

  if (!strcasecmp(argv[1], "might")) {
    config->might = strtoul (argv[2], NULL, 0);
    chprintf(chp, "might set to %d.\r\n", config->might);
    return;
  }

  if (!strcasecmp(argv[1], "rtc")) {
    rtc = strtoul (argv[2], NULL, 0);
    rtc_set_at = chVTGetSystemTime();
    chprintf(chp, "rtc set to %d.\r\n", rtc);
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
    chprintf(chp, "   show           show config\r\n");
    chprintf(chp, "   set nnn yyy    set variable nnn to yyy (vars: name, sound, type)\r\n");
    chprintf(chp, "   led list       list animations available\r\n");
    chprintf(chp, "   led dim n      dimmer level (0-7) 0=brightest\r\n");
    chprintf(chp, "   led run n      run pattern #n\r\n");
    chprintf(chp, "   led all r g b  set all leds to one color (0-255)\r\n");
    chprintf(chp, "   led stop       stop and blank LEDs\r\n");
    chprintf(chp, "   save           save config to flash\r\n\r\n");

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
  
  if (!strcasecmp(argv[0], "led")) {
    if (!strcasecmp(argv[1], "list")) {
      cmd_config_led_list(chp);
      return;
    }
    
    if (!strcasecmp(argv[1], "dim")) {
      cmd_config_led_dim(chp, argc, argv);
      return;
    }
    
    if (!strcasecmp(argv[1], "all")) {
      cmd_config_led_all(chp, argc, argv);
      return;
    }
    
    if (!strcasecmp(argv[1], "run")) { 
      cmd_config_led_run(chp, argc, argv);
      return;
    }
    
    if (!strcasecmp(argv[1], "stop")) { 
      cmd_config_led_stop(chp);
      return;
    }
  }
  
  chprintf(chp, "Unrecognized config command.\r\n");
  
}

/* LED configuration */
extern struct FXENTRY fxlist[];

static void cmd_config_led_stop(BaseSequentialStream *chp) {
  effectsStop();
  chprintf(chp, "Off.\r\n");
}

static void cmd_config_led_run(BaseSequentialStream *chp, int argc, char *argv[]) {

  uint8_t pattern;
  userconfig *config = getConfig();
    
  if (argc != 3) {
    chprintf(chp, "No pattern specified\r\n");
    return;
  }

  pattern = strtoul(argv[2], NULL, 0);
  if ((pattern < 1) || (pattern > LED_PATTERN_COUNT)) {
    chprintf(chp, "Invaild pattern #!\r\n");
    return;
  }

  config->led_pattern = pattern-1;
  ledResetPattern();
  ledSetFunction(fxlist[config->led_pattern].function);
  
  chprintf(chp, "Pattern changed to %s.\r\n", fxlist[pattern-1].name);
}

static void cmd_config_led_all(BaseSequentialStream *chp, int argc, char *argv[]) {

  int16_t r,g,b;
  userconfig *config = getConfig();

  if (argc != 5) {
    chprintf(chp, "Not enough arguments.\r\n");
    return;
  }

  r = strtoul(argv[2], NULL, 0);
  g = strtoul(argv[3], NULL, 0);
  b = strtoul(argv[4], NULL, 0);

  if ((r < 0) || (r > 255) ||
      (g < 0) || (g > 255) ||
      (b < 0) || (b > 255) ) {
    chprintf(chp, "Invaild value. Must be 0 to 255.\r\n");
    return;
  }

  chprintf(chp, "LEDs set.\r\n");

  config->led_r = r;
  config->led_g = g;
  config->led_b = b;
  config->led_pattern = LED_PATTERN_COUNT - 1;

  // the last pattern is always the 'ALL' state. 
  ledResetPattern();
  

}

static void cmd_config_led_dim(BaseSequentialStream *chp, int argc, char *argv[]) {

  int8_t level;
  userconfig *config = getConfig();

  if (argc != 3) {
    chprintf(chp, "level?\r\n");
    return;
  }

  level = strtoul(argv[2], NULL, 0);

  if ((level < 0) || (level > 7)) {
    chprintf(chp, "Invaild level. Must be 0 to 7.\r\n");
    return;
  }

  ledSetBrightness(level);
  chprintf(chp, "Level now %d.\r\n", level);

  config->led_shift = level;
}

static void cmd_config_led_list(BaseSequentialStream *chp) {
  chprintf(chp, "\r\nAvailable LED Patterns\r\n");

  for (int i=0; i < LED_PATTERN_COUNT; i++) {
    chprintf(chp, "%2d) %s\r\n", i+1, fxlist[i].name);
  }
}


orchard_command("config", cmd_config);
