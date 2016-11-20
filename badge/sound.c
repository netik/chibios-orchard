/* sound.c - monotonic music for the badge */
#include "ch.h"
#include "hal.h"
#include "tpm.h"
#include "notes.h"
#include "sound.h"

#define note4  400 /* mS per quarter note @ 140bpm */
#define note16 100 /* 16th note */
#define note32 50  /* 32nd note */

static const PWM_NOTE soundStart[] = {
	{ NOTE_FS3, note32 * 3 },
	{ PWM_NOTE_PAUSE, note32 * 2 },
	{ NOTE_CS5, note32 * 2 },
	{ NOTE_D5, note32 * 2 },
	{ NOTE_F4, note32 * 2 },
	{ NOTE_B4, note32 * 5 },
	{ 0, PWM_DURATION_END }
};
    
void playTone(uint16_t freq, uint16_t duration) {
  pwmToneStart(freq);
  chThdSleepMilliseconds (duration);
  pwmToneStop();
}

void playStartupSong(void) {
  /* the galaga level up sound */
  pwmThreadPlay (soundStart);

#ifdef testing
  playTone(NOTE_FS3, note32 * 3);
  chThdSleepMilliseconds(note32 * 2);
  playTone(NOTE_CS5, note32 * 2);
  playTone(NOTE_D5, note32 * 2);
  playTone(NOTE_F4, note32 * 2);
  playTone(NOTE_B4, note32 * 5);
#endif

}

