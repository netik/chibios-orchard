#include "ch.h"
#include "hal.h"
#include "oled.h"
#include "spi.h"

#include "orchard.h"
#include "orchard-ui.h"

#include "gfx.h"

static GHandle ghLabel1;
static GWidgetInit wi;
static gdispImage myImage;

void oledOrchardBanner(void)
{
	font_t font;
	uint8_t x, y;
 
	/*orchardGfxStart();*/

	if (gdispImageOpenFile (&myImage,
	    "caesar.rgb") == GDISP_IMAGE_ERR_OK) {
		for (x = 0; x < 4; x++) {
			for (y = 0; y < 3; y++) {
				gdispImageDraw (&myImage,
					x * 80, y * 80,
					myImage.width,
					myImage.height, 0, 0);
			}
		}
		gdispImageClose (&myImage);
	}

	font = gdispOpenFont ("DejaVuSans24");
 
	gdispDrawStringBox (0, 0, gdispGetWidth(),
		gdispGetFontMetric(font, fontHeight),
		"Hello world......", font, White, justifyCenter);

	gdispCloseFont (font);

	/* Apply some default values for GWIN */
	wi.customDraw = 0;
	wi.customParam = 0;
	wi.customStyle = 0;
	wi.g.show = TRUE;
 
	/* Apply the label parameters */
	wi.g.y = 200;
	wi.g.x = 10;
	wi.g.width = 120;
	wi.g.height = 20;
	wi.text = "     Defcon 25";

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
