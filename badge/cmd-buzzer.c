/*
  cmd-buzzer.c

  basic buzzer test code - should not be included in final badge
  J. Adams <jna@retina.net>
*/

#include "ch.h"
#include "hal.h"
#include "shell.h"
#include "chprintf.h"
#include "tpm.h"

#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include "orchard.h"
#include "orchard-shell.h"
#include "userconfig.h"

#include "sound.h"

static void buzzer_stop(BaseSequentialStream *chp) {
  chprintf(chp, "silence!\r\n");
  pwmToneStop();;
}

static void buzzer_tone(BaseSequentialStream *chp, int argc, char *argv[]) {

  uint16_t freq;
  userconfig *config = getConfig();
  
  if (argc != 2) {
    chprintf(chp, "No frequency specified\r\n");
    return;
  }
  
  freq = strtoul(argv[1], NULL, 0);
  
  if (config->sound_enabled == 0) {
    chprintf(chp, "Sound is disabled. Please re-enable in setup!\r\n");
    return;
  }

  chprintf(chp, "play frequency %d...\r\n", freq);
  pwmToneStart(freq);
}

static void buzzer_play(BaseSequentialStream *chp, int argc, char *argv[]) {

  uint16_t selection;
  userconfig *config = getConfig();  
  if (config->sound_enabled == 0) {
    chprintf(chp, "Sound is disabled. Please re-enable in setup!\r\n");
    return;
  }
  
  if (argc != 2) {
    chprintf(chp, "No song specified\r\n");
    return;
  }

  selection = strtoul(argv[1], NULL, 0);

  chprintf(chp, "playing...%d\r\n", selection);

  switch(selection) {
  case 0:
    pwmChanThreadPlay (NULL, NULL, NULL);
    break;
  case 1:
    playStartupSong();
    break;
  case 2:
    playHardFail();
    break;
  case 3:
    playAttacked();
    break;
  case 4:
    playVictory();
    break;
  case 5:
    playDefeat();
    break;
  case 6:
    playDodge();
    break;
  case 7:
    playHit();
    break;
  case 8:
    playMsPacman();
    break;
  default:
    chprintf(chp, "No songa specified\r\n");
  }
  
}

static void cmd_buzzer(BaseSequentialStream *chp, int argc, char *argv[]) {

  if (argc == 0) {
    chprintf(chp, "buzzer commands:\r\n");
    chprintf(chp, "   tone [freq]          play frequency (in Hz)\r\n");
    chprintf(chp, "   play n               play note sequene #n\r\n");
    chprintf(chp, "   stop                 turn off buzzer\r\n");
    return;
  }

  if (!strcasecmp(argv[0], "tone")) {
    buzzer_tone(chp, argc, argv);
    return;
  }

  if (!strcasecmp(argv[0], "play")) {
    buzzer_play(chp, argc, argv);
    return;
  }

  if (!strcasecmp(argv[0], "stop"))
    buzzer_stop(chp);
  else
    chprintf(chp, "Unrecognized buzzer command\r\n");
}

orchard_command("buzzer", cmd_buzzer);
