/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#include "gfx.h"

#if GFX_USE_GDISP

#if defined(GDISP_SCREEN_HEIGHT)
	#warning "GDISP: This low level driver does not support setting a screen size. It is being ignored."
	#undef GISP_SCREEN_HEIGHT
#endif
#if defined(GDISP_SCREEN_WIDTH)
	#warning "GDISP: This low level driver does not support setting a screen size. It is being ignored."
	#undef GDISP_SCREEN_WIDTH
#endif

#define GDISP_DRIVER_VMT			GDISPVMT_STM32F429iDiscovery
#include "drivers/gdisp/STM32F429iDiscovery/gdisp_lld_config.h"
#include "src/gdisp/gdisp_driver.h"

#include "stm32_ltdc.h"

typedef struct ltdcLayerConfig {
	// frame
	LLDCOLOR_TYPE	*frame;			// Frame buffer address
	coord_t			width, height;	// Frame size in pixels
	coord_t			pitch;			// Line pitch, in bytes
	uint16_t		fmt;			// Pixel format in LTDC format

	// window
	coord_t			x, y;			// Start pixel position of the virtual layer
	coord_t			cx, cy;			// Size of the virtual layer

	uint32_t		defcolor;		// Default color, ARGB8888
	uint32_t		keycolor;		// Color key, RGB888
	uint32_t		blending;		// Blending factors
	const uint32_t *palette;		// The palette, RGB888 (can be NULL)
	uint16_t		palettelen;		// Palette length
	uint8_t			alpha;			// Constant alpha factor
	uint8_t			layerflags;		// Layer configuration
} ltdcLayerConfig;

#define LTDC_UNUSED_LAYER_CONFIG	{ 0, 1, 1, 1, LTDC_FMT_L8, 0, 0, 1, 1, 0x000000, 0x000000, LTDC_BLEND_FIX1_FIX2, 0, 0, 0, 0 }

typedef struct ltdcConfig {
	coord_t		width, height;				// Screen size
	coord_t		hsync, vsync;				// Horizontal and Vertical sync pixels
	coord_t		hbackporch, vbackporch;		// Horizontal and Vertical back porch pixels
	coord_t		hfrontporch, vfrontporch;	// Horizontal and Vertical front porch pixels
	uint8_t		syncflags;					// Sync flags
	uint32_t	bgcolor;					// Clear screen color RGB888

	ltdcLayerConfig	bglayer;				// Background layer config
	ltdcLayerConfig	fglayer;				// Foreground layer config
} ltdcConfig;

#if GDISP_LLD_PIXELFORMAT == GDISP_PIXELFORMAT_RGB565
	#define LTDC_PIXELFORMAT	LTDC_FMT_RGB565
	#define LTDC_PIXELBYTES		2
	#define LTDC_PIXELBITS		16
#elif GDISP_LLD_PIXELFORMAT == GDISP_PIXELFORMAT_RGB888
	#define LTDC_PIXELFORMAT	LTDC_FMT_RGB888
	#define LTDC_PIXELBYTES		3
	#define LTDC_PIXELBITS		24
#else
	#error "GDISP: STM32F4iDiscovery - unsupported pixel format"
#endif

#include "board_STM32F429iDiscovery.h"

#include "ili9341.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#ifndef GDISP_INITIAL_CONTRAST
	#define GDISP_INITIAL_CONTRAST	50
#endif
#ifndef GDISP_INITIAL_BACKLIGHT
	#define GDISP_INITIAL_BACKLIGHT	100
#endif

/*===========================================================================*/
/* Driver local routines    .                                                */
/*===========================================================================*/

#define PIXIL_POS(g, x, y)		((y) * driverCfg.bglayer.pitch + (x) * LTDC_PIXELBYTES)
#define PIXEL_ADDR(g, pos)		((LLDCOLOR_TYPE *)((uint8_t *)driverCfg.bglayer.frame+pos))

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

