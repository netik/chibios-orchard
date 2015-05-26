#ifndef __LED_H__
#define __LED_H__

#include "hal.h"

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
uint8_t gray_encode(uint8_t n);
uint8_t gray_decode(uint8_t n);

void ledStart(uint32_t leds, uint8_t *o_fb, uint32_t ui_leds, uint8_t *o_ui_fb);

void effectsStart(void);
void effectsSetPattern(char *name);
uint8_t effectsGetPattern(void);
void bump(uint32_t amount);
void setShift(uint8_t s);
uint8_t getShift(void);
void effectsNextPattern(void);
void effectsPrevPattern(void);

void uiLedGet(uint8_t index, Color *c);
void uiLedSet(uint8_t index, Color c);

void listEffects(void);

#define EFFECTS_REDRAW_MS 35

#endif /* __LED_H__ */
