#include "ch.h"
#include "hal.h"
#include "oled.h"
#include "spi.h"
#include "string.h"

#include "orchard.h"
#include "orchard-ui.h"
#include "gitversion.h"

#include "gfx.h"
#include "images.h"

static GHandle ghLabel1;
static GWidgetInit wi;
static gdispImage myImage;

void oledOrchardBanner(void)
{
	font_t font;
	
	if (gdispImageOpenFile (&myImage,
				IMG_SPLASH) == GDISP_IMAGE_ERR_OK) {
	  gdispImageDraw (&myImage,
			  0, 0,
			  myImage.width,
			  myImage.height, 0, 0);
	  gdispImageClose (&myImage);
	}
	
	font = gdispOpenFont ("DejaVuSans24");
 
	/* 	gdispDrawStringBox (0, 0, gdispGetWidth(),
		gdispGetFontMetric(font, fontHeight),
		"It works!", font, White, justifyCenter);
	*/

	gdispCloseFont (font);

	/* Apply some default values for GWIN */
	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = TRUE;
 
	/* Apply the label parameters */
	wi.g.y = 205;
	wi.g.x = 2;
	wi.g.width = 316;
	wi.g.height = 35;
	wi.text = "  IDES OF MARCH - DC 25 - Build #xxx";
	font = gdispOpenFont ("UI1");
	gwinSetDefaultFont (font);
	/*gwinSetDefaultStyle (&WhiteWidgetStyle, FALSE);*/
 
	/* Create the actual label */
	ghLabel1 = gwinLabelCreate (NULL, &wi);  
	gwinLabelSetBorder (ghLabel1, TRUE);
	gwinRedraw (ghLabel1);

	gdispCloseFont (font);
	/* orchardGfxEnd();*/

	return;
}
