#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

#include "orchard-app.h"
#include "led.h"
#include "radio.h"
#include "orchard-ui.h"
#include "images.h"

#include "userconfig.h"

static int putImageFile(char *name, int16_t x, int16_t y);

static int putImageFile(char *name, int16_t x, int16_t y) {

  gdispImage img;
    
  if (gdispImageOpenFile (&img, name) == GDISP_IMAGE_ERR_OK) {
    gdispImageDraw (&img,
		    x, y,
		    img.width,
		    img.height, 0, 0);
    
    gdispImageClose (&img);
    return(1);
  } else {
    chprintf(stream, "\r\ncan't load image %s!!\r\n", name);
    return(0);
  }    

}

static void redraw_badge(void) {
  // draw the entire background badge image. Shown when the screen is idle. 
  font_t fontLG, fontSM;
  const userconfig *config = getConfig();
  
  gdispClear(Black);

  putImageFile(IMG_GUARD_IDLE_L, POS_PLAYER1_X, POS_PLAYER1_Y);
  gdispDrawThickLine(0, POS_FLOOR_Y, 320, POS_FLOOR_Y, White, 2, FALSE);

  fontLG = gdispOpenFont ("DejaVuSans32");
  fontSM = gdispOpenFont ("DejaVuSans20");
  coord_t leftpos = 165;

  gdispDrawStringBox (leftpos,
		      gdispGetHeight() / 2 - (gdispGetFontMetric(fontLG, fontHeight) / 2),
		      gdispGetWidth() - leftpos,
		      gdispGetFontMetric(fontLG, fontHeight),
		      config->name,
		      fontLG, Yellow, justifyRight);

  gdispDrawStringBox (leftpos,
		      50,
		      gdispGetWidth() - leftpos,
		      gdispGetFontMetric(fontSM, fontHeight),
		      "LEVEL 10",
		      fontSM, Yellow, justifyRight);
}

static uint32_t default_init(OrchardAppContext *context) {
  (void)context;
  return 0;
}

static uint32_t default_start(OrchardAppContext *context) {
  redraw_badge();
  return 0;
}

void default_event(OrchardAppContext *context, const OrchardAppEvent *event) {

}

static void default_exit(OrchardAppContext *context) {
  return;
}

orchard_app("Badge", default_init, default_start, default_event, default_exit);


