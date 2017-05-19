#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "orchard.h"
#include "orchard-events.h"
#include "orchard-app.h"
#include "orchard-ui.h"
#include "led.h"
#include "dac_lld.h"
#include "radio_lld.h"

#include "shell.h" // for enemy testing function
#include "orchard-shell.h" // for enemy testing function
#include "userconfig.h"

orchard_app_start();
orchard_app_end();

const OrchardApp *orchard_app_list;

/* the one and in fact only instance of any orchard app */
orchard_app_instance instance;  

/* graphics event handle */
static OrchardAppEvent ugfx_evt;

/* Joypad event handle */
static OrchardAppKeyEvent joyEvent;

/* orchard event sources */
event_source_t orchard_app_terminated;
event_source_t orchard_app_terminate;
event_source_t timer_expired;
event_source_t ui_completed;
event_source_t orchard_app_gfx;
event_source_t orchard_app_radio;
event_source_t orchard_app_key;

/* Default state is all buttons pressed. */

static uint8_t joyState = 0xFF;

static const joyInfo joyTbl[6] = {
  { BUTTON_ENTER_PORT, BUTTON_ENTER_PIN, JOY_ENTER, keySelect },
  { BUTTON_UP_PORT, BUTTON_UP_PIN, JOY_UP, keyUp },
  { BUTTON_DOWN_PORT, BUTTON_DOWN_PIN, JOY_DOWN, keyDown },
  { BUTTON_LEFT_PORT, BUTTON_LEFT_PIN, JOY_LEFT, keyLeft },
  { BUTTON_RIGHT_PORT, BUTTON_RIGHT_PIN, JOY_RIGHT, keyRight },
  { TILT_SENSOR_PORT, TILT_SENSOR_PIN, JOY_TILT, keyTilt }
};

/* Enemy ping/pong handling ------------------------------------------------*/

static virtual_timer_t ping_timer;
static event_source_t ping_timeout;  // fires when ping_timer is ping'd
static uint8_t cleanup_state = 0;    // cleans up every other ping
void enemy_cleanup(void);            // reaps the list every other ping
/* end ping handling */

// lock/unlock this mutex before touching the enemies list!
mutex_t enemies_mutex;
static peer *enemies[MAX_ENEMIES]; // array of pointers to enemy structures
/* END Enemy ping/pong handling --------------------------------------------*/

static uint8_t ui_override = 0;

/* app based globals */
uint8_t shout_received;
uint8_t shout_ok;

/* this is shared between app-badge and app-unlock */
/* engaging a temporary class change like bender or caesar will set
   it, and it will be cleared on reset */
systime_t char_reset_at = 0;

static KW01_PKT orchard_pkt;
uint8_t orchard_pkt_busy;

static void run_ping(void *arg) {
  (void)arg;
  chSysLockFromISR();
  chEvtBroadcastI(&ping_timeout);
  chVTSetI(&ping_timer,
    MS2ST(PING_MIN_INTERVAL + rand() % PING_RAND_INTERVAL), run_ping, NULL);
  chSysUnlockFromISR(); 
}

uint8_t nearby_caesar(void) {
  uint8_t result = FALSE; 
  uint8_t i;
  /* returns true if any caesars are nearby */
  osalMutexLock(&enemies_mutex);
  for( i = 0; i < MAX_ENEMIES; i++ ) {
    if( enemies[i] == NULL )
      continue;
    if (enemies[i]->current_type == p_caesar) { 
      result = TRUE;
    }
  }
  osalMutexUnlock(&enemies_mutex);

  return result;
};


void enemy_cleanup(void) {
  /* Called periodically to decrement credits and de-alloc enemies
   * we haven't seen in a while 
   */
  uint32_t i;

  osalMutexLock(&enemies_mutex);
  for( i = 0; i < MAX_ENEMIES; i++ ) {
    if( enemies[i] == NULL )
      continue;

    enemies[i]->ttl--;
    if( enemies[i]->ttl == 0 ) {
      chHeapFree(enemies[i]);
      enemies[i] = NULL;
    }
  }
  osalMutexUnlock(&enemies_mutex);
}

