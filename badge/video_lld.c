/*-
 * Copyright (c) 2017
 *      Bill Paul <wpaul@windriver.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This module implements a video playback routine for the Freescale/NXP
 * KW01 and a 320x240 display with ILI9341 chip. It's fairly rudimentary
 * and relies on several gimmicks. We're limited in terms of I/O performance
 * due to the fact that a) the SD card and screen share the same SPI port,
 * b) our system clock is limited to 48MHz and c) some SD cards are slower
 * than others.
 *
 * Best performance is achieved when using full speed SPI clock and an SD
 * card of class 4 or better. As of this writing, we run the SPI clock
 * at full speed using the micro SD card port on the East Rising display
 * module. Early prototype hardware using the MCU Friend display modules
 * and standard size SD cards was not fast enough to handle the maximum
 * rate and the clock was limited to half speed to compensate. Currently
 * we implement DMA with the SPI controller. This, in conjunction with
 * the full 12MHz SPI clock rate gives us sufficient I/O bandwidth even
 * with class 4 cards.
 *
 * We use a video frame resolution of 128x96 pixels and a frame rate of 8
 * frames per second. The 128x96 resolution is upscaled on the fly to the
 * full 320x240 resolution of the display. This is a little tricky because
 * it results in a ratio of 2.5 to 1. Upscaling by a factor of 2 would be
 * easy, but 2.5 is slightly more complicated. (You can't draw half a pixel.)
 *
 * The upscaling is accomplished using the following algorithm:
 *
 * - We always draw each pixel twice
 * - Every other pixel is drawn three times
 * - We also always draw every scanline twice
 * - Every other scanline is drawn three times
 *
 * This results in a cadence of 1:2, 1:3, 1:2, 1:3, etc... which yields
 * an overall increase of 2.5.
 *
 * Audio playback is accomplished using the DAC. Audio and video are stored
 * in the same file. The audio sample rate has been deliberately chosen to
 * be 9216Hz. This results in 12 audio samples for each 128-pixel scanline 
 * in a video frame at a rate of 8 frames per second. Audio is played by
 * loading samples into a buffer and having each sample written to the DAC
 * data register by an interrupt service routine tied to one of the interval
 * timers, which is set to trigger at the same frequency as the audio
 * sample rate.
 *
 * The video and audio are stored on the SD card in a single file with the
 * video pixel data and audio sample data interleaved using the following
 * pattern:
 *
 * [ 2 video scanlines (256 pixels) ][ 24 audio samples ]
 * [ 2 video scanlines (256 pixels) ][ 24 audio samples ]
 * [ 2 video scanlines (256 pixels) ][ 24 audio samples ]
 *
 * When playing back, each interleaved chunk is read back and the audio
 * is stored in a buffer while the video is written to the screen. When
 * enough audio samples have been buffered, the DAC is allowed to play
 * them.
 *
 * To maintain synchronization, each time a new chunk of samples is queued
 * up to play, we wait until the previous chunk of samples has completed
 * playing before proceeding. This kills two birds with one stone: first, it
 * ensures the video and audio stay in sync, and second it keeps both audio
 * and video playing at the correct rate. Since the video is synced to the
 * audio, as long as we play the audio at the correct rate, which is
 * enforced by the interval timer, the video will implicitly be forced to
 * play at the correct rate too.
 *
 * Encoding of the video and audio is done on a host system using ffmpeg.
 * The video is extracted as individual uncompressed frames in the native
 * RGB565 pixel format supported by the ILI9341 display controller. The
 * audio is converted to 16-bit monoaural raw sample data, and then processed
 * with a custom utility to convert the 16-bit samples to 12-bit samples
 * (which is the resulution limit of the DAC). Finally, another custom
 * program combines the raw video and audio samples together.
 *
 * If the SD card is too slow, things will still work, but there will be
 * pauses between the audio samples, which will make things sound choppy
 * or warbly. To compensate for this, an optional feature is available
 * which allows us to detect if we are processing samples faster than
 * we can load new ones, and if so we reduce the interval timer
 * frequency until we hit a rate that we can sustain. Hopefully this
 * condition should not arise very often with the production hardware.
 * With the currently available SD card I/O bandwidth, we no don't have
 * any problems keeping up, so this feature is disabled by default to
 * conserve code size.
 */

