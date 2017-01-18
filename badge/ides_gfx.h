/* ides_gfx.h
 *
 * Shared Graphics Routines
 * J. Adams <1/2017>
 */

/* Graphics */
extern void drawProgressBar(coord_t x, coord_t y, coord_t width, coord_t height, int32_t maxval, int32_t currentval, uint8_t use_leds, uint8_t reverse);
extern int putImageFile(char *name, int16_t x, int16_t y);
extern void blinkText (coord_t x, coord_t y,coord_t cx, coord_t cy, char *text, font_t font, color_t color, justify_t justify, uint8_t times, int16_t delay);