static void InitController(GDisplay *g) {
	#define REG_TYPEMASK	0xFF00
	#define REG_DATAMASK	0x00FF

	#define REG_DATA		0x0000
	#define REG_COMMAND		0x0100
	#define REG_DELAY		0x0200

	static const uint16_t initdata[] = {
			REG_COMMAND | ILI9341_CMD_RESET,
			REG_DELAY   | 5,
			REG_COMMAND | ILI9341_CMD_DISPLAY_OFF,
			REG_COMMAND | ILI9341_SET_FRAME_CTL_NORMAL, 0x00, 0x1B,
			REG_COMMAND | ILI9341_SET_FUNCTION_CTL, 0x0A, 0xA2,
			REG_COMMAND | ILI9341_SET_POWER_CTL_1, 0x10,
			REG_COMMAND | ILI9341_SET_POWER_CTL_2, 0x10,
			REG_COMMAND | ILI9341_SET_VCOM_CTL_1, 0x45, 0x15,
			REG_COMMAND | ILI9341_SET_VCOM_CTL_2, 0x90,
			REG_COMMAND | ILI9341_SET_MEM_ACS_CTL, 0xC8,
			REG_COMMAND | ILI9341_SET_RGB_IF_SIG_CTL, 0xC2,
			REG_COMMAND | ILI9341_SET_FUNCTION_CTL, 0x0A, 0xA7, 0x27, 0x04,
			REG_COMMAND | ILI9341_SET_COL_ADDR, 0x00, 0x00, 0x00, 0xEF,
			REG_COMMAND | ILI9341_SET_PAGE_ADDR, 0x00, 0x00, 0x01, 0x3F,
			REG_COMMAND | ILI9341_SET_IF_CTL, 0x01, 0x00, 0x06,
			REG_COMMAND | ILI9341_SET_GAMMA, 0x01,
			REG_COMMAND | ILI9341_SET_PGAMMA,
				0x0F, 0x29, 0x24, 0x0C, 0x0E, 0x09, 0x4E, 0x78,
				0x3C, 0x09, 0x13, 0x05, 0x17, 0x11, 0x00,
			REG_COMMAND | ILI9341_SET_NGAMMA,
				0x00, 0x16, 0x1B, 0x04, 0x11, 0x07, 0x31, 0x33,
				0x42, 0x05, 0x0C, 0x0A, 0x28, 0x2F, 0x0F,
			REG_COMMAND | ILI9341_CMD_SLEEP_OFF,
			REG_DELAY   | 10,
			REG_COMMAND | ILI9341_CMD_DISPLAY_ON,
			REG_COMMAND | ILI9341_SET_MEM
	};

	const uint16_t	*p;

	acquire_bus(g);
	for(p = initdata; p < &initdata[sizeof(initdata)/sizeof(initdata[0])]; p++) {
		switch(*p & REG_TYPEMASK) {
		case REG_DATA:		write_data(g, *p);					break;
		case REG_COMMAND:	write_index(g, *p);					break;
		case REG_DELAY:		gfxSleepMilliseconds(*p & 0xFF);	break;
		}
	}
	release_bus(g);
}

static void LTDC_Reload(void) {
	LTDC->SRCR |= LTDC_SRCR_IMR;
	while (LTDC->SRCR & (LTDC_SRCR_IMR | LTDC_SRCR_VBR))
		gfxYield();
}

static void LTDC_LayerInit(LTDC_Layer_TypeDef *pLayReg, const ltdcLayerConfig * pCfg) {
	static const uint8_t fmt2Bpp[] = {
		4, /* LTDC_FMT_ARGB8888 */
		3, /* LTDC_FMT_RGB888 */
		2, /* LTDC_FMT_RGB565 */
		2, /* LTDC_FMT_ARGB1555 */
		2, /* LTDC_FMT_ARGB4444 */
		1, /* LTDC_FMT_L8 */
		1, /* LTDC_FMT_AL44 */
		2  /* LTDC_FMT_AL88 */
	};
	uint32_t start, stop;

	// Set the framebuffer dimensions and format
	pLayReg->PFCR = (pLayReg->PFCR & ~LTDC_LxPFCR_PF) | ((uint32_t)pCfg->fmt & LTDC_LxPFCR_PF);
	pLayReg->CFBAR = (uint32_t)pCfg->frame & LTDC_LxCFBAR_CFBADD;
	pLayReg->CFBLR = ((((uint32_t)pCfg->pitch << 16) & LTDC_LxCFBLR_CFBP) | (((uint32_t)fmt2Bpp[pCfg->fmt] * pCfg->width + 3) & LTDC_LxCFBLR_CFBLL));
	pLayReg->CFBLNR = (uint32_t)pCfg->height & LTDC_LxCFBLNR_CFBLNBR;

	// Set the display window boundaries
	start = (uint32_t)pCfg->x + driverCfg.hsync + driverCfg.hbackporch;
	stop  = start + pCfg->cx - 1;
	pLayReg->WHPCR = ((start <<  0) & LTDC_LxWHPCR_WHSTPOS) | ((stop  << 16) & LTDC_LxWHPCR_WHSPPOS);
	start = (uint32_t)pCfg->y + driverCfg.vsync + driverCfg.vbackporch;
	stop  = start + pCfg->cy - 1;
	pLayReg->WVPCR = ((start <<  0) & LTDC_LxWVPCR_WVSTPOS) | ((stop  << 16) & LTDC_LxWVPCR_WVSPPOS);

	// Set colors
	pLayReg->DCCR = pCfg->defcolor;
	pLayReg->CKCR = (pLayReg->CKCR & ~0x00FFFFFF) | (pCfg->keycolor & 0x00FFFFFF);
	pLayReg->CACR = (pLayReg->CACR & ~LTDC_LxCACR_CONSTA) | ((uint32_t)pCfg->alpha & LTDC_LxCACR_CONSTA);
	pLayReg->BFCR = (pLayReg->BFCR & ~(LTDC_LxBFCR_BF1 | LTDC_LxBFCR_BF2)) | ((uint32_t)pCfg->blending & (LTDC_LxBFCR_BF1 | LTDC_LxBFCR_BF2));
	for (start = 0; start < pCfg->palettelen; start++)
		pLayReg->CLUTWR = ((uint32_t)start << 24) | (pCfg->palette[start] & 0x00FFFFFF);

	// Final flags
	pLayReg->CR = (pLayReg->CR & ~LTDC_LEF_MASK) | ((uint32_t)pCfg->layerflags & LTDC_LEF_MASK);
}