#include "ch.h"
#include "hal.h"
#include "osal.h"

#include "dac_lld.h"
#include "dac_reg.h"
#include "pit_lld.h"
#include "pit_reg.h"
#include "dma_lld.h"

#include "ff.h"
#include "ffconf.h"

#include "gfx.h"
#include "orchard-ui.h"

#include "src/gdisp/gdisp_driver.h"

/* Enable this if SD card I/O is too slow. */

#undef VIDEO_AUTO_RATE_ADJUST

#define SAMPLE_CHUNKS		32
#define SAMPLES_PER_LINE	12

#define FRAMERES_HORIZONTAL	128
#define FRAMERES_VERTICAL	96
#define FRAMES_PER_SECOND	8
#define LINE_RATE		2

#define SAMPLE_BYTES_PER_LINE	(SAMPLES_PER_LINE * 2)
#define BYTES_HORIZONTAL	(FRAMERES_HORIZONTAL * 2)
#define SAMPLES_PER_PLAY	(SAMPLES_PER_LINE * SAMPLE_CHUNKS) / LINE_RATE

#define VID_BUFSZ		(BYTES_HORIZONTAL * LINE_RATE)
#define VID_EXTRA		(SAMPLE_BYTES_PER_LINE * 2)
#define AUD_BUFSZ		(SAMPLE_BYTES_PER_LINE * SAMPLE_CHUNKS)

#define SAMPLE_INTERVAL		((SAMPLE_CHUNKS / 4) - 1)

/******************************************************************************
*
* write_pixel - write pixel data to the display
*
* This function is used to write pixel data to the ILI9341 screen controller.
* We use this in place of the routine in the ILI9341 driver in order to save
* a few cycles. We always write every pixel from the video data stream twice
* in order to upscale it. To achieve a scaling ration of 2.5:1, we sometimes
* write pixels three times. This function will always dump the necessary
* number of pixels to the SPI channel and wait for it to drain.
*
* RETURNS: N/A
*/

static void
write_pixel (pixel_t p1, int extra)
{
	volatile pixel_t * dst;

	dst = (pixel_t *)&SPI1->DL;

	dst[0] = p1;
	dst[0] = p1;
	if (extra)
		dst[0] = p1;

	while ((SPI1->S & SPIx_S_SPTEF) == 0)
		;

        return;
}

/******************************************************************************
*
* draw_lines - draw video scanlines on the display
*
* This is a helper function which takes one scanline's worth of data read
* from a video file and draws it to the screen. The <buf> argument is a
* pointer to a buffer containing 16-bit pixel data. The size is assumed
* to be the encoded horizontal video width (which is currently 128 pixels).
*
* Since the video is 128x96 pixels and the display is 320x240, we upscale
* each scanline by a factor of 2.5 so that we fill the whole screen. Each
* scanline is drawn at least twice. If the <extra> argument is non-zero,
* the scanline is drawn a third time as well. This results in each line
* being drawn 2.5 times. The write_pixel() function used to place each
* pixel on the screen uses a similar transformation.
*
* Note that we manually draw the pixels rather than using DMA to draw each
* display line. This is an intentional design trade-off. Since we perform
* a transformation on each scanline before writing it to the screen, we
* would have to allocate an additional RAM buffer and put the formatted
* pixel data therebefore we could use a DMA transfer to write it to the
* display. It turns out writing to the display without DMA is still fast
* enough that we can get away without doing that.
*
* RETURNS: N/A
*/

static void
draw_lines (pixel_t * buf, int extra)
{
	uint8_t p;

	for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
		write_pixel (buf[p], p & 1);
	}

	for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
		write_pixel (buf[p], p & 1);
	}

	if (extra) {
		for (p = 0; p < FRAMERES_HORIZONTAL; p++) {
			write_pixel (buf[p], p & 1);
		}
	}

	return;
}

