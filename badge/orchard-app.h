#ifndef __ORCHARD_APP_H__
#define __ORCHARD_APP_H__

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "shell.h"
#include "orchard.h"
#include "gfx.h"
#include "orchard-ui.h"
#include "orchard-events.h"
#include "userconfig.h"

#define APP_FLAG_HIDDEN		0x00000001
#define APP_FLAG_AUTOINIT	0x00000002

#define PING_MIN_INTERVAL  3000 // base time between pings
// defines how long a enemy record stays around before expiration
// max level of credit a enemy can have; defines how long a record can stay around
// once a enemy goes away. Roughly equal to
// 2 * (PING_MIN_INTERVAL + PING_RAND_INTERVAL / 2 * MAX_CREDIT) milliseconds
#define ENEMIES_TTL_INITIAL  4
#define ENEMIES_TTL_MAX  12
#define ENEMIES_SORT_HYSTERESIS 4

struct _OrchardApp;
typedef struct _OrchardApp OrchardApp;
struct _OrchardAppContext;
typedef struct _OrchardAppContext OrchardAppContext;
struct orchard_app_instance;

/* Emitted to an app when it's about to be terminated */
extern event_source_t orchard_app_terminate;

/* Emitted to the system after the app has terminated */
extern event_source_t orchard_app_terminated;

// Emitted to the system after a UI dialog is completed
extern event_source_t ui_completed;

extern void orchardAppInit(void);
extern void orchardAppRestart(void);
extern void orchardAppWatchdog(void);
extern const OrchardApp *orchardAppByName(const char *name);
extern void orchardAppRun(const OrchardApp *app);
extern void orchardAppExit(void);
extern void orchardAppTimer(const OrchardAppContext *context,
                     uint32_t usecs,
                     bool repeating);
extern void orchardAppUgfxCallback (void * arg, GEvent * pe);
extern void orchardAppRadioCallback (KW01_PKT * pkt);

void enemiesSort(void);
void enemiesLock(void);
void enemiesUnlock(void);
uint8_t enemyCount(void);
user *enemyAdd(user *u);
user **enemiesGet(void);

#define MAX_ENEMIES  16   // max # of enemies to track
#define UI_IDLE_TIME 8000 // after 8 seconds, abort to main

typedef struct _OrchardAppContext {
  struct orchard_app_instance *instance;
  uint32_t                    priv_size;
  void                        *priv;
} OrchardAppContext;

typedef struct _OrchardApp {
  char *name;
  uint32_t flags;
  uint32_t (*init)(OrchardAppContext *context);
  void (*start)(OrchardAppContext *context);
  void (*event)(OrchardAppContext *context, const OrchardAppEvent *event);
  void (*exit)(OrchardAppContext *context);
} OrchardApp;

typedef struct orchard_app_instance {
  const OrchardApp      *app;
  const OrchardApp      *next_app;
  OrchardAppContext     *context;
  thread_t              *thr;
  uint32_t              keymask;
  virtual_timer_t       timer;
  uint32_t              timer_usecs;
  bool                  timer_repeating;
  const OrchardUi       *ui;
  OrchardUiContext      *uicontext;
  uint32_t              ui_result;
} orchard_app_instance;


#define orchard_app_start()                                                   \
  static char start[4] __attribute__((unused,                                 \
    aligned(4), section(".chibi_list_app_1")));

#define orchard_apps() (const OrchardApp *)&start[4]

#define orchard_app(_name, _flags, _init, _start, _event, _exit)                        \
  const OrchardApp _orchard_app_list_##_init##_start##_event##_exit           \
  __attribute__((used, aligned(4), section(".chibi_list_app_2_" # _event # _start # _init # _exit))) =  \
     { _name, _flags, _init, _start, _event, _exit }

#define orchard_app_end()                                                     \
  const OrchardApp _orchard_app_list_final                                    \
  __attribute__((used, aligned(4), section(".chibi_list_app_3_end"))) =     \
     { NULL, 0, NULL, NULL, NULL, NULL }

#define ORCHARD_APP_PRIO (LOWPRIO + 2)


#endif /* __ORCHARD_APP_H__ */
