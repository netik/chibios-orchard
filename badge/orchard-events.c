#include "ch.h"
#include "hal.h"
#include "ext.h"
#include "radio_lld.h"
#include "orchard.h"
#include "orchard-events.h"
#include "orchard-app.h"
#include "chprintf.h"

#include "sound.h"

#include <string.h>

extern void * stream;

event_source_t rf_pkt_rdy;
extern event_source_t orchard_app_key;
extern orchard_app_instance instance;  

static void
joyInterrupt (EXTDriver *extp, expchannel_t channel)
{
	(void)extp;
	(void)channel;

	chSysLockFromISR ();
	if (instance.app != NULL)
		chEvtBroadcastI (&orchard_app_key);
	chSysUnlockFromISR ();

        return;
}

/*
 * On the Freescale KW019032 board:
 * Radio pin DIO0 is connected to PORTC pin 3
 * Radio pin DIO1 is connected to PORTC pin 4
 * We want pin 3 since that will trigger on payload ready/tx done events.
 */

static const EXTConfig ext_config = {
  {
    {EXT_CH_MODE_RISING_EDGE | EXT_CH_MODE_AUTOSTART, radioInterrupt, PORTC, 3},

    {EXT_CH_MODE_BOTH_EDGES | EXT_CH_MODE_AUTOSTART, joyInterrupt, PORTA, 4},
#ifdef ENABLE_JOYPAD
    {EXT_CH_MODE_BOTH_EDGES | EXT_CH_MODE_AUTOSTART, joyInterrupt, PORTA, 19},
    {EXT_CH_MODE_BOTH_EDGES | EXT_CH_MODE_AUTOSTART, joyInterrupt, PORTC, 1},
    {EXT_CH_MODE_BOTH_EDGES | EXT_CH_MODE_AUTOSTART, joyInterrupt, PORTC, 2},
    {EXT_CH_MODE_BOTH_EDGES | EXT_CH_MODE_AUTOSTART, joyInterrupt, PORTC, 4},
#endif
#ifdef ENABLE_TILT_SENSOR
    {EXT_CH_MODE_BOTH_EDGES | EXT_CH_MODE_AUTOSTART, joyInterrupt, PORTD, 6},
#endif
  }
};

void orchardEventsStart(void) {

  chEvtObjectInit(&rf_pkt_rdy);

  extStart(&EXTD1, &ext_config);
}
