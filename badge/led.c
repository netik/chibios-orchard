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

#define VU_T_PERIOD 2500

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

extern void ledUpdate(uint8_t *fb, uint32_t len);
static void ledSetRGB(void *ptr, int x, uint8_t r, uint8_t g, uint8_t b, uint8_t shift);

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
}



