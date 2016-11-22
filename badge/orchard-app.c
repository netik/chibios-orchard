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
#include "paging.h"
#include "led.h"
#include "storage.h"
#include "radio.h"

#include "shell.h" // for friend testing function
#include "orchard-shell.h" // for friend testing function

orchard_app_start();
orchard_app_end();

static const OrchardApp *orchard_app_list;
static virtual_timer_t run_launcher_timer;
static bool run_launcher_timer_engaged;
#define RUN_LAUNCHER_TIMEOUT MS2ST(500)

orchard_app_instance instance;  // the one and in fact only instance of any orchard app

#define DWELL_THRESH  500  // time to spend in one state before direction intent is null

event_source_t orchard_app_terminated;
event_source_t orchard_app_terminate;
event_source_t timer_expired;
event_source_t ui_completed;

#define COLLECT_INTERVAL 50  // time to collect events for multi-touch gesture
#define TRACK_INTERVAL 1  // trackpad debounce in ms

static char *friends[MAX_FRIENDS]; // array of pointers to friends' names; first byte is priority metric
#define FRIENDS_INIT_CREDIT  4  // defines how long a friend record stays around before expiration
// max level of credit a friend can have; defines how long a record can stay around
// once a friend goes away. Roughly equal to
// 2 * (PING_MIN_INTERVAL + PING_RAND_INTERVAL / 2 * MAX_CREDIT) milliseconds
#define FRIENDS_MAX_CREDIT   12
#define FRIENDS_SORT_HYSTERESIS 4 

mutex_t friend_mutex;

static uint8_t ui_override = 0;

#define MAIN_MENU_MASK  ((1 << 11) | (1 << 0))
#define MAIN_MENU_VALUE ((1 << 11) | (1 << 0))

static void run_launcher(void *arg) {
  (void)arg;

  chSysLockFromISR();
  /* Launcher is the first app in the list */
  instance.next_app = orchard_app_list;
  instance.thr->p_flags |= CH_FLAG_TERMINATE;
  chEvtBroadcastI(&orchard_app_terminate);
  run_launcher_timer_engaged = false;
  chSysUnlockFromISR();
}

static void ui_complete_cleanup(eventid_t id) {
  (void)id;
  OrchardAppEvent evt;
  
  // unhook the UI patch so key & dial events pass into the app
  instance.ui = NULL;

  evt.type = uiEvent;
  evt.ui.code = uiComplete;
  evt.ui.flags = uiOK;

  instance.app->event(instance.context, &evt);  
}


static void terminate(eventid_t id) {

  (void)id;
  OrchardAppEvent evt;

  if (!instance.app->event)
    return;

  evt.type = appEvent;
  evt.app.event = appTerminate;
  instance.app->event(instance.context, &evt);
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
    instance.app->event(instance.context, &evt);

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
  chThdTerminate(instance.thr);
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
  chVTSet(&context->instance->timer, US2ST(usecs), timer_do_send_message, NULL);
}

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

  chRegSetThreadName("Orchard App");

  evtTableInit(orchard_app_events, 32);
  /*
  evtTableHook(orchard_app_events, ui_completed, ui_complete_cleanup);
  evtTableHook(orchard_app_events, orchard_app_terminate, terminate);
  evtTableHook(orchard_app_events, timer_expired, timer_event);
  */

  // if APP is null, the system will crash here. 
  if (instance->app->init)
    app_context.priv_size = instance->app->init(&app_context);
  else
    app_context.priv_size = 0;
  
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

  //  chVTReset(&run_launcher_timer);
  //  run_launcher_timer_engaged = false;

  /*  evtTableUnhook(orchard_app_events, timer_expired, timer_event);
  evtTableUnhook(orchard_app_events, orchard_app_terminate, terminate);
  evtTableUnhook(orchard_app_events, ui_completed, ui_complete_cleanup);
  */
  /* Atomically broadcasting the event source and terminating the thread,
     there is not a chSysUnlock() because the thread terminates upon return.*/
  chSysLock();
  chEvtBroadcastI(&orchard_app_terminated);
  chThdExitS(MSG_OK);
}

void orchardAppInit(void) {
  int i;

  orchard_app_list = orchard_apps();
  instance.app = orchard_app_list;
  chEvtObjectInit(&orchard_app_terminated);
  chEvtObjectInit(&orchard_app_terminate);
  chEvtObjectInit(&timer_expired);
  chEvtObjectInit(&ui_completed);
  chVTReset(&instance.timer);

  /* Hook this outside of the app-specific runloop, so it runs even if
     the app isn't listening for events.*/
  //  evtTableHook(orchard_events, captouch_changed, poke_run_launcher_timer);

  // usb detection and charge state management is also meta to the apps
  // sequence of events:
  // 0. timer chargecheck_timer is set for CHARGECHECK_INTERVAL
  // 1. timer chargecheck_timer times out, and run_chargecheck callback is executed
  // 2. run_chargecheck callback issues a chargecheck_timeout event and re-schedules itself
  // 3. event sytsem receives chargecheck_timeout event and  dispatches handle_chargecheck_timeout
  // 4. handle_chargecheck_timeout issues an analogUpdateUsbStatus() call and exits
  // 5. analogUpdateUsbStatus() eventually results in a usbdet_rdy event
  // 6. usbdet_rdy event dispatches into the handle_charge_state event handler
  // 7. handle_charge_state runs all the logic for managing charge state

  // Steps 0-7 create a periodic timer that polls the USB D+/D- pin state so we can
  // make a determination of how to correctly set the charger/boost state
  // It's complicated because both the timer and the D+/D- detetion are asynchronous
  // and you have to use events to poke operations that can't happen in interrupt contexts!
  //  evtTableHook(orchard_events, usbdet_rdy, handle_charge_state);
  //  evtTableHook(orchard_events, chargecheck_timeout, handle_chargecheck_timeout);

  //  evtTableHook(orchard_events, radio_page, handle_radio_page);
  //  evtTableHook(orchard_events, ping_timeout, handle_ping_timeout);

  // these calls break the system, don't do them yet. 
  //  chVTReset(&ping_timer);
  //  chVTSet(&ping_timer, MS2ST(PING_MIN_INTERVAL + rand() % PING_RAND_INTERVAL), run_ping, NULL);

  for( i = 0; i < MAX_FRIENDS; i++ ) {
    friends[i] = NULL;
  }
  osalMutexObjectInit(&friend_mutex);
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
