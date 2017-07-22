#include <stdlib.h>

#include "ch.h"
#include "hal.h"
#include "pwm.h"
#include "led.h"
#include "gfx.h"

#include "chprintf.h"
#include "orchard-ui.h"

#include "userconfig.h"

#include <string.h>
#include <math.h>
#include <shell.h>
#include "fastled.h"

/* these keep track of the animation position and are used selectively
   by the algorithms */
#define MAX_INT_VALUE 65535

static int16_t fx_index = 0;           // fx index
static int16_t fx_position = 0;        // fx position counter
static float   led_progress = 0;       // last progress setting for drawProgressBar
static int8_t  fx_direction = 1;       // fx direction
static uint8_t led_brightshift = 0;    // right-shift value, normally zero.

// the current function that updates the LEDs. Override with ledSetFunction();
static void   (*led_current_func)(void) = NULL;

static struct led_config {
  uint8_t  *fb;          // effects frame buffer
  uint8_t  pixel_count;  // generated pixel length
  uint8_t  max_pixels;   // maximal generation length
} led_config;

/* storage area for current hp state */
static led_hp ledhp;

/* animation prototypes */
static void anim_dot(void);
static void anim_larson(void);
static void anim_police_all(void);
static void anim_mardi_gras(void);
static void anim_rainbow_fade(void);
static void anim_rainbow_loop(void);
static void anim_solid_color(void);
static void anim_triangle(void);
static void anim_comet(void);
static void anim_all_same(void);
static void anim_double_bounce(void);
static void anim_rgb_bounce(void);
static void anim_violets(void);
static void anim_wave(void);
static void anim_violetwave(void); // sorry, my favorite color is purple sooo...
static void anim_kraftwerk(void);

static uint8_t ledExitRequest = 0;
static uint8_t ledsOff = 1;

extern void ledUpdate(uint8_t *fb, uint32_t len);
static void ledSetRGB(void *ptr, int x, uint8_t r, uint8_t g, uint8_t b);

/* Update FX_COUNT in led.h if you make changes here */
const struct FXENTRY fxlist[] = {
  { "Off", NULL },
  { "Double Bounce", anim_double_bounce },
  { "Dot (White)", anim_dot},
  { "Larson Scanner", anim_larson},
  { "Rainbow Fade", anim_rainbow_fade},
  { "Rainbow Loop", anim_rainbow_loop},
  { "Green Cycle", anim_solid_color},
  { "Yellow Ramp", anim_triangle},
  { "Comet", anim_comet},
  { "Wave", anim_wave },
  { "Mardi Gras", anim_mardi_gras},
  { "All Solid", anim_all_same},
  { "Spot the Fed", anim_police_all},
  { "Violets", anim_violets},
  { "RGB Bounce", anim_rgb_bounce},
  { "Violet Wave", anim_violetwave },
  { "Kraftwerk", anim_kraftwerk }
};

// stock colors
const RgbColor roygbiv[7] = { {0xff, 0, 0},
                              {0xff, 0x32, 0},
                              {0xff, 0xff, 0},
                              {0, 0xff, 0},
                              {0, 0, 0xff},
                              {0x4B, 0x00, 0x82},
                              {0xee, 0x82, 0xee}};
void ledClear(void);

/// Fast 16-bit approximation of sin(x). This approximation never varies more than
/// 0.69% from the floating point value you'd get by doing
///
///     float s = sin(x) * 32767.0;
///
/// @param theta input angle from 0-65535
/// @returns sin of theta, value between -32767 to 32767.
static int16_t msin( uint16_t theta )
{
  static const uint16_t base[] =
    { 0, 6393, 12539, 18204, 23170, 27245, 30273, 32137 };
  static const uint8_t slope[] =
    { 49, 48, 44, 38, 31, 23, 14, 4 };

  uint16_t offset = (theta & 0x3FFF) >> 3; // 0..2047
  if( theta & 0x4000 ) offset = 2047 - offset;

  uint8_t section = offset / 256; // 0..7
  uint16_t b   = base[section];
  uint8_t  m   = slope[section];

  uint8_t secoffset8 = (uint8_t)(offset) / 2;

  uint16_t mx = m * secoffset8;
  int16_t  y  = mx + b;

  if( theta & 0x8000 ) y = -y;

  return y;
}

