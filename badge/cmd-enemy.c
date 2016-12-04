#include "ch.h"
#include "shell.h"
#include "chprintf.h"

#include "orchard-shell.h"
#include "orchard-app.h"
#include "radio.h"

#include <stdlib.h>
#include <string.h>

extern mutex_t enemies_mutex;

void cmd_enemylist(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)argv;
  (void)argc;
  user **enemies;
  int i;
  
  enemies = enemiesGet();

  for(i = 0; i < MAX_ENEMIES; i++) {
    if( enemies[i] != NULL ) {
      chprintf(chp, "%d: %d %s (hp:%d, ic:%d, lvl:%d)\n\r", i,
	       enemies[i]->priority,
	       enemies[i]->name,
	       enemies[i]->hp,
	       enemies[i]->in_combat,
	       enemies[i]->level);
    }
  }
}

orchard_command("enemylist", cmd_enemylist);

void cmd_enemyadd(BaseSequentialStream *chp, int argc, char *argv[]) {
  user **enemies;
  uint16_t i,ic,hp,level;

  if (argc != 5) {
    chprintf(chp, "Usage: enemyadd <index> <name> <incombat> <hp> <level>\r\n");
    return;
  }
  enemies = enemiesGet();
  
  i = strtoul(argv[0], NULL, 0);
  if( i > MAX_ENEMIES )
    i = MAX_ENEMIES;

  ic = strtoul(argv[2], NULL, 0);
  hp = strtoul(argv[3], NULL, 0);
  level = strtoul(argv[4], NULL, 0);
  
  if( enemies[i] != NULL ) {
    chprintf(chp, "Index used. Incrementing %s instead.\n\r", enemies[i]->name);
    osalMutexLock(&enemies_mutex);
    enemies[i]->priority++;
    enemies[i]->in_combat = ic;
    enemies[i]->hp = hp;
    enemies[i]->level = level;
    osalMutexUnlock(&enemies_mutex);
  } else {
    osalMutexLock(&enemies_mutex);
    enemies[i] = (user *) chHeapAlloc(NULL, sizeof(user));
    enemies[i]->priority = ENEMIES_INIT_CREDIT;
    enemies[i]->in_combat = ic;
    enemies[i]->hp = hp;
    enemies[i]->level = level;
    strncpy(enemies[i]->name, argv[1], CONFIG_NAME_MAXLEN);
    osalMutexUnlock(&enemies_mutex);
  }
}
orchard_command("enemyadd", cmd_enemyadd);

void cmd_enemyping(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void) chp;
  (void) argc;
  (void) argv;

  //  chEvtBroadcast(&radio_app);
}
orchard_command("enemyping", cmd_enemyping);

static int should_stop(void) {
  uint8_t bfr[1];
  return chnReadTimeout(serialDriver, bfr, sizeof(bfr), 1);
}

// generate a list of random names and broadcast them until told to stop
void cmd_enemysim(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void) chp;
  (void) argc;
  (void) argv;
  user **enemies;

  enemies = enemiesGet();
  osalMutexLock(&enemies_mutex);
  for (int i=0; i< MAX_ENEMIES; i++) {
    if (enemies[i] == NULL) {
      char tmp[15];
      enemies[i] = (user *) chHeapAlloc(NULL, sizeof(user));
      // enemies[i]->priority will be filled in by the recipient.
      enemies[i]->type = p_guard;
      enemies[i]->netid = i;
      enemies[i]->priority = 100;
      enemies[i]->in_combat = 0;
      enemies[i]->hp = 1337;
      enemies[i]->level = 9;

      chsnprintf(tmp, sizeof(tmp), "test-%d",i);
      strncpy(enemies[i]->name, tmp, CONFIG_NAME_MAXLEN);
    }
    osalMutexUnlock(&enemies_mutex);
  }

  // jna: remove this once we go live
  chprintf(stream, "Test enemies generated.\r\n");
  // generate some packets... 
  //  while(!should_stop()) {
    //    radioAcquire(radioDriver);
    //    radioSend(radioDriver, RADIO_BROADCAST_ADDRESS, radio_prot_ping,
    //	      strlen(enemylist[i]) + 1, enemylist[i]);
    //    radioRelease(radioDriver);
  //    chThdSleepMilliseconds((5000 + rand() % 2000) / 16); // simulate timeouts
  ///  }
}
orchard_command("enemysim", cmd_enemysim);

