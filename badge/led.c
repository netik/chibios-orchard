#include "ch.h"
#include "hal.h"
#include "pwm.h"
#include "led.h"
#include "gfx.h"

#include "chprintf.h"
#include "stdlib.h"
#include "orchard-ui.h"

#include "userconfig.h"

#include <string.h>
#include <math.h>
#include <shell.h>

/* these keep track of the animation position and are used selectively
   by the algorithms */
static int16_t fx_index = 0;           // fx index 
static int16_t fx_position = 0;        // fx position counter
static float   led_progress = 0;       // last progress setting
static int8_t  fx_direction = 1;       // fx direction
static uint8_t led_brightshift = 0;    // right-shift value, normally zero.

// the current function that updates the LEDs. Override with ledSetFunction();
static void   (*led_current_func)(void) = NULL; 

static struct led_config {
  uint8_t  *fb;          // effects frame buffer
  uint8_t  pixel_count;  // generated pixel length
  uint8_t  max_pixels;   // maximal generation length
} led_config;

/* animation prototypes */
static void anim_color_bounce_fade(void);
static void anim_dot(void);
static void anim_larsen(void);
static void anim_police_all(void);
static void anim_police_dots(void);
static void anim_rainbow_fade(void);
static void anim_rainbow_loop(void);
static void anim_solid_color(void);
static void anim_triangle(void);
static void anim_one_hue_pulse(void);
static void anim_all_same(void);

// stock colors
const RgbColor roygbiv[7] = { {255, 0, 0},
			      {255, 80, 0},
			      {255, 255, 0},
			      {0, 255, 0},
			      {65, 65, 190},
			      {0, 0, 100},
			      {0, 0, 120} };
static uint8_t ledExitRequest = 0;
static uint8_t ledsOff = 0;

extern void ledUpdate(uint8_t *fb, uint32_t len);
static void ledSetRGB(void *ptr, int x, uint8_t r, uint8_t g, uint8_t b);


/* Update FX_COUNT in led.h if you make changes here */
const struct FXENTRY fxlist[] = {
  {"ColorBounce", anim_color_bounce_fade},
  {"Dot (White)", anim_dot},
  {"Larsen Scanner", anim_larsen},
  {"Police (Solid)", anim_police_all},
  {"Police (Dots)", anim_police_dots},
  {"Rainbow Fade", anim_rainbow_fade},
  {"Rainbow Loop", anim_rainbow_loop},
  {"Green Spiral", anim_solid_color},
  {"Yellow Triangle", anim_triangle},
  {"Random Hue Pulse", anim_one_hue_pulse},
  {"All Solid", anim_all_same}
};

void ledClear(void);

/* Maclaurin series of sin(x), weak approximation */
#define msin(x) (x - ((x^3)/factorial(3)) + ((x^5)/factorial(5)) - ((x^7)/factorial(7)) + ((x^9)/factorial(9)))

uint16_t factorial(uint16_t n)
{
  uint16_t c;
  uint16_t result = 1;

  for (c = 1; c <= n; c++)
    result = result * c;

  return result;
}

void ledSetBrightness(uint8_t bval) {
  led_brightshift = bval;
}

void ledSetFunction(void *func) {
  ledClear();
  led_current_func = func;
}

void ledSetProgress(float p) {
  led_progress = p;
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
    ledSetRGB(led_config.fb, i, roygbiv[c].r, roygbiv[c].g, roygbiv[c].b);
  }
					     
}

/* utility functions */
// FIND INDEX OF ANTIPODAL (OPPOSITE) LED
static int antipodal_index(int i) {
  int iN = i + LEDS_TOP_INDEX;
  if (i >= LEDS_TOP_INDEX) {iN = ( i + LEDS_TOP_INDEX ) % led_config.max_pixels; }
  return iN;
}

// FIND ADJACENT INDEX CLOCKWISE
static int adjacent_cw(int i) {
  int r;
  if (i < led_config.max_pixels - 1) {r = i + 1;}
  else {r = 0;}
  return r;
}


// FIND ADJACENT INDEX COUNTER-CLOCKWISE
static int adjacent_ccw(int i) {
  int r;
  if (i > 0) {r = i - 1;}
  else {r = led_config.max_pixels - 1;}
  return r;
}


static void HSVtoRGB(int hue, int sat, int val, int colors[3]) {
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
  buf[0] = g >> led_brightshift;
  buf[1] = r >> led_brightshift;
  buf[2] = b >> led_brightshift;
}