static void execute_ping(eventid_t id) {

#ifndef LEADERBOARD_AGENT
  userconfig * config;
  peer upkt;
  unsigned long clockdelta;
  config = getConfig();
#endif
  
  (void) id;
  
#ifndef LEADERBOARD_AGENT
  if (config->airplane_mode) {
    /* we do not send pings in airplane mode */
    return;
  }
  
  /* time to send a ping. */
  /* build packet */
  memset (&upkt, 0, sizeof(peer));
  
  upkt.netid = config->netid;

  strncpy(upkt.name, config->name, CONFIG_NAME_MAXLEN);

  // copy the user data into the packet for transmission
  upkt.current_type = config->current_type;
  upkt.p_type    = config->p_type;
  upkt.in_combat = config->in_combat;
  upkt.hp        = config->hp;
  upkt.level     = config->level;
  upkt.won       = config->won;
  upkt.lost      = config->lost;
  upkt.xp        = config->xp;
  upkt.agl       = config->agl;
  upkt.might     = config->might;
  upkt.luck      = config->luck;

  // if we have the clock, send the current clock.
  if (rtc != 0) {
    clockdelta = ST2S((chVTGetSystemTime() - rtc_set_at));
    upkt.rtc = rtc + clockdelta;
  }
  
  radioSend (&KRADIO1, RADIO_BROADCAST_ADDRESS,  RADIO_PROTOCOL_PING,
	     sizeof (upkt), &upkt);

#endif /* LEADERBOARD_AGENT */
  // while we're at it, clean up the enemy list every two pings
  if( cleanup_state ) {
    enemy_cleanup();
  }
  cleanup_state = !cleanup_state;
  
}

void orchardAppUgfxCallback (void * arg, GEvent * pe)
{
  GListener * gl;

  gl = (GListener *)arg;

  ugfx_evt.type = ugfxEvent;
  ugfx_evt.ugfx.pListener = gl;
  ugfx_evt.ugfx.pEvent = pe;

  chEvtBroadcast (&orchard_app_gfx);

  return;
}

static void ugfx_event(eventid_t id) {

  (void) id;

  dacPlay ("click.raw");
  instance.app->event (instance.context, &ugfx_evt);
  geventEventComplete (ugfx_evt.ugfx.pListener);

  return;
}

void orchardAppRadioCallback (KW01_PKT * pkt) {

  if (instance.context == NULL)
    return;

  /*
   * We buffer the current frame. If another frame arrives before
   * the app has processed it, then we drop the new frame. We could
   * expand this to permit buffering of more frames, but for now
   * this seems sufficient.
   */
  if (orchard_pkt_busy == 0) {
    memcpy (&orchard_pkt, pkt, sizeof(KW01_PKT));
    orchard_pkt_busy++;
  } else
    return;

  chEvtBroadcast (&orchard_app_radio);

  return;
}

static void radio_event(eventid_t id) {
  OrchardAppEvent evt;

  (void) id;

  if (instance.context != NULL) {
    evt.type = radioEvent;
    evt.radio.pPkt = &orchard_pkt;

    instance.app->event (instance.context, &evt);
    orchard_pkt_busy--;
  }

  return;
}

static uint8_t
joyHandle (uint8_t s) {
  OrchardAppEvent evt;
  uint8_t pad;

  pad = palReadPad (joyTbl[s].port, joyTbl[s].pin);

  pad <<= s;

  if (pad ^ (joyState & joyTbl[s].bit)) {
    joyState &= ~joyTbl[s].bit;
    joyState |= pad;
    joyEvent.code = joyTbl[s].code;
    if (pad)
      joyEvent.flags = keyRelease;
    else
      joyEvent.flags = keyPress;
  
    if (instance.context != NULL) {
      evt.type = keyEvent;
      evt.key = joyEvent;
      if (joyEvent.code != keyTilt && joyEvent.flags == keyPress)
          dacPlay("click.raw");
      instance.app->event (instance.context, &evt);
    }
    return (1);
  }

  return (0);
}

