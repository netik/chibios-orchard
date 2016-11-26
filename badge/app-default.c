#include "ch.h"
#include "hal.h"

#include "orchard-app.h"
#include "led.h"
#include "radio.h"
#include "orchard-ui.h"
#include <string.h>

static void redraw_badge(void) {
  // draw the entire background badge
  gdispClear(Black);

}

static uint32_t default_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static uint32_t default_start(OrchardAppContext *context) {
  redraw_badge();
  return 0;
}

void default_event(OrchardAppContext *context, const OrchardAppEvent *event) {

}

static void default_exit(OrchardAppContext *context) {
  return;
}

orchard_app("Badge", default_init, default_start, default_event, default_exit);