static void anim_triangle(void) {
  // TODO: Make it breathe. 
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

static void anim_one_hue_pulse(void) { //-PULSE BRIGHTNESS ON ALL LEDS TO ONE COLOR
  if (fx_index == 0) {
    // if it's the first time through, pick a random color
    fx_index = 200;
  }
    
  if (fx_direction == 0) {
    fx_position++;
    if (fx_position >= 255) {fx_direction = 1;}
  }

  if (fx_direction == 1) {
    fx_position--;
    if (fx_position <= 1) {fx_direction = 0;}
  }
  
  int acolor[3];
  HSVtoRGB(fx_index, 255, fx_position, acolor);

  for(int i = 0 ; i < led_config.max_pixels; i++ ) {
    ledSetRGB(led_config.fb, i, acolor[0], acolor[1], acolor[2]);
  }
}

static void anim_rainbow_fade(void) {
    //-FADE ALL LEDS THROUGH HSV RAINBOW
    fx_position++;
    if (fx_position >= 359) { fx_position = 0; }
    int thisColor[3];

    HSVtoRGB(fx_position, 255, 255, thisColor);

    for(int idex = 0 ; idex < led_config.max_pixels; idex++ ) {
      ledSetRGB(led_config.fb, idex,thisColor[0],thisColor[1],thisColor[2]);
    }
}

static void anim_rainbow_loop(void) { //-LOOP HSV RAINBOW
  fx_position++;
  fx_index = fx_index + 10;
  int icolor[3];

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

static void anim_police_dots(void) {
  fx_position++;

  if (fx_position >= led_config.max_pixels) {fx_position = 0;}
  int idexR = fx_position;
  int idexB = antipodal_index(idexR);
  for(int i = 0; i < led_config.max_pixels; i++ ) {
    if (i == idexR) { ledSetRGB(led_config.fb, i, 255, 0, 0); }
    else if (i == idexB) { ledSetRGB(led_config.fb, i, 0, 0, 255 ); }
    else  { ledSetRGB(led_config.fb, i, 0, 0, 0 ); }
  }
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

static void anim_larsen(void) {
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

static void anim_color_bounce_fade(void) {
    //-BOUNCE COLOR (SIMPLE MULTI-LED FADE)
    // jna: a bit too close to the larsen scanner, maybe get rid of this?
    if (fx_direction == 0) {
      fx_position = fx_position + 1;
      if (fx_position >= led_config.max_pixels) {
	fx_direction = 1;
	fx_position = fx_position - 1;
      }
    }
    if (fx_direction == 1) {
      fx_position = fx_position - 1;
      if (fx_position < 0) {
	fx_direction = 0;
      }
    }
    int iL1 = adjacent_cw(fx_position);
    int iL2 = adjacent_cw(iL1);
    int iL3 = adjacent_cw(iL2);
    int iR1 = adjacent_ccw(fx_position);
    int iR2 = adjacent_ccw(iR1);
    int iR3 = adjacent_ccw(iR2);

    for(int i = 0; i < led_config.max_pixels; i++ ) {
      if (i == fx_position) {ledSetRGB(led_config.fb, i, 255, 0, 0);}
      else if (i == iL1) {ledSetRGB(led_config.fb, i, 100, 0, 0);}
      else if (i == iL2) {ledSetRGB(led_config.fb, i, 50, 0, 0);}
      else if (i == iL3) {ledSetRGB(led_config.fb, i, 10, 0, 0);}
      else if (i == iR1) {ledSetRGB(led_config.fb, i, 100, 0, 0);}
      else if (i == iR2) {ledSetRGB(led_config.fb, i, 50, 0, 0);}
      else if (i == iR3) {ledSetRGB(led_config.fb, i, 10, 0, 0);}
      else { ledSetRGB(led_config.fb, i, 0, 0, 0); }
  }
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
  config = getConfig();
  led_brightshift = config->led_shift;

  (*fxlist[config->led_pattern].function)();
  
  ledExitRequest = 0;
  ledsOff = 0;

  /* LEDs are very, very sensitive to timing. Make this slightly
   * better than a normal thread */
  chThdCreateStatic(waEffectsThread, sizeof(waEffectsThread),
                    NORMALPRIO + 1, effects_thread, &led_config);
}

