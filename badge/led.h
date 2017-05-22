#ifndef __LED_H__
#define __LED_H__

#include "hal.h"
#include "math.h"

#define LEDS_COUNT 12

/* starting from 1, not 0!  */
#define LED_PATTERNS_LIMITED 11 // most but not all patterns
#define LED_PATTERNS_FULL    16 // all if unlocked.

#define sign(x) (( x > 0 ) - ( x < 0 ))

typedef struct Color Color;
struct Color {
  uint8_t g;
  uint8_t r;
  uint8_t b;
};

typedef struct HsvColor {
  uint8_t h;
  uint8_t s;
  uint8_t v;
} HsvColor;

typedef struct RgbColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} RgbColor;

RgbColor HsvToRgb(HsvColor hsv);
HsvColor RgbToHsv(RgbColor rgb);

void ledStart(uint32_t leds, uint8_t *o_fb);

void effectsStart(void);
uint8_t effectsStop(void);

uint8_t effectsNameLookup(const char *name);
void effectsSetPattern(uint8_t);
uint8_t effectsGetPattern(void);
void effectsNextPattern(void);
void effectsPrevPattern(void);

void listEffects(void);
void ledResetPattern(void);
void ledSetBrightness(uint8_t bval);
void ledSetFunction(void *func);
void ledSetProgress(float p);
void leds_test(void);
void leds_all_strobered(void);
void leds_all_strobe(void);
void leds_show_unlocks(void);

const char *effectsCurName(void);

#define EFFECTS_REDRAW_MS 35
struct FXENTRY {
  char *name;
  void (*function)(void);
};

#endif /* __LED_H__ */