static void key_event(eventid_t id) {

  (void) id;

  if (joyHandle (JOY_ENTER_SHIFT))
    return;

#ifdef ENABLE_JOYPAD
  if (joyHandle (JOY_UP_SHIFT))
    return;
  if (joyHandle (JOY_DOWN_SHIFT))
    return;
  if (joyHandle (JOY_LEFT_SHIFT))
    return;
  if (joyHandle (JOY_RIGHT_SHIFT))
    return;
#endif

#ifdef ENABLE_TILT_SENSOR
  if (joyHandle (JOY_TILT_SHIFT))
    return;
#endif

  return;
}
static void ui_complete_cleanup(eventid_t id) {
  (void)id;
  OrchardAppEvent evt;
 
  evt.type = uiEvent;
  evt.ui.code = uiComplete;
  evt.ui.flags = uiOK;

  instance.app->event(instance.context, &evt);  

  return;
}

static void terminate(eventid_t id) {
  OrchardAppEvent evt;

  (void)id;

  if (!instance.app->event)
    return;

  evt.type = appEvent;
  evt.app.event = appTerminate;
  instance.app->event(instance.context, &evt);

  orchard_pkt_busy = 0;

  chThdTerminate(instance.thr);
}

static void timer_event(eventid_t id) {

  (void)id;
  OrchardAppEvent evt;

  if (!instance.app->event)
    return;

  evt.type = timerEvent;
  evt.timer.usecs = instance.timer_usecs;
  if( !ui_override )
    {
    instance.app->event(instance.context, &evt);
    }

  if (instance.timer_repeating)
    orchardAppTimer(instance.context, instance.timer_usecs, true);
}

static void timer_do_send_message(void *arg) {

  (void)arg;
  chSysLockFromISR();
  chEvtBroadcastI(&timer_expired);
  chSysUnlockFromISR();
}

const OrchardApp *orchardAppByName(const char *name) {
  const OrchardApp *current;

  current = orchard_apps();
  while(current->name) {
    if( !strncmp(name, current->name, 16) ) {
      return current;
    }
    current++;
  }
  return NULL;
}

void orchardAppRun(const OrchardApp *app) {
  instance.next_app = app;
  chThdTerminate(instance.thr);
  chEvtBroadcast(&orchard_app_terminate);
}

void orchardAppExit(void) {
  instance.next_app = orchard_app_list;  // the first app is the launcher

  /*
   * The call to terminate the app thread below seems to be wrong. The
   * terminate() handler will be invoked by broadcasting the app
   * terminate event, and it already calls chThdTerminate() once it sends
   * the app terminate event to the app's event handler, and that can't
   * happen if the thread has already exited from here.
   */
  /*chThdTerminate(instance.thr);*/

  chEvtBroadcast(&orchard_app_terminate);
}

void orchardAppTimer(const OrchardAppContext *context,
                     uint32_t usecs,
                     bool repeating) {

  if (!usecs) {
    chVTReset(&context->instance->timer);
    context->instance->timer_usecs = 0;
    return;
  }

  context->instance->timer_usecs = usecs;
  context->instance->timer_repeating = repeating;

  /*
   * The US2ST() macro used by ChibiOS does not work with
   * large microsecond counts when the system tick resolution is
   * fairly high. With a resolutio of 10000, the US2ST() macro
   * macro wraps at 429496. So for values above this, we need to
   * base the calculation on milliseconds instead.
   */

  if (usecs > 429496)
    chVTSet(&context->instance->timer, MS2ST(usecs / 1000),
      timer_do_send_message, NULL);
  else
    chVTSet(&context->instance->timer, US2ST(usecs),
      timer_do_send_message, NULL);

  return;
}

// jna: second argument here is stack space allocated to the application thread.
// 0x300 = 768  = Breaks app-fight.c badly
// 0x400 = 1024 = Works fine with app-fight.c.
//
// Please don't modify this unless absolutely necessary. I will figure
// out how to reduce stack usage in app-fight.c 