static void LTDC_Init(void) {
	// Set up the display scanning
	uint32_t hacc, vacc;

	/* Reset the LTDC hardware module.*/
	RCC->APB2RSTR |= RCC_APB2RSTR_LTDCRST;
	RCC->APB2RSTR = 0;

	/* Enable the LTDC clock.*/
	RCC->DCKCFGR = (RCC->DCKCFGR & ~RCC_DCKCFGR_PLLSAIDIVR) | (1 << 16); /* /4 */

	// Enable the module
	RCC->APB2ENR |= RCC_APB2ENR_LTDCEN;

	// Turn off the controller and its interrupts.
	LTDC->GCR = 0;
	LTDC->IER = 0;
	LTDC_Reload();

	// Set synchronization params
	hacc = driverCfg.hsync - 1;
	vacc = driverCfg.vsync - 1;
	LTDC->SSCR = ((hacc << 16) & LTDC_SSCR_HSW) | ((vacc <<  0) & LTDC_SSCR_VSH);

	// Set accumulated back porch params
	hacc += driverCfg.hbackporch;
	vacc += driverCfg.vbackporch;
	LTDC->BPCR = ((hacc << 16) & LTDC_BPCR_AHBP) | ((vacc <<  0) & LTDC_BPCR_AVBP);

	// Set accumulated active params
	hacc += driverCfg.width;
	vacc += driverCfg.height;
	LTDC->AWCR = ((hacc << 16) & LTDC_AWCR_AAW) | ((vacc <<  0) & LTDC_AWCR_AAH);

	// Set accumulated total params
	hacc += driverCfg.hfrontporch;
	vacc += driverCfg.vfrontporch;
	LTDC->TWCR = ((hacc << 16) & LTDC_TWCR_TOTALW) | ((vacc <<  0) & LTDC_TWCR_TOTALH);

	// Set signal polarities and other flags
	LTDC->GCR = driverCfg.syncflags & (LTDC_EF_MASK & ~LTDC_EF_ENABLE);

	// Set background color
	LTDC->BCCR = (LTDC->BCCR & ~0x00FFFFFF) | (driverCfg.bgcolor & 0x00FFFFFF);

	// Load the background layer
	LTDC_LayerInit(LTDC_Layer1, &driverCfg.bglayer);

	// Load the foreground layer
	LTDC_LayerInit(LTDC_Layer2, &driverCfg.fglayer);

	// Interrupt handling
	//nvicEnableVector(STM32_LTDC_EV_NUMBER, CORTEX_PRIORITY_MASK(STM32_LTDC_EV_IRQ_PRIORITY));
	//nvicEnableVector(STM32_LTDC_ER_NUMBER, CORTEX_PRIORITY_MASK(STM32_LTDC_ER_IRQ_PRIORITY));
	// Possible flags - LTDC_IER_RRIE, LTDC_IER_LIE, LTDC_IER_FUIE, LTDC_IER_TERRIE etc
	LTDC->IER = 0;

	// Set everything going
	LTDC_Reload();
	LTDC->GCR |= LTDC_GCR_LTDCEN;
	LTDC_Reload();
}

LLDSPEC bool_t gdisp_lld_init(GDisplay *g) {

	// Initialize the private structure
	g->priv = 0;
	g->board = 0;
	//if (!(g->priv = gfxAlloc(sizeof(fbPriv))))
	//	gfxHalt("GDISP Framebuffer: Failed to allocate private memory");

	// Init the board
	init_board(g);
	//((fbPriv *)g->priv)->fbi.cfg = init_board(g);

	// Initialise the ILI9341 controller
	InitController(g);

	// Initialise the LTDC controller
	LTDC_Init();

	// Initialise DMA2D
	//dma2dStart(&DMA2DD1, &dma2d_cfg);
	//dma2d_test();

    // Finish Init the board
    post_init_board(g);

	/* Turn on the back-light */
	set_backlight(g, GDISP_INITIAL_BACKLIGHT);

	/* Initialise the GDISP structure */
	g->g.Width = driverCfg.bglayer.width;
	g->g.Height = driverCfg.bglayer.height;
	g->g.Orientation = GDISP_ROTATE_0;
	g->g.Powermode = powerOn;
	g->g.Backlight = GDISP_INITIAL_BACKLIGHT;
	g->g.Contrast = GDISP_INITIAL_CONTRAST;
	return TRUE;
}

