#include "ch.h"
#include "shell.h"
#include "chprintf.h"

#include "orchard-shell.h"
#include "orchard-app.h"
#include "unlocks.h"

#include <stdlib.h>
#include <string.h>

extern mutex_t enemies_mutex;

void cmd_peerlist(BaseSequentialStream *chp, int argc, char *argv[])
{
  (void)argv;
  (void)argc;
  peer **enemies;
  int i,cnt;

  cnt = 0;
  enemies = enemiesGet();
  
  for(i = 0; i < MAX_ENEMIES; i++) {
    if( enemies[i] != NULL ) {
      cnt++;
      chprintf(chp, "%d: [%08x] %s (hp:%d, in_combat:%d, lvl:%d, ttl %d)\n\r",
               i,
               enemies[i]->netid,
	       enemies[i]->name,
	       enemies[i]->hp,
	       enemies[i]->in_combat,
	       enemies[i]->level,
               enemies[i]->ttl);
    }
  }

  if (cnt == 0) {
    chprintf(chp, "No enemies. Why so lonely?\r\n");
  }
}

orchard_command("peers", cmd_peerlist);

void cmd_peeradd(BaseSequentialStream *chp, int argc, char *argv[]) {
  peer **enemies;
  uint16_t i,ic,hp,level;

  if (argc != 5) {
    chprintf(chp, "Usage: peeradd <index> <name> <incombat> <hp> <level>\r\n");
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
    enemies[i]->ttl++;
    enemies[i]->in_combat = ic;
    enemies[i]->hp = hp;
    enemies[i]->level = level;
    osalMutexUnlock(&enemies_mutex);
  } else {
    osalMutexLock(&enemies_mutex);
    enemies[i] = (peer *) chHeapAlloc(NULL, sizeof(peer));
    enemies[i]->ttl = PEER_TTL_INITIAL;
    enemies[i]->in_combat = ic;
    enemies[i]->hp = hp;
    enemies[i]->level = level;
    strncpy(enemies[i]->name, argv[1], CONFIG_NAME_MAXLEN);
    osalMutexUnlock(&enemies_mutex);
  }
}
orchard_command("peeradd", cmd_peeradd);

// Generate a list of random names to simulate other players.  if you
// attack these simulated users, the game will run the fight locally
// and not wait for network traffic.

void cmd_peersim(BaseSequentialStream *chp, int argc, char *argv[]) {
  (void) chp;
  (void) argc;
  (void) argv;
  peer **enemies;

  enemies = enemiesGet();
  osalMutexLock(&enemies_mutex);
  for (int i=0; i< MAX_ENEMIES; i++) {
    if (enemies[i] == NULL) {
      char tmp[15];
      enemies[i] = (peer *) chHeapAlloc(NULL, sizeof(peer));
      // enemies[i]->ttl will be filled in by the recipient.
      enemies[i]->current_type = p_guard;
      enemies[i]->p_type = p_guard;
      enemies[i]->netid = i; // net id's below 0x10 are simulated
      enemies[i]->ttl = 12;
      enemies[i]->in_combat = 0;
      enemies[i]->level = i >=10 ? 10 : i;
      enemies[i]->unlocks = UL_SIMULATED; // this tells app-fight to not to request these peers over the radio
      enemies[i]->hp = maxhp(p_guard, 0, enemies[i]->level ) * 0.85; // start at 85% HP to test

      chsnprintf(tmp, sizeof(tmp), "test%05d",i);
      strncpy(enemies[i]->name, tmp, CONFIG_NAME_MAXLEN);
    }
    osalMutexUnlock(&enemies_mutex);
  }

  chprintf(stream, "Test enemies generated.\r\n");
}
orchard_command("peersim", cmd_peersim);

