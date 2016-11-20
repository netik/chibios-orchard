#include "ch.h"
#include "hal.h"
#include "pwm.h"
#include "led.h"
#include "gfx.h"

#include "chprintf.h"
#include "stdlib.h"
#include "orchard-ui.h"

#include <string.h>
#include <math.h>

static uint8_t fx_index = 0;  // current effect
static uint16_t fx_position = 0;   // fx position counter
static uint8_t fx_max = 0;    // max # of effects

extern void ledUpdate(uint8_t *fb, uint32_t len);
static void ledSetRGB(void *ptr, int x, uint8_t r, uint8_t g, uint8_t b, uint8_t shift);

// hardware configuration information
// max length is different from actual length because some
// pattens may want to support the option of user-added LED
// strips, whereas others will focus only on UI elements in the
// circle provided on the board itself
static struct led_config {
  uint8_t       *fb; // effects frame buffer
  uint32_t      pixel_count;  // generated pixel length
  uint32_t      max_pixels;   // maximal generation length
} led_config;

static uint8_t ledExitRequest = 0;
static uint8_t ledsOff = 0;

uint8_t effectsStop(void) {
  ledExitRequest = 1;
  return ledsOff;
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
  unsigned int j;
  led_config.max_pixels = leds;
  led_config.pixel_count = leds;
  led_config.fb = o_fb;

  for (j = 0; j < leds * 3; j++)
    led_config.fb[j] = 0x00;

  chSysLock();
  ledUpdate(led_config.fb, led_config.max_pixels);
  chSysUnlock();
}

static void ledSetRGB(void *ptr, int x, uint8_t r, uint8_t g, uint8_t b, uint8_t shift) {
  uint8_t *buf = ((uint8_t *)ptr) + (3 * x);
  buf[0] = g >> shift;
  buf[1] = r >> shift;
  buf[2] = b >> shift;
}

static void draw_pattern(void) {
  uint8_t i;

  // solid color spiral
  for(i=0; i < led_config.pixel_count; i++) {
    ledSetRGB(led_config.fb,i,0,
	     ((sin(i+fx_position) * 127 + 128)/255)*180,
	      0,0);
  }

  fx_position++;
  if (fx_position > 360) fx_position = 0;
}

static THD_WORKING_AREA(waEffectsThread, 256);
static THD_FUNCTION(effects_thread, arg) {
  (void)arg;
  chRegSetThreadName("LED effects");
  
  while (!ledsOff) {
    // transmit the actual framebuffer to the LED chain
    chSysLock();
    ledUpdate(led_config.fb, led_config.pixel_count);
    chSysUnlock();
    
    // wait until the next update cycle
    chThdYield();
    chThdSleepMilliseconds(EFFECTS_REDRAW_MS);
    
    // re-render the internal framebuffer animations
    draw_pattern();
    
    if( ledExitRequest ) {
      // force one full cycle through an update on request to force LEDs off
      chSysLock();
      ledUpdate(led_config.fb, led_config.pixel_count);
      ledsOff = 1;
      chThdExitS(MSG_OK);
      chSysUnlock();
    }
  }
  return;
}

void effectsStart(void) {
  fx_max = 0;
  fx_index = 0;
  draw_pattern();
  ledExitRequest = 0;
  ledsOff = 0;

  chThdCreateStatic(waEffectsThread, sizeof(waEffectsThread),
      NORMALPRIO - 6, effects_thread, &led_config);
}

