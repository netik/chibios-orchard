#include "ch.h"
#include "hal.h"
#include "ext.h"
#include "radio_lld.h"
#include "orchard.h"
#include "orchard-events.h"
#include "chprintf.h"

#include "sound.h"

extern void * stream;

event_source_t rf_pkt_rdy;
event_source_t tilt_rdy;

static void
tiltInterrupt (EXTDriver *extp, expchannel_t channel)
{
	(void)extp;
	(void)channel;

	chSysLockFromISR ();
	chEvtBroadcastI (&tilt_rdy);
	chSysUnlockFromISR ();

        return;
}

static void
tiltIntrHandle (eventid_t id)
{
	uint8_t pad;

	(void)id;

	pad = palReadPad (GPIOA, 4);

	if (pad == 1) {
		playDoh ();
		chprintf (stream, "Released!\r\n", pad);
	} else if (pad == 0) {
		playWoohoo ();
		chprintf (stream, "Pressed!\r\n", pad);
	}

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
    {EXT_CH_MODE_BOTH_EDGES | EXT_CH_MODE_AUTOSTART, tiltInterrupt, PORTA, 4},
  }
};

void orchardEventsStart(void) {

  chEvtObjectInit(&rf_pkt_rdy);
  chEvtObjectInit(&tilt_rdy);

  extStart(&EXTD1, &ext_config);

  evtTableHook (orchard_events, tilt_rdy, tiltIntrHandle);
}
