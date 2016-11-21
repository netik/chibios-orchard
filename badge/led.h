#ifndef __LED_H__
#define __LED_H__

#include "hal.h"
#include "math.h"

#define LEDS_COUNT 16
#define LEDS_TOP_INDEX 8

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

const char *effectsCurName(void);

#define EFFECTS_REDRAW_MS 35
struct FXENTRY {
  char *name;
  void (*function)(void);
};

#define FX_COUNT 9

#endif /* __LED_H__ */