LLDSPEC void gdisp_lld_draw_pixel(GDisplay *g) {
	unsigned	pos;

	#if GDISP_NEED_CONTROL
		switch(g->g.Orientation) {
		case GDISP_ROTATE_0:
		default:
			pos = PIXIL_POS(g, g->p.x, g->p.y);
			break;
		case GDISP_ROTATE_90:
			pos = PIXIL_POS(g, g->p.y, g->g.Width-g->p.x-1);
			break;
		case GDISP_ROTATE_180:
			pos = PIXIL_POS(g, g->g.Width-g->p.x-1, g->g.Height-g->p.y-1);
			break;
		case GDISP_ROTATE_270:
			pos = PIXIL_POS(g, g->g.Height-g->p.y-1, g->p.x);
			break;
		}
	#else
		pos = PIXIL_POS(g, g->p.x, g->p.y);
	#endif

		PIXEL_ADDR(g, pos)[0] = gdispColor2Native(g->p.color);
}

LLDSPEC	color_t gdisp_lld_get_pixel_color(GDisplay *g) {
	unsigned		pos;
	LLDCOLOR_TYPE	color;

	#if GDISP_NEED_CONTROL
		switch(g->g.Orientation) {
		case GDISP_ROTATE_0:
		default:
			pos = PIXIL_POS(g, g->p.x, g->p.y);
			break;
		case GDISP_ROTATE_90:
			pos = PIXIL_POS(g, g->p.y, g->g.Width-g->p.x-1);
			break;
		case GDISP_ROTATE_180:
			pos = PIXIL_POS(g, g->g.Width-g->p.x-1, g->g.Height-g->p.y-1);
			break;
		case GDISP_ROTATE_270:
			pos = PIXIL_POS(g, g->g.Height-g->p.y-1, g->p.x);
			break;
		}
	#else
		pos = PIXIL_POS(g, g->p.x, g->p.y);
	#endif

	color = PIXEL_ADDR(g, pos)[0];
	return gdispNative2Color(color);
}

#if GDISP_NEED_CONTROL
	LLDSPEC void gdisp_lld_control(GDisplay *g) {
		switch(g->p.x) {
		case GDISP_CONTROL_POWER:
			if (g->g.Powermode == (powermode_t)g->p.ptr)
				return;
			switch((powermode_t)g->p.ptr) {
			case powerOff: case powerOn: case powerSleep: case powerDeepSleep:
				// TODO
				break;
			default:
				return;
			}
			g->g.Powermode = (powermode_t)g->p.ptr;
			return;

		case GDISP_CONTROL_ORIENTATION:
			if (g->g.Orientation == (orientation_t)g->p.ptr)
				return;
			switch((orientation_t)g->p.ptr) {
				case GDISP_ROTATE_0:
				case GDISP_ROTATE_180:
					if (g->g.Orientation == GDISP_ROTATE_90 || g->g.Orientation == GDISP_ROTATE_270) {
						coord_t		tmp;

						tmp = g->g.Width;
						g->g.Width = g->g.Height;
						g->g.Height = tmp;
					}
					break;
				case GDISP_ROTATE_90:
				case GDISP_ROTATE_270:
					if (g->g.Orientation == GDISP_ROTATE_0 || g->g.Orientation == GDISP_ROTATE_180) {
						coord_t		tmp;

						tmp = g->g.Width;
						g->g.Width = g->g.Height;
						g->g.Height = tmp;
					}
					break;
				default:
					return;
			}
			g->g.Orientation = (orientation_t)g->p.ptr;
			return;

		case GDISP_CONTROL_BACKLIGHT:
			if ((unsigned)g->p.ptr > 100) g->p.ptr = (void *)100;
			set_backlight(g, (unsigned)g->p.ptr);
			g->g.Backlight = (unsigned)g->p.ptr;
			return;

		case GDISP_CONTROL_CONTRAST:
			if ((unsigned)g->p.ptr > 100) g->p.ptr = (void *)100;
			// TODO
			g->g.Contrast = (unsigned)g->p.ptr;
			return;
		}
	}
#endif

#endif /* GFX_USE_GDISP */
