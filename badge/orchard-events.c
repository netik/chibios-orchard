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
event_source_t joy_rdy;

/* Default state is all buttons pressed. */

uint8_t joyState = 0xFF;

static void
joyInterrupt (EXTDriver *extp, expchannel_t channel)
{
	(void)extp;
	(void)channel;

	chSysLockFromISR ();
	chEvtBroadcastI (&joy_rdy);
	chSysUnlockFromISR ();

        return;
}

static void
joyIntrHandle (eventid_t id)
{
	uint8_t pad;

	(void)id;

	pad = palReadPad (BUTTON_ENTER_PORT, BUTTON_ENTER_PIN);

	pad <<= JOY_ENTER_SHIFT;

	if (pad ^ (joyState & JOY_ENTER)) {
		joyState &= ~JOY_ENTER;
		joyState |= pad;
		if (pad) {
			playDoh ();
			chprintf (stream, "Enter released!\r\n", pad);
		} else {
			playWoohoo ();
			chprintf (stream, "Enter pressed!\r\n", pad);
		}
	}

	pad = palReadPad (BUTTON_UP_PORT, BUTTON_UP_PIN);

	pad <<= JOY_UP_SHIFT;

	if (pad ^ (joyState & JOY_UP)) {
		joyState &= ~JOY_UP;
		joyState |= pad;
		if (pad) {
			playDoh ();
			chprintf (stream, "Up released!\r\n", pad);
		} else {
			playWoohoo ();
			chprintf (stream, "Up pressed!\r\n", pad);
		}
	}

	pad = palReadPad (BUTTON_DOWN_PORT, BUTTON_DOWN_PIN);

	pad <<= JOY_DOWN_SHIFT;

	if (pad ^ (joyState & JOY_DOWN)) {
		if (pad) {
			playDoh ();
			chprintf (stream, "Down released!\r\n", pad);
		} else {
			playWoohoo ();
			chprintf (stream, "Down pressed!\r\n", pad);
		}
		joyState &= ~JOY_DOWN;
		joyState |= pad;
	}

	pad = palReadPad (BUTTON_LEFT_PORT, BUTTON_LEFT_PIN);

	pad <<= JOY_LEFT_SHIFT;

	if (pad ^ (joyState & JOY_LEFT)) {
		joyState &= ~JOY_LEFT;
		joyState |= pad;
		if (pad) {
			playDoh ();
			chprintf (stream, "Left released!\r\n", pad);
		} else {
			playWoohoo ();
			chprintf (stream, "Left pressed!\r\n", pad);
		}
	}

	pad = palReadPad (BUTTON_RIGHT_PORT, BUTTON_RIGHT_PIN);

	pad <<= JOY_RIGHT_SHIFT;

	if (pad ^ (joyState & JOY_RIGHT)) {
		joyState &= ~JOY_RIGHT;
		joyState |= pad;
		if (pad) {
			playDoh ();
			chprintf (stream, "Right released!\r\n", pad);
		} else {
			playWoohoo ();
			chprintf (stream, "Right pressed!\r\n", pad);
		}
	}

	pad = palReadPad (TILT_SENSOR_PORT, TILT_SENSOR_PIN);

	pad <<= JOY_TILT_SHIFT;

	if (pad ^ (joyState & JOY_TILT)) {
		joyState &= ~JOY_TILT;
		joyState |= pad;
		if (pad) {
			playDoh ();
			chprintf (stream, "Rightside up!\r\n", pad);
		} else {
			playWoohoo ();
			chprintf (stream, "Upside down!\r\n", pad);
		}
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
  chEvtObjectInit(&joy_rdy);

  extStart(&EXTD1, &ext_config);

  evtTableHook (orchard_events, joy_rdy, joyIntrHandle);
}
