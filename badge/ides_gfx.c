/* Ides of March Badge
 *
 * Shared Graphics Routines
 * J. Adams <1/2017>
 */

#include "orchard-app.h"
#include "led.h"
#include "string.h"
#include "fontlist.h"
#include "ides_gfx.h"

// WidgetStyle: RedButton, the only button we really use
const GWidgetStyle RedButtonStyle = {
  HTML2COLOR(0xff0000),              // background
  HTML2COLOR(0xff6666),              // focus

  // Enabled color set
  {
    HTML2COLOR(0xffffff),         // text
    HTML2COLOR(0x800000),         // edge
    HTML2COLOR(0xff0000),         // fill
    HTML2COLOR(0x008000),         // progress (inactive area)
  },

  // Disabled color set
  {
    HTML2COLOR(0x808080),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0x404040),         // fill
    HTML2COLOR(0x004000),         // progress (active area)
  },

  // Pressed color set
  {
    HTML2COLOR(0xFFFFFF),         // text
    HTML2COLOR(0x800000),         // edge
    HTML2COLOR(0xff6a71),         // fill
    HTML2COLOR(0x008000),         // progress (active area)
  }
};

const GWidgetStyle DarkBlueStyle = {
  HTML2COLOR(0x001789),              // background
  HTML2COLOR(0x2ca0ff),              // focus

  // Enabled color set
  {
    HTML2COLOR(0xc8c8c8),         // text
    HTML2COLOR(0xc0c0c0),         // edge
    HTML2COLOR(0x001789),         // fill
    HTML2COLOR(0x008000),         // progress (inactive area)
  },

  // Disabled color set
  {
    HTML2COLOR(0x808080),         // text
    HTML2COLOR(0x404040),         // edge
    HTML2COLOR(0x404040),         // fill
    HTML2COLOR(0x004000),         // progress (active area)
  },

  // Pressed color set
  {
    HTML2COLOR(0xFFFFFF),         // text
    HTML2COLOR(0xC0C0C0),         // edge
    HTML2COLOR(0xE0E0E0),         // fill
    HTML2COLOR(0x008000),         // progress (active area)
  }
};

extern void handle_progress(void);

void drawProgressBar(coord_t x, coord_t y, coord_t width, coord_t height, int32_t maxval, int32_t currentval, uint8_t use_leds, uint8_t reverse) {
  // draw a bar if reverse is true, we draw right to left vs left to
  // right

  // WARNING: if x+w > screen_width or y+height > screen_height,
  // unpredicable things will happen in memory. There is no protection
  // for overflow here.
  
  color_t c = Lime;
  float remain_f;
  
  if (currentval < 0) { currentval = 0; } // never overflow
  if (currentval > maxval) {
    // prevent bar overflow
    remain_f = 1;
  } else {  
    remain_f = (float) currentval / (float)maxval;
  }
  
  int16_t remain = width * remain_f;
  
  if (use_leds == 1) {
    ledSetFunction(handle_progress);
    ledSetProgress(100 * remain_f);
  }
  
  if (remain_f >= 0.8) {
    c = Lime;
  } else if (remain_f >= 0.5) {
    c = Yellow;
  } else {
    c = Red;
  }

  if (reverse) { 
    gdispFillArea(x,y+1,(width - remain)-1,height-2, Black);
    gdispFillArea((x+width)-remain,y,remain,height, c);
    gdispDrawBox(x,y,width,height, c);
  } else {
    gdispFillArea(x + remain,y+1,(width - remain)-1,height-2, Black);
    gdispFillArea(x,y,remain,height, c);
    gdispDrawBox(x,y,width,height, c);
  }
}


char *getAvatarImage(int ptype, char *imgtype, uint8_t frameno, uint8_t reverse) {
  static char fname[13];
  const char classlist[] = "_gsxcb";

  /* return an image based on the desired character class, image type,
   * and frame number. It is up to the caller to supply the correct
   * player type. Unexpected results will occur if the player type is
   * greater than 2!
   * 
   * 0 = notset
   * 1 = guard
   * 2 = senator
   * 3 = gladatrix
   * 4 = caesar
   * 5 = bender (shh...)
   *
   * getAvatarImage(0,'deth',1) == gdeth1.rgb
   * getAvatarImage(2,'attm',1) == cattm1.rgb
   */

  chsnprintf(fname, sizeof(fname), "%c%s%d%s.rgb", classlist[ptype], imgtype, frameno, reverse == true ? "r" : "");
  return(fname);
         
}

int putImageFile(char *name, int16_t x, int16_t y) {
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

void blinkText (coord_t x, coord_t y,coord_t cx, coord_t cy, char *text, font_t font, color_t color, justify_t justify, uint8_t times, int16_t delay) {
  for (int i=0; i < times; i++) {
    if (i & 1)
      gdispDrawStringBox (x,y,cx,cy,text,font,Black,justify);
    else
      gdispDrawStringBox (x,y,cx,cy,text,font,color,justify);
    gdispFlush();
    chThdSleepMilliseconds(delay);
  }
}

void screen_alert_draw(uint8_t clear, char *msg) {
  uint16_t middle = (gdispGetHeight() / 2);
  font_t fontFF = gdispOpenFont (FONT_FIXED);
  
  if (clear) {
    gdispClear(Black);
  } else {
    // just black out the drawing area. 
    gdispFillArea( 0, middle - 20, 320, 40, Black );
  }
  
  gdispDrawThickLine(0,middle - 20, 320, middle -20, Red, 2, FALSE);
  gdispDrawThickLine(0,middle + 20, 320, middle +20, Red, 2, FALSE);
    
  gdispDrawStringBox (0,
		      (gdispGetHeight() / 2) - (gdispGetFontMetric(fontFF, fontHeight) / 2),
		      gdispGetWidth(),
		      gdispGetFontMetric(fontFF, fontHeight),
		      msg,
		      fontFF, Red, justifyCenter);
  gdispCloseFont(fontFF);
}

uint8_t airplane_mode_check(void) {
  userconfig *config = getConfig();

  if (config->airplane_mode) {
    screen_alert_draw(true, "TURN OFF AIRPLANE MODE!");
    chThdSleepMilliseconds(ALERT_DELAY);
    orchardAppRun(orchardAppByName("Badge"));
    return true;
  } else {
    return false;
  }
}
