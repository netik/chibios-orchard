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

static void buzzer_stop(BaseSequentialStream *chp) {
  chprintf(chp, "silence!\r\n");
  pwmToneStop();;
}

static void buzzer_play(BaseSequentialStream *chp, int argc, char *argv[]) {

  uint16_t freq;

  if (argc != 2) {
    chprintf(chp, "No frequency specified\r\n");
    return;
  }

  freq = strtoul(argv[1], NULL, 0);
  chprintf(chp, "play frequency %d...\r\n", freq);
  pwmToneStart(freq);
}

static void cmd_buzzer(BaseSequentialStream *chp, int argc, char *argv[]) {

  if (argc == 0) {
    chprintf(chp, "buzzer commands:\r\n");
    chprintf(chp, "   play [freq]          play frequency (in Hz)\r\n");
    return;
  }

  if (!strcasecmp(argv[0], "play"))
    buzzer_play(chp, argc, argv);
  else
    if (!strcasecmp(argv[0], "stop"))
      buzzer_stop(chp);
    else
      chprintf(chp, "Unrecognized buzzer command\r\n");
}

orchard_command("buzzer", cmd_buzzer);
