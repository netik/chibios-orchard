#include "ch.h"
#include "hal.h"
#include "ext.h"
#include "radio.h"
#include "orchard-events.h"

event_source_t rf_pkt_rdy;
event_source_t radio_app;

/*
 * On the Freescale KW019032 board:
 * Radio pin DIO0 is connected to PORTC pin 3
 * Radio pin DIO1 is connected to PORTC pin 4
 * We want pin 3 since that will trigger on payload ready/tx done events.
 */

static const EXTConfig ext_config = {
  {
    {EXT_CH_MODE_RISING_EDGE | EXT_CH_MODE_AUTOSTART, radioInterrupt, PORTC, 3},
  }
};

void orchardEventsStart(void) {

  chEvtObjectInit(&rf_pkt_rdy);
  chEvtObjectInit(&radio_app);

  extStart(&EXTD1, &ext_config);
}
