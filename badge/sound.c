/* sound.c - monotonic music for the badge */
#include "ch.h"
#include "hal.h"
#include "tpm.h"
#include "notes.h"
#include "sound.h"

#define note4  400 /* mS per quarter note @ 140bpm */
#define note16 100 /* 16th note */
#define note32 50  /* 32nd note */

static const PWM_NOTE soundGalaga[] = {
  { 392, 288 },
  { 523, 144 },
  { 587, 288 },
  { 698, 144 },
  { 659, 288 },
  { 523, 144 },
  { 587, 288 },
  { 880, 144 },
  { 784, 288 },
  { 523, 144 },
  { 587, 288 },
  { 698, 144 },
  { 659, 288 },
  { 523, 144 },
  { 784, 288 },
  { 988, 144 },
  { 1047, 288 },
  { 932, 144 },
  { 831, 288 },
  { 784, 144 },
  { 698, 288 },
  { 622, 144 },
  { 587, 288 },
  { 466, 144 },
  { 932, 288 },
  { 1047, 144 },
  { 932, 288 },
  { 784, 144 },
  { 880, 144 },
  { 698, 144 },
  { 587, 144 },
  { 784, 144 },
  { 659, 144 },
  { 587, 144 },
  { 0, PWM_DURATION_END }
};

#ifdef notdef
static const PWM_NOTE soundStart[] = {
  { NOTE_FS3, note32 * 3 },
  { PWM_NOTE_PAUSE, note32 * 2 },
  { NOTE_CS5, note32 * 2 },
  { NOTE_D5, note32 * 2 },
  { NOTE_A4, note32 * 2 },
  { NOTE_FS5, note32 * 5 },
  { 0, PWM_DURATION_END }
};
#endif

static const PWM_NOTE soundSadPanda[] = {
  /* dissonant failure sound */
  { NOTE_GS3, note16 },
  { NOTE_F3, note16 },
  { NOTE_E3, note4 },
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundAttacked[] = {
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_A6, note16 },
  { NOTE_A7, note16 },  
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundVictory[] = {
  { NOTE_D4, note16 },
  { NOTE_FS4, note16 },
  { NOTE_A4, note16 },
  { NOTE_D5, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_D4, note16 * 2 },
  { NOTE_D5, note16 * 3 },
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundDodge[] = {
  { 0, PWM_DURATION_END }
};

static const PWM_NOTE soundDefeat[] = {
  { 0, PWM_DURATION_END }
};

void playTone(uint16_t freq, uint16_t duration) {
  pwmToneStart(freq);
  chThdSleepMilliseconds (duration);
  pwmToneStop();
}

void playStartupSong(void) {
  /* the galaga game start sound */
  pwmThreadPlay (soundGalaga);
}

void playHardFail(void) {
  /* sad panda tone */
  pwmThreadPlay (soundSadPanda);
}

void playAttacked(void) {
  /* played when you're getting attacked by another player */
  pwmThreadPlay (soundAttacked);
}

void playVictory(void) {
  /* played when you win */
  pwmThreadPlay (soundVictory);
}

void playDefeat(void) {
  /* played when you lose */
  pwmThreadPlay (soundDefeat);
}

void playDodge(void) {
  /* played when you dodge an attack */
  pwmThreadPlay (soundDodge);
}

void playHit(void) {
  /* played when you're hit */
  
}