static THD_WORKING_AREA(waOrchardAppThread, 0x400);
static THD_FUNCTION(orchard_app_thread, arg) {

  (void)arg;
  struct orchard_app_instance *instance = arg;
  struct evt_table orchard_app_events;
  OrchardAppContext app_context;

  ui_override = 0;
  memset(&app_context, 0, sizeof(app_context));
  instance->context = &app_context;
  app_context.instance = instance;
  
  // set UI elements to null
  instance->ui = NULL;
  instance->uicontext = NULL;
  instance->ui_result = 0;

  evtTableInit(orchard_app_events, 6);
  evtTableHook(orchard_app_events, ui_completed, ui_complete_cleanup);
  evtTableHook(orchard_app_events, orchard_app_terminate, terminate);
  evtTableHook(orchard_app_events, orchard_app_gfx, ugfx_event);
  evtTableHook(orchard_app_events, orchard_app_radio, radio_event);
  evtTableHook(orchard_app_events, orchard_app_key, key_event);
  evtTableHook(orchard_app_events, timer_expired, timer_event);

  // if APP is null, the system will crash here. 
  if (instance->app->init)
    app_context.priv_size = instance->app->init(&app_context);
  else
    app_context.priv_size = 0;

  chRegSetThreadName(instance->app->name);

  /* Allocate private data on the stack (word-aligned) */
  uint32_t priv_data[app_context.priv_size / 4];

  if (app_context.priv_size) {
    memset(priv_data, 0, sizeof(priv_data));
    app_context.priv = priv_data;
  } else {
    app_context.priv = NULL;
  }

  if (instance->app->start)
    instance->app->start(&app_context);
 
  if (instance->app->event) {
    {
      OrchardAppEvent evt;
      evt.type = appEvent;
      evt.app.event = appStart;
      instance->app->event(instance->context, &evt);
    }
    while (!chThdShouldTerminateX())
      chEvtDispatch(evtHandlers(orchard_app_events), chEvtWaitOne(ALL_EVENTS));
  }

  chVTReset(&instance->timer);

  if (instance->app->exit)
    instance->app->exit(&app_context);

  instance->context = NULL;

  /* Set up the next app to run when the orchard_app_terminated message is
     acted upon.*/
  if (instance->next_app)
    instance->app = instance->next_app;
  else
    instance->app = orchard_app_list;
  instance->next_app = NULL;

  evtTableUnhook(orchard_app_events, timer_expired, timer_event);
  evtTableUnhook(orchard_app_events, orchard_app_terminate, terminate);
  evtTableUnhook(orchard_app_events, ui_completed, ui_complete_cleanup);
  evtTableUnhook(orchard_app_events, orchard_app_gfx, ugfx_event);
  evtTableUnhook(orchard_app_events, orchard_app_radio, radio_event);
  evtTableUnhook(orchard_app_events, orchard_app_key, key_event);
 
  orchard_pkt_busy = 0;

  /* Atomically broadcasting the event source and terminating the thread,
     there is not a chSysUnlock() because the thread terminates upon return.*/
  chSysLock();
  chEvtBroadcastI(&orchard_app_terminated);
  chThdExitS(MSG_OK);
}

void orchardAppInit(void) {
  const OrchardApp * current;

  orchard_app_list = orchard_apps();
  instance.app = orchard_app_list;
  chEvtObjectInit(&orchard_app_terminated);
  chEvtObjectInit(&orchard_app_terminate);
  chEvtObjectInit(&timer_expired);
  chEvtObjectInit(&ping_timeout);
  chEvtObjectInit(&ui_completed);
  chEvtObjectInit(&orchard_app_gfx);
  chEvtObjectInit(&orchard_app_radio);
  chEvtObjectInit(&orchard_app_key);
  chVTReset(&instance.timer);

  // set up our ping timer
  chVTReset(&ping_timer);
  //  evtTableHook(orchard_events, radio_page, handle_radio_page);
  evtTableHook(orchard_events, ping_timeout, execute_ping);

  chVTSet(&ping_timer,
    MS2ST(PING_MIN_INTERVAL + rand() % PING_RAND_INTERVAL), run_ping, NULL);

  // initalize the seen-enemies list
  for( int i = 0; i < MAX_ENEMIES; i++ ) {
    enemies[i] = NULL;
  }
  osalMutexObjectInit(&enemies_mutex);

  current = orchard_app_list;
  while (current->name) {
    if (current->flags & APP_FLAG_AUTOINIT)
      current->init(NULL);
    current++;
  }

}

