#include "ch.h"
#include "hal.h"
#include "ext.h"
#include "radio.h"
#include "orchard-events.h"

event_source_t rf_pkt_rdy;
event_source_t gpiox_rdy;
event_source_t celcius_rdy;

#ifdef notdef
event_source_t mic_rdy;
event_source_t usbdet_rdy;
#endif
#if ORCHARD_BOARD_REV != ORCHARD_REV_EVT1
static void gpiox_rdy_cb(EXTDriver *extp, expchannel_t channel) {

  (void)extp;
  (void)channel;

  chSysLockFromISR();
  chEvtBroadcastI(&gpiox_rdy);
  chSysUnlockFromISR();
}
#endif

/*
 * On the Freescale KW019032 board:
 * Radio pin DIO0 is connected to PORTC pin 3
 * Radio pin DIO1 is connected to PORTC pin 4
 * We want pin 3 since that will trigger on payload ready/tx done events.
 */

static const EXTConfig ext_config = {
  {
    {EXT_CH_MODE_RISING_EDGE | EXT_CH_MODE_AUTOSTART, radioInterrupt, PORTC, 3},
#if ORCHARD_BOARD_REV != ORCHARD_REV_EVT1
    {EXT_CH_MODE_FALLING_EDGE | EXT_CH_MODE_AUTOSTART, gpiox_rdy_cb, PORTD, 4},
#endif
  }
};

void orchardEventsStart(void) {

  chEvtObjectInit(&rf_pkt_rdy);
  chEvtObjectInit(&gpiox_rdy);

  // ADC-related events
  chEvtObjectInit(&celcius_rdy);
#ifdef notdef
  chEvtObjectInit(&mic_rdy);
  chEvtObjectInit(&usbdet_rdy);
#endif

  extStart(&EXTD1, &ext_config);
}