/******************************************************************************
*
* videoWinPlay - play a video
*
* This function plays an encoded video file specified by <fname> from the
* SD card. The file must be encoded using the encode_video.sh script. There
* is no special header on the file. It contains raw RGB565 pixel data and
* 12-bit samples which are output to the ILI9341 controller and the DAC,
* respectively.
*
* The video will keep playing until the end of file is reached, or until the
* user touches the touch screen.
*
* If <x> and <y> are non-zero, then the video will be played at its native
* 128x96 resolution at the specified X and Y coordinates on the screen. If
* <x> and <y> are both 0, then the vide will be upscaled to consume the
* entire display.
*
* RETURNS: 0 when video finishes playing, or -1 if interrupted
*/

int
videoWinPlay (char * fname, int x, int y)
{
	GEventMouse * me = NULL;
	GListener gl;
	GSourceHandle gs;
	uint16_t * cur;
	uint16_t * ps;
	pixel_t * buf;
	FIL f;
	UINT br;
	int p;
	int i;
	int j;
#ifdef VIDEO_AUTO_RATE_ADJUST
	uint32_t rate;
	int lastwait;
	int curwait;
	int skipcnt;
#endif

	if (f_open (&f, fname, FA_READ) != FR_OK)
		return (0);

	/* Capture mouse up/down events */

	gs = ginputGetMouse (0);
	geventListenerInit (&gl);
	geventAttachSource (&gl, gs, GLISTEN_MOUSEMETA);

	/* start the video */

	/*
	 * This tells the graphics controller the dimensions of the
	 * drawing viewport. The drawing viewport is like a circular
	 * buffer: we can keep writing pixels to it sequentially and
	 * when we reach the end, the controller will automatically
	 * wrap back to the beginning. We tell the controller the
	 * drawing frame is 2.5 times the size of our actual video
	 * frame data, and then upscale on the fly.
	 */

	if (x == 0 && y == 0) {
		GDISP->p.x = 0;
		GDISP->p.y = 0;
		GDISP->p.cx = gdispGetWidth ();
		GDISP->p.cy = gdispGetHeight ();
	} else {
		GDISP->p.x = x;
		GDISP->p.y = y;
		GDISP->p.cx = FRAMERES_HORIZONTAL;
		GDISP->p.cy = FRAMERES_VERTICAL;
	}

	gdisp_lld_write_start (GDISP);
	gdisp_lld_write_stop (GDISP);

	/*
	 * Now that we've programmed the viewport, we set the X/Y
	 * coordinates to special values that tell the
	 * gdisp_lld_write_start function to skip programming
	 * the viewport area again. This saves us from calling
	 * the set_viewport() function for every scanline, which
	 * would be redundant: we're always just drawing data to
	 * the same window until the video compltes.
	 */

	GDISP->p.x = -1;
	GDISP->p.y = -1;

	buf = chHeapAlloc (NULL, VID_BUFSZ + VID_EXTRA);

	dacPlay (NULL);

	i = 0;
	j = 0;
	dacBuf = chHeapAlloc (NULL, AUD_BUFSZ);
	ps = dacBuf;
	cur = dacBuf;
#ifdef VIDEO_AUTO_RATE_ADJUST
	skipcnt = 0;
	curwait = 0;
	lastwait = 0;
#endif

	/* Enable the PIT for the DAC */

	pitEnable (&PIT1, 1);

	while (1) {
		/* Read two scan lines from the stream */

		f_read (&f, buf, VID_BUFSZ + VID_EXTRA, &br);

		if (br == 0)
			break;

		/* Extract the audio sample data */

		for (p = 0; p < SAMPLES_PER_LINE * 2; p++)
			ps[p] = buf[(VID_BUFSZ / 2) + p];

		/*
		 * When we have enough audio data buffered,
		 * start it playing.
		 */

		if ((i & SAMPLE_INTERVAL) == SAMPLE_INTERVAL) {
#ifdef VIDEO_AUTO_RATE_ADJUST
			curwait = dacSamplesWait ();
			if (curwait == 0 && lastwait == 0)
				skipcnt++;
			else
				skipcnt = 0;
			lastwait = curwait;
#else
			(void) dacSamplesWait ();
#endif
			dacSamplesPlay (cur, SAMPLES_PER_PLAY);
			if (cur == dacBuf)
				cur = dacBuf + SAMPLES_PER_PLAY; 
			else
				cur = dacBuf;
		}

		ps += (SAMPLES_PER_LINE * 2);
		i++;
		if (i == (SAMPLE_CHUNKS / 2)) {
			ps = dacBuf;
			i = 0;
		}

#ifdef VIDEO_AUTO_RATE_ADJUST
		/*
		 * This is a hack to deal with slow SD cards. If the
		 * card is too slow, audio will sound warbly due to
		 * delays caused by waiting for data to load. To avoid
		 * the warble, we reduce the PIT frequency a little
		 * to slow down the audio until the rate matches
		 * what the SD card can sustain.
		 *
		 * This should not be necessary now that we set the
		 * SPI clock to full speed for SD card access, but
		 * the code is kept around just in case.
		 */

		if (skipcnt > 5) {
			skipcnt = 0;
			rate = CSR_READ_4(&PIT1, PIT_LDVAL1);
 			rate += 100;
			CSR_WRITE_4(&PIT1, PIT_LDVAL1, rate);
		}
#endif

		/*
		 * Write video data to the screen. Note: we write each
                 * pixel and line two and a half times. This scales up the
		 * size of the displayed image, at the expense of pixelating
		 * it But hey, old school pixelated video is elite, right?
		 *
		 * In the case where we're drawing the video in the
		 * same 128x96 pixel format in which it was encoded,
		 * we can just DMA the pixels directly to the screen.
		 *
		 * Note that when we call gdisp_lld_write_start() here,
		 * it does not reset the viewport because we previously
		 * programmed the display X/Y coordinates to -1 before
		 * we began playing the video. This tells the start
		 * routine to skip the set_viewport() ste. We don't need
		 * it because the display chip will automatically wrap
		 * back to the start of the viewport once we finish drawing
		 * a frame. This means we only need to initialize the
		 * viewport once.
		 *
		 * Sadly, we still need to call the start/stop routines
		 * every time we draw to the screen because the sceen,
		 * SD card and touch controller are all on the same SPI
		 * channel. For performance, we program the SPI controller
		 * to use 16-bit mode when talking to to the screen, but we
		 * use 8-bit mode fo everything else. So we need the
		 * start/stop functions to switch the modes.
		 */

		gdisp_lld_write_start (GDISP);

		if (x == 0 && y == 0) {
			draw_lines (buf, j & 1);
			draw_lines (buf + FRAMERES_HORIZONTAL, j & 1);

			j++;
			if (j == FRAMERES_VERTICAL)
				j = 0;
		} else {
			dmaSend16 (buf, VID_BUFSZ);
		}

		gdisp_lld_write_stop (GDISP);

		/* Check for the user requesting exit. */

		me = (GEventMouse *)geventEventWait (&gl, 0);
		if (me != NULL && me->type == GEVENT_TOUCH)
			break;
	}

	geventDetachSource (&gl, NULL);
	chHeapFree (buf);
	chHeapFree (dacBuf);
	dacBuf = NULL;
	f_close (&f);


	pitDisable (&PIT1, 1);
	dacSamplesPlay (NULL, 0);

#ifdef VIDEO_AUTO_RATE_ADJUST
	/* Re-initialize the DAC's PIT frequency, in case we changed it. */

	CSR_WRITE_4(&PIT1, PIT_LDVAL1,
	    KINETIS_BUSCLK_FREQUENCY / DAC_SAMPLERATE);
#endif

	if (me != NULL && me->type == GEVENT_TOUCH)
		return (-1);

	return (0);
}

/******************************************************************************
*
* videoPlay - play a video
*
* This is a shortcut version of videoWinPlay() that always plays a video at
* full screen size.
*
* RETURNS: N/A
*/

int videoPlay (char * fname)
{
	return (videoWinPlay (fname, 0, 0));
}
