#include "orchard-app.h"
#include "orchard-ui.h"
#include "ff.h"
#include "ffconf.h"
#include "board_ILI9341.h"

#define ILI9341_VSDEF	0x33
#define ILI9341_VSADD	0x37

typedef struct gdisp_image {
        uint8_t                 gdi_id1;
        uint8_t                 gdi_id2;
        uint8_t                 gdi_width_hi;
        uint8_t                 gdi_width_lo;
        uint8_t                 gdi_height_hi;
        uint8_t                 gdi_height_lo;
        uint8_t                 gdi_fmt_hi;
        uint8_t                 gdi_fmt_lo;
} GDISP_IMAGE;

typedef struct _CreditsHandles {
	GHandle		ghExitButton;
	GListener	gl;
	uint8_t		stop;
} CreditsHandles;

static void scrollAreaSet (uint16_t TFA, uint16_t BFA)
{
	acquire_bus (NULL);
	write_index (NULL, ILI9341_VSDEF);
	write_data (NULL, TFA >> 8);
	write_data (NULL, (uint8_t)TFA);
	write_data (NULL, (320 - TFA - BFA) >> 8);
	write_data (NULL, (uint8_t)(320 - TFA - BFA));
	write_data (NULL, BFA >> 8);
	write_data (NULL, (uint8_t)BFA);
	release_bus (NULL);

	return;
}

static void scroll (int lines)
{
	acquire_bus (NULL);
	write_index (NULL, ILI9341_VSADD);
	write_data (NULL, lines >> 8);
	write_data (NULL, (uint8_t)lines);
	release_bus (NULL);

	return;
}

static uint32_t credits_init(OrchardAppContext *context)
{
	(void)context;

	return (0);
}

static void credits_start(OrchardAppContext *context)
{
	int i;
	int pos;
	FIL f;
 	UINT br;
	GDISP_IMAGE hdr;
	uint16_t h;
	pixel_t * buf;
	CreditsHandles * p;
	GWidgetInit wi;
	orientation_t o;

	if (f_open (&f, "credits.rgb", FA_READ) != FR_OK) {
		orchardAppExit ();
		return;
	}

	p = chHeapAlloc (NULL, sizeof(CreditsHandles));
	context->priv = p;

	gwinWidgetClearInit (&wi);
	wi.g.show = TRUE;
	wi.g.width = gdispGetWidth();
	wi.g.height = gdispGetHeight();
  	wi.g.y = 0;
  	wi.g.x = 0;
  	wi.text = "";
 	wi.customDraw = noRender;

	p->ghExitButton = gwinButtonCreate (NULL, &wi);
	geventListenerInit (&p->gl);
	gwinAttachListener (&p->gl);
	geventRegisterCallback (&p->gl, orchardAppUgfxCallback, &p->gl);

	p->stop = 0;

	f_read (&f, &hdr, sizeof(hdr), &br);
	h = hdr.gdi_height_hi << 8 | hdr.gdi_height_lo;

	buf = chHeapAlloc (NULL, sizeof(pixel_t) * gdispGetHeight ());

	o = gdispGetOrientation();

	scrollAreaSet (0, 0);
	pos = gdispGetWidth() - 1;
	for (i = 0; i < h; i++) {
		if (p->stop)
			break;
		f_read (&f, buf, sizeof(pixel_t) * gdispGetHeight (), &br);
		gdispBlitAreaEx (pos, 0, 1, gdispGetHeight (), 0, 0, 1, buf);
		pos++;
		if (pos == gdispGetWidth ())
			pos = 0;
		scroll (o == GDISP_ROTATE_90 ?
			(gdispGetWidth() - 1) - pos : pos);
		chThdSleepMilliseconds (15);
	}

	scroll (0);
	chHeapFree (buf);
	f_close (&f);

	if (p->stop == 0)
		chThdSleepMilliseconds (800);

	orchardAppExit ();

	return;
}

void credits_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	CreditsHandles * p;

        if (event->type == ugfxEvent) {
		p = context->priv;
		p->stop = 1;
	}

	return;
}

static void credits_exit(OrchardAppContext *context)
{
	CreditsHandles * p;

	p = context->priv;

	if (p != NULL) {
		geventRegisterCallback (&p->gl, NULL, NULL);
		geventDetachSource (&p->gl, NULL);
		gwinDestroy (p->ghExitButton);

		chHeapFree (p);
		context->priv = NULL;
	}

	return;
}

orchard_app("Credits", 0, credits_init, credits_start,
		credits_event, credits_exit);
