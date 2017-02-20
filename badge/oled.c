#include "ch.h"
#include "hal.h"
#include "oled.h"
#include "spi.h"
#include "string.h"
#include "rand.h"

#include "orchard.h"
#include "orchard-ui.h"

#include "gfx.h"
#include "images.h"
#include "buildtime.h"
#include "fontlist.h"

void splash_footer(void) {
  font_t font;

  font = gdispOpenFont (FONT_SYS);
  gwinSetDefaultFont (font);
  
  gdispDrawStringBox (0, 210, gdispGetWidth(),
		      gdispGetFontMetric(font, fontHeight),
		      "IDES OF MARCH | DEFCON 25 (2017)",
		      font, White, justifyCenter);
  
  gdispDrawStringBox (0, 225, gdispGetWidth(),
		      gdispGetFontMetric(font, fontHeight),
		      BUILDTIME,
		      font, White, justifyCenter);

  gdispCloseFont (font);  

}

void oledSDFail(void) {
  font_t font;
  
  splash_footer();
  font = gdispOpenFont (FONT_LG_SANS);
  
  gdispDrawStringBox (0, 0, gdispGetWidth(),
		      gdispGetFontMetric(font, fontHeight),
		      "CHECK SD CARD!",
		      font, Red, justifyCenter); 
  
}

void oledOrchardBanner(void)
{
  gdispImage myImage;
  
  if (gdispImageOpenFile (&myImage,
			  IMG_SPLASH) == GDISP_IMAGE_ERR_OK) {
    gdispImageDraw (&myImage,
		    0, 0,
		    myImage.width,
		    myImage.height, 0, 0);
    
    gdispImageClose (&myImage);
  }
  splash_footer();
  return;
}

typedef struct sponsor {
    char * logo;
    char * caption;
} SPONSOR;

#ifdef notyet
/* Don't have any real sponsor logos yet. */
static const SPONSOR real_sponsors[] = {
  {
     NULL,
     NULL
  }
};
#endif

static const SPONSOR gag_sponsors[] = {
  {
    "yoyodyne.rgb",
    "Transportation provided by"
  },
  {
    "tyrell.rgb",
    "Labor resources provided by"
  },
  {
    "ocp.rgb",
    "Security by"
  },
  {
    "cyberdyn.rgb",
    "Technical support by"
  },
  {
    "wolfram.rgb",
    "Legal counsel"
  },
  {
    "zorg.rgb",
    "Human resource services by"
  },
  {
    "weyland.rgb",
    "Public relations by"
  },
  {
    "umbrella.rgb",
    "Health services by"
  },
  {
    "ellingsn.rgb",
    "Trash collection by"
  },
  {
    "lexcorp.rgb",
    "Research and development by"
  },
  {
    "jupiter.rgb",
    "Catering by"
  },
  {
    NULL,
    NULL
  }
};

static void
sponsorDisplay (const SPONSOR * p)
{
  gdispImage myImage;
  font_t font;

  if (gdispImageOpenFile (&myImage, p->logo) != GDISP_IMAGE_ERR_OK)
    return;

  gdispImageDraw (&myImage, 0, 0, myImage.width, myImage.height, 0, 0);
  gdispImageClose (&myImage);

  if (p->caption != NULL) {
    font = gdispOpenFont (FONT_LG_SANS);
    gdispDrawStringBox (0, 5, gdispGetWidth(),
      gdispGetFontMetric(font, fontHeight), p->caption, font, Green,
      justifyCenter);
    gdispCloseFont (font);  
  }

  chThdSleepMilliseconds (3000);

  return;
}

void oledOrchardSponsor(void)
{
  const SPONSOR * p;
  volatile int i;

  /* Display of real sponsor logos should go here */

  i = rand () % 11;
  i = rand () % 11;

  p = &gag_sponsors[i];

  sponsorDisplay (p);

  return;
}
