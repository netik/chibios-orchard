#include "ch.h"
#include "hal.h"

#include "orchard-app.h"
#include "led.h"
#include "radio.h"
#include "orchard-ui.h"
#include <string.h>

static uint32_t default_init(OrchardAppContext *context) {
  return 0;
}

static uint32_t default_start(OrchardAppContext *context) {
  return;
}

void default_event(OrchardAppContext *context, const OrchardAppEvent *event) {
}

static void default_exit(OrchardAppContext *context) {
  return;
}

//orchard_app("Main", default_init, default_start, default_event, default_exit);