// enemy handling routines
uint8_t enemyCount(void) {
  int i;
  uint8_t count = 0;
  
  osalMutexLock(&enemies_mutex);
  for( i = 0; i < MAX_ENEMIES; i++ ) {
    if ( ( enemies[i] != NULL ) &&
         ( enemies[i]->in_combat == 0) &&
         ( enemies[i]->p_type != p_notset) )
      count++;
  }
  osalMutexUnlock(&enemies_mutex);
  return count;
}

peer *enemy_lookup(peer *u) {
  // return an enemy record by name (bad?)
  osalMutexLock(&enemies_mutex);

  for( int i = 0; i < MAX_ENEMIES; i++ ) {
    if (enemies[i] != NULL) {
      if (enemies[i]->netid == u->netid) { 
	osalMutexUnlock(&enemies_mutex);
	return enemies[i];
      }
    }
  }

  osalMutexUnlock(&enemies_mutex);
  return NULL;
}

peer *enemyAdd(peer *u) {
  peer *record;
  uint32_t i;
  uint32_t oldttl;
  record = enemy_lookup(u);

  if( record != NULL ) {
    /* update name and stats */
    oldttl = record->ttl;
    if(record->ttl < PEER_MAX_TTL) {
      oldttl++;
    }

    osalMutexLock(&enemies_mutex);
    memcpy(record, u, sizeof(peer));
    record->ttl = oldttl;
    osalMutexUnlock(&enemies_mutex);
    return record;  // enemy already exists, don't add it again
  }

  osalMutexLock(&enemies_mutex);

  for( i = 0; i < MAX_ENEMIES; i++ ) {
    if( enemies[i] == NULL ) {
      enemies[i] = (peer *) chHeapAlloc(NULL, sizeof(peer));
      memcpy(enemies[i], u, sizeof(peer));
      enemies[i]->ttl = PEER_TTL_INITIAL;
      osalMutexUnlock(&enemies_mutex);
      return enemies[i];
    }
  }
  osalMutexUnlock(&enemies_mutex);

  // if we got here, we couldn't add the enemy because we ran out of space
  return NULL;
}

int peer_comp(const void *a, const void *b) {
  char *mya;
  char *myb;
  
  mya = ((peer *)a)->name;
  myb = ((peer *)b)->name;

  if( mya == NULL && myb == NULL )
    return 0;

  if( mya == NULL )
    return 1;

  if( myb == NULL )
    return -1;

  return strncmp(mya, myb, CONFIG_NAME_MAXLEN );
}

void enemiesSort(void) {
  osalMutexLock(&enemies_mutex);
  qsort(enemies, MAX_ENEMIES, sizeof(char *), peer_comp);
  osalMutexUnlock(&enemies_mutex);
}

void enemiesLock(void) {
  osalMutexLock(&enemies_mutex);
}

void enemiesUnlock(void) {
  osalMutexUnlock(&enemies_mutex);
}

peer **enemiesGet(void) {
  return (peer **) enemies;   // you shouldn't modify this record outside of here, hence const
}

void orchardAppRestart(void) {

  /* Recovers memory of the previous application. */
  if (instance.thr) {
    osalDbgAssert(chThdTerminatedX(instance.thr), "App thread still running");
    chThdRelease(instance.thr);
    instance.thr = NULL;
  }

  instance.thr = chThdCreateStatic(waOrchardAppThread,
                                   sizeof(waOrchardAppThread),
                                   ORCHARD_APP_PRIO,
                                   orchard_app_thread,
                                   (void *)&instance);
}