// These are a variety of functions shamelessly ported from fastled.
static inline uint8_t qadd8 ( uint8_t i, uint8_t j) {
  unsigned int t = i + j;
  // 8 bit "saturating" addition
  if( t > 255) t = 255;
  return t;
}

static inline uint8_t qsub8(uint8_t i, uint8_t j) {
  int t = i - j;
  // 8 bit floor-restricted subtraction
  if( t < 0) t = 0;
  return t;
}

// scale one value into a smaller range
static inline uint8_t scale8( uint8_t i, fract8 scale)
{
  return (((uint16_t)i) * (1+(uint16_t)(scale))) >> 8;
}

// scale one value into a smaller range
static inline uint8_t scale8_video( uint8_t i, fract8 scale)
{
  uint8_t j = (((int)i * (int)scale) >> 8) + ((i&&scale)?1:0);
  return j;
}
// map function from arduino

long map(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
// ---- end fastled stuff ----

void ledSetBrightness(uint8_t bval) {
  led_brightshift = bval;
}

void ledSetFunction(void *func) {
  /* Sets the current LED function. Use NULL to reset the LEDs to
   * the configured pattern. */
  userconfig *config = getConfig();

  fx_position = 0;
  fx_index = 0;
  
  ledClear();
  if (func == NULL) {
    // run the current config.
    if (config->led_pattern == 0) {
      if (ledsOff == 0) {
        effectsStop();  // we are entering sleep
      }
    }
    led_current_func = fxlist[config->led_pattern].function;
  } else {
    if (ledsOff == 1) // we are coming out of sleep
        effectsStart();

    led_current_func = func;
  }
}

void ledSetProgress(float p) {
  led_progress = p;
}

void ledSetHP(userconfig *c, peer *p) {
  ledhp.top = c->hp;
  ledhp.topmax = maxhp(c->current_type, c->unlocks, c->level);
  ledhp.bot = p->hp;
  ledhp.botmax = maxhp(p->current_type, p->unlocks, p->level);
}

void ledShowHP(void) {
  uint8_t i;

  /* Light the LEDs to represent the HP available. We will pulse them at a rate that matches the amount remaining */
  float pct_lit_top = (float)ledhp.top / ledhp.topmax;
  float pct_lit_bot = (float)ledhp.bot / ledhp.botmax;
  int total_lit_top = 6 * pct_lit_top;
  int total_lit_bot = 6 * pct_lit_bot;

  ledClear();

  /* top */
  for(i=0; i < total_lit_top; i++)  {
    /* orange bar */
    ledSetRGB(led_config.fb, i, roygbiv[1].r, roygbiv[1].g, roygbiv[1].b);
  }

  for(i=0; i < total_lit_bot; i++)  {
    /* blue bar */
    ledSetRGB(led_config.fb, 11-i, roygbiv[4].r, roygbiv[4].g, roygbiv[4].b);
  }
}

void handle_progress(void) {
  uint8_t i,c;

  float pct_lit = (float)led_progress / 100;
  int total_lit = led_config.pixel_count * pct_lit;

  if (pct_lit >= 0.8) {
    c = 3; // green
  } else if (pct_lit >= 0.5) {
    c = 2; // yel
  } else {
    c = 0; // red
  }

  ledClear();

  for(i=0; i < total_lit; i++)  {
    // invert the last few, because our LEDs are clockwise.
    if (i > 5) {

      /*
        board layout:          actual representation:
         0  1  2  3  4  5      0 1 2 3  4  5
        11 10  9  8  7  6      6 7 8 9 10 11

        so when we want to light 6, we want to light 11.
        light 7, we light 10, etc...
      */

      ledSetRGB(led_config.fb, 17-i, roygbiv[c].r, roygbiv[c].g, roygbiv[c].b);
    } else {
      ledSetRGB(led_config.fb, i, roygbiv[c].r, roygbiv[c].g, roygbiv[c].b);
    }
  }

}

void leds_all_strobered(void) {
  int i;
  ledClear();
  for(i=0; i < LEDS_COUNT; i++)  {
    if (fx_position % 2 == 0) {
      if (i % 2 == 0) {
        ledSetRGB(led_config.fb, i, 0xff,0, 0);
      }
    } else {
      if (i % 2 != 0) {
        ledSetRGB(led_config.fb, i, 0xff,0, 0);
      }
    }
  }
  fx_position++;
}

void leds_all_strobe(void) {
  int i;
  for(i=0; i < LEDS_COUNT; i++)  {
    if (fx_position % 2 == 0) {
      ledSetRGB(led_config.fb, i, 0x00, 0x00, 0x00);
    } else {
      ledSetRGB(led_config.fb, i, 0xff, 0xff, 0xff);
    }
  }

  fx_position++;
}

void leds_show_unlocks(void) {
  /* show the unlock status as a set of binary lights */
  int i;
  ledClear();
  userconfig *config = getConfig();

  if (fx_index > 250) {
    fx_index = 250;
    fx_direction = 0;
  }

  if (fx_index < 50) {
    fx_index = 50;
    fx_direction = 1;
  }
  if (fx_direction == 1) {
    fx_index += 5;
  } else {
    fx_index -= 5;
  }

  for(i=0; i < LEDS_COUNT; i++)  {
    if (config->unlocks & (1 << i)) {
      ledSetRGB(led_config.fb, i, fx_index,0,fx_index);
    } else {
      ledSetRGB(led_config.fb, i, 0, 0, 0xff);
    }
  }
}


void leds_shownetid(void) {
  userconfig *config = getConfig();

  /* display our netid on the leds
   *
   * the encoding here is:
   * MSB                            LSB
   * 3        2          1
   * 21098765432109876543210987654321
   * "RGBRGB
   */
  uint8_t rgb[3];
  int8_t pos = 31;    // position in netid
  int8_t col = 0;     // position in rgb (starts on R in RGB)
  uint8_t lednum = 0; // which led we start on

  ledClear();
  fx_position++;      // we will light the LEDs one by one until full
  if (fx_position % 3 == 0) { 
    fx_index++;
  }
  /*
   * starting at the MSB
   * starting at R
   *
   */
  rgb[0] = rgb[1] = rgb[2] = 0;

  while (pos >= 0)  { /* bits 0-31 */
    if (config->netid & (1 << pos)) {
      rgb[col] = 0xff;
    }
    col++;

    if (col > 2) {
      ledSetRGB(led_config.fb, lednum, rgb[0], rgb[1], rgb[2]);
      rgb[0] = rgb[1] = rgb[2] = 0;
      col = 0;
      lednum++;

    }
    pos--;
    if ((32-fx_index) > pos) {
      // be stingy about lighting the LEDs
      return;
    }

  }
  ledSetRGB(led_config.fb, lednum, rgb[0], rgb[1], rgb[2]);
}

void leds_test(void) {
  /* walks through R, G, B to test LEDs. */
  int i;

  fx_position++;

  if (fx_position > 10)  {
    fx_position = 0;
    fx_index++;
    if (fx_index > 2) { fx_index = 0; }
  }

  for(i=0; i < LEDS_COUNT; i++)  {
    switch (fx_index) {
    case 0:
      ledSetRGB(led_config.fb, i, 255, 0, 0);
      break;
    case 1:
      ledSetRGB(led_config.fb, i, 0, 255, 0);
      break;
    default:
      ledSetRGB(led_config.fb, i, 0,  0, 255);
      break;
    }
#ifdef ENABLE_JOYPAD
    // switch test, holding down buttons lights the LEDs white
    switch (i) {
    case 0:
      // UP
      if (palReadPad (BUTTON_UP_PORT, BUTTON_UP_PIN) == 0) {
        ledSetRGB(led_config.fb, i, 255, 255, 255);
      }
      break;
    case 1:
      // DOWN
      if (palReadPad (BUTTON_DOWN_PORT, BUTTON_DOWN_PIN) == 0) {
        ledSetRGB(led_config.fb, i, 255, 255, 255);
      }
      break;
    case 2:
      // LEFT
      if (palReadPad (BUTTON_LEFT_PORT, BUTTON_LEFT_PIN) == 0) {
        ledSetRGB(led_config.fb, i, 255, 255, 255);
      }
      break;
    case 3:
      // RIGHT
      if (palReadPad (BUTTON_RIGHT_PORT, BUTTON_RIGHT_PIN) == 0) {
        ledSetRGB(led_config.fb, i, 255, 255, 255);
      }
      break;
    case 4:
      // ENTER
      if (palReadPad (BUTTON_ENTER_PORT, BUTTON_ENTER_PIN) == 0) {
        ledSetRGB(led_config.fb, i, 255, 255, 255);
      }
      break;
    default:
      break;
    }
#endif
  }
}

/* utility functions */
// FIND INDEX OF ANTIPODAL (OPPOSITE) LED
#define LEDS_TOP_INDEX 6
static int antipodal_index(int i) {
  int iN = i + LEDS_TOP_INDEX;
  if (i >= LEDS_TOP_INDEX) {iN = ( i + LEDS_TOP_INDEX ) % led_config.max_pixels; }
  return iN;

}

static void HSVtoRGB(int hue, int sat, int val, uint8_t colors[3]) {
  // hue: 0-359, sat: 0-255, val (lightness): 0-255
  int r = 0;
  int b = 0;
  int g = 0;
  int base = 0;

  if (sat == 0) { // Achromatic color (gray).
    colors[0]=val;
    colors[1]=val;
    colors[2]=val;
  } else  {
    base = ((255 - sat) * val)>>8;
    switch(hue/60) {
    case 0:
      r = val;
      g = (((val-base)*hue)/60)+base;
      b = base;
      break;
    case 1:
      r = (((val-base)*(60-(hue%60)))/60)+base;
      g = val;
      b = base;
      break;
    case 2:
      r = base;
      g = val;
      b = (((val-base)*(hue%60))/60)+base;
      break;
    case 3:
      r = base;
      g = (((val-base)*(60-(hue%60)))/60)+base;
      b = val;
      break;
    case 4:
      r = (((val-base)*(hue%60))/60)+base;
      g = base;
      b = val;
      break;
    case 5:
      r = val;
      g = base;
      b = (((val-base)*(60-(hue%60)))/60)+base;
      break;
    }
    colors[0]=r;
    colors[1]=g;
    colors[2]=b;
  }
}

uint8_t effectsStop(void) {
  ledExitRequest = 1;
  ledClear();
  return ledsOff;
}

void ledClear(void) {
  uint8_t j;
  for (j = 0; j < led_config.max_pixels * 3; j++)
    led_config.fb[j] = 0x00;
}

void ledResetPattern(void) {
  fx_position = fx_index = 0;
  fx_direction = 1;
}

/**
 * @brief   Initialize Led Driver
 * @details Initialize the Led Driver based on parameters.
 *
 * @param[in] leds      length of the LED chain controlled by each pin
 * @param[out] o_fb     initialized frame buffer
 *
 */

void ledStart(uint32_t leds, uint8_t *o_fb)
{
  led_config.max_pixels = leds;
  led_config.pixel_count = leds;
  led_config.fb = o_fb;

  ledClear();

  ledUpdate(led_config.fb, led_config.max_pixels);
}

static void ledSetRGB(void *ptr, int x, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t *buf = ((uint8_t *)ptr) + (3 * x);

  // note color reordering, ws2812b's are g,r,b not r,g,b.
  buf[0] = g >> led_brightshift;
  buf[1] = r >> led_brightshift;
  buf[2] = b >> led_brightshift;
}

static void ledAddHSV(void *ptr, int x, int h, uint8_t s, uint8_t v) {
  uint8_t *buf = ((uint8_t *)ptr) + (3 * x);

  uint8_t color[3];

  HSVtoRGB(h,s,v,color);
  // note color reordering, ws2812b's are g,r,b not r,g,b.
  buf[0] = qadd8(buf[0], color[1]) >> led_brightshift;
  buf[1] = qadd8(buf[1], color[0]) >> led_brightshift;
  buf[2] = qadd8(buf[2], color[2]) >> led_brightshift;
}


// Anti-aliasing code care of Mark Kriegsman Google+: https://plus.google.com/112916219338292742137/posts/2VYNQgD38Pw
void drawFractionalBar(int pos16, int width, int hue, bool wrap)
{
  uint8_t i = pos16 / 16; // convert from pos to raw pixel number

  uint8_t frac = pos16 & 0x0F; // extract the 'factional' part of the position
  uint8_t firstpixelbrightness = 255 - (frac * 16);
  uint8_t lastpixelbrightness = 255 - firstpixelbrightness;

  uint8_t bright;

  for (int n = 0; n <= width; n++) {
    if (n == 0) {
      // first pixel in the bar
      bright = firstpixelbrightness;
    }
    else if (n == width) {
      // last pixel in the bar
      bright = lastpixelbrightness;
    }
    else {
      // middle pixels
      bright = 255;
    }

    ledAddHSV(led_config.fb, i, hue, 255, bright);

    i++;
    if (i == LEDS_COUNT)
      {
        if (wrap == 1) {
          i = 0; // wrap around
        }
        else{
          return;
        }
      }
  }
}

static void FadeLEDs(void *ptr, int8_t fadeamount) {
  // reduce or increase all LEDs by some value
  for(int i = 0; i < led_config.max_pixels; i++ ) {
    uint8_t *buf = ((uint8_t *)ptr) + (3 * i);
    buf[0] = qsub8(buf[0], fadeamount);
    buf[1] = qsub8(buf[1], fadeamount);
    buf[2] = qsub8(buf[2], fadeamount);
  }
}

static void Bounce(uint16_t animationFrame, int hue)
{
  uint16_t pos16;
  if (animationFrame < (MAX_INT_VALUE / 2))
    {
      pos16 = animationFrame * 2;
    } else {
    pos16 = MAX_INT_VALUE - ((animationFrame - (MAX_INT_VALUE/2))*2);
  }

  int position = map(pos16, 0, MAX_INT_VALUE, 0, ((LEDS_COUNT) * 16));
  drawFractionalBar(position, 3, hue,0);
}

static void anim_double_bounce(void) {
  // https://gist.github.com/hsiboy/f9ef711418b40e259b06

  ledClear();
  Bounce(fx_position, 0);
  Bounce(fx_position + (MAX_INT_VALUE/2), 15);
  fx_position += 500;
}

static void Wave(uint16_t animationFrame, int hue) {
  // https://gist.github.com/hsiboy/f9ef711418b40e259b06
  ledClear();

  uint16_t value;

  for(uint8_t i=0;i<LEDS_COUNT;i++)
    {
      value=(msin(animationFrame+((MAX_INT_VALUE/LEDS_COUNT)*i)) + (MAX_INT_VALUE/2))/256;
      if (value > 0) {
        ledAddHSV(led_config.fb, i, hue, 255, value);
      }
    }
}

static void anim_wave(void) {
  Wave(fx_position, 330);
  Wave(fx_position + (MAX_INT_VALUE/2), 142);
  fx_position += 1000;
}

static void anim_violetwave(void) {
  Wave(fx_position, 264);
  fx_position += 2000;
}

static void anim_triangle(void) {
  // pulsating triangle, ramp up
  int16_t N3 = led_config.max_pixels/4;
  fx_index++;
  if ( fx_index % 3 != 0 ) { return; } // 1/3rd speed
  ledClear();

  for(int i = 0; i < led_config.max_pixels; i++ ) {
    if ((i == (0+fx_position)) || (i == (N3+1+fx_position)) || (i == (N3 * 3 - 1) + fx_position)) {
      ledSetRGB(led_config.fb, i, fx_index % 255, fx_index % 255, 0);
    }
  }

  fx_position++;
  if (fx_position > N3) { fx_position = 0; }
}


static void anim_kraftwerk(void) {
  fx_index++;
  if (fx_index % 8 == 0) { fx_position++; }

  if (fx_position > 6) { fx_position = 0; };

  ledClear();
  ledSetRGB(led_config.fb, 11-fx_position, 255,0,0);
  ledSetRGB(led_config.fb, fx_position, 255,0,0);
}

static void anim_rgb_bounce(void) {
  ledClear();
  Bounce(fx_position, 0); // red
  Bounce(fx_position + (MAX_INT_VALUE/2), 114); // green
  Bounce(fx_position + (MAX_INT_VALUE/3), 253); // blue
  fx_position += 500;
}

static void anim_violets(void) {
  uint8_t acolor[3];

  // breathing violet colors with sparkle ponies
  if (fx_direction) {
    fx_position++;
  } else {
    fx_position--;
  }

  if (fx_position < 10)  {
    fx_position = 10;
    fx_direction = 1;
  }

  if (fx_position > 128) {
    fx_position = 128;
    fx_direction = 0;
  }

  for(int i = 0; i < led_config.max_pixels; i++ ) {
    HSVtoRGB(274, 255, fx_position, acolor);
    ledSetRGB(led_config.fb, i, acolor[0], acolor[1], acolor[2]);
  }

  // sparkles
  if (fx_position % 5 == 0)
    ledSetRGB(led_config.fb, random8(0, led_config.max_pixels), 0xf0, 0xf0, 0xf0);
}

static void anim_comet(void) {
  uint8_t color[3];
  int hueval = 180;
  uint8_t intensity = 200;
  uint8_t bright;

  if (fx_position >= led_config.max_pixels) {
    fx_position = 0;
  }

  HSVtoRGB(hueval, 255, intensity, color);
  ledSetRGB(led_config.fb, fx_position, color[0], color[1], color[2]); // head


  bright=random8(50,100);
  HSVtoRGB((hueval+40), 255, bright, color);
  ledSetRGB(led_config.fb, fx_position, color[0], color[1], color[2]);

  FadeLEDs(led_config.fb, 8);
  fx_position++;

}

static void anim_rainbow_fade(void) {
    //-FADE ALL LEDS THROUGH HSV RAINBOW
    fx_position++;
    if (fx_position >= 359) { fx_position = 0; }
    uint8_t thisColor[3];

    HSVtoRGB(fx_position, 255, 255, thisColor);

    for(int idex = 0 ; idex < led_config.max_pixels; idex++ ) {
      ledSetRGB(led_config.fb, idex,thisColor[0],thisColor[1],thisColor[2]);
    }
}

static void anim_rainbow_loop(void) { //-LOOP HSV RAINBOW
  fx_position++;
  fx_index = fx_index + 10;
  uint8_t icolor[3];

  if (fx_position >= led_config.max_pixels) {fx_position = 0;}
  if (fx_index >= 359) {fx_index = 0;}

  HSVtoRGB(fx_index, 255, 255, icolor);
  ledSetRGB(led_config.fb, fx_position, icolor[0], icolor[1], icolor[2]);
}


static void anim_police_all(void) {
  //-POLICE LIGHTS (TWO COLOR SOLID)
  fx_position++;
  if (fx_position >= led_config.max_pixels) {fx_position = 0;}
  int fx_positionR = fx_position;
  int fx_positionB = antipodal_index(fx_positionR);
  ledSetRGB(led_config.fb, fx_positionR, 255, 0, 0);
  ledSetRGB(led_config.fb, fx_positionB, 0, 0, 255);
}

static void anim_mardi_gras(void) {
  fx_position++;

  if (fx_position >= led_config.max_pixels) {fx_position = 0;}
  int idexR = fx_position;
  int idexB = antipodal_index(idexR);
  for(int i = 0; i < led_config.max_pixels; i++ ) {
    if (i == idexR) { ledSetRGB(led_config.fb, i, 255, 255, 0); }
    else if (i == idexB) { ledSetRGB(led_config.fb, i, 225, 0, 255 ); }
    else  { ledSetRGB(led_config.fb, i, 0, 0, 0 ); }
  }

  chThdSleepMilliseconds(20);
}


static void anim_dot(void) { // single dim white dot, one direction
  fx_position++;
  if (fx_position >= led_config.max_pixels) {fx_position = 0;}

  for(int i = 0; i < led_config.max_pixels; i++ ) {
    if (i == fx_position) {
      ledSetRGB(led_config.fb, i, 20,20,20 );
    } else {
      ledSetRGB(led_config.fb, i, 0, 0, 0 );
    }
  }
}

static void anim_larson(void) {
  Color c, scol;
  uint8_t i;
  float spread = 1.0;
  c.r = 255;
  c.g = 0;
  c.b = 0;

  ledClear();

  // set primary dot
  ledSetRGB(led_config.fb, fx_position, c.r, c.g, c.b);

  // handle spread
  for (i = 1; i <= spread; i++) {
    scol.r = c.r * ((spread + 1 - i) / (spread + 1)) - (40*i);
    scol.g = c.g * ((spread + 1 - i) / (spread + 1));
    scol.b = c.b * ((spread + 1 - i) / (spread + 1));

    if (fx_position + i < led_config.max_pixels) {
      ledSetRGB(led_config.fb, fx_position + i, scol.r, scol.g, scol.b);
    }

    if (fx_position - i >= 0) {
      ledSetRGB(led_config.fb, fx_position - i, scol.r, scol.g, scol.b);
    }
  }

  fx_position += fx_direction;

  // check for out-of-bounds:
  if (fx_position >= led_config.max_pixels) {
    fx_position -= 2;
    fx_direction = -1; // all skate, reverse direction!
  }

  if (fx_position < 0) {
    fx_position += 2;
    fx_direction = 1;
  }
}

static void anim_solid_color(void) {
  uint8_t i;

  // solid color spiral (green)
  for(i=0; i < led_config.pixel_count; i++) {
    // range 0-16 +
    ledSetRGB(led_config.fb, i, 0, ((msin((i+fx_position)) * 127 + 128)/255)*180,  0);
  }

  fx_position++;
  if (fx_position > 360) fx_position = 0;
}

static void anim_all_same(void) {
  userconfig *config = getConfig();

  for(int i = 0 ; i < led_config.max_pixels; i++ ) {
    ledSetRGB(led_config.fb, i, config->led_r, config->led_g, config->led_b);
  }
}

static THD_WORKING_AREA(waEffectsThread, 256);
static THD_FUNCTION(effects_thread, arg) {
  (void)arg;
  userconfig *config = getConfig();

  chRegSetThreadName("LED effects");

  led_current_func = fxlist[config->led_pattern].function;

  while (!ledsOff) {

    // transmit the actual framebuffer to the LED chain
    ledUpdate(led_config.fb, led_config.pixel_count);

    // wait until the next update cycle
    chThdYield();
    chThdSleepMilliseconds(EFFECTS_REDRAW_MS);

    // re-render the internal framebuffer animations
    if (led_current_func != NULL)
      led_current_func();

    if( ledExitRequest ) {
      // force one full cycle through an update on request to force LEDs off
      ledUpdate(led_config.fb, led_config.pixel_count);
      ledsOff = 1;
      chThdExitS(MSG_OK);
    }
  }
  return;
}

void effectsStart(void) {
  userconfig *config = getConfig();

  // init running params
  fx_index = 0;

  // set user config
  led_brightshift = config->led_shift;

  if (*fxlist[config->led_pattern].function != NULL)
    (*fxlist[config->led_pattern].function)();

  ledExitRequest = 0;
  ledsOff = 0;

  /* LEDs are very, very sensitive to timing. Make this slightly
   * better than a normal thread */
  chThdCreateStatic(waEffectsThread, sizeof(waEffectsThread),
                    NORMALPRIO  + 2, effects_thread, &led_config);
}

