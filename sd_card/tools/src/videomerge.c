#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*
 * This program merges a video and audio stream together into a single file
 * for playback on the Kinetis KW01 using our video player. The video stream
 * is a file containing sequential 128x96 resolution frames in 16-bit RGB565
 * pixel format. The audio stream is a fine containing a stream of 12-bit
 * audio samples encoded at a rate of 9216Hz, stored 16 bits per sample.
 * We interleave the video and audio together, outputting two video scalines
 * 256 pixels) followed by two lines worth of audio samples (24 samples,
 * at a rate of 9216Hz and 96 sets of samples per frame).
 *
 * We drop the first video frame, as the player ends up playing the audio
 * slightly ahead of the video. Ignoring the first video frame seems to
 * restore synchronization.
 */

#define SAMPLE_CHUNKS		2

#define FRAME_WIDTH		128
#define FRAME_HEIGHT		96
#define SAMPLES_PER_FRAME	12

#define VID_BUFSZ		(FRAME_WIDTH * SAMPLE_CHUNKS)
#define AUD_BUFSZ		(SAMPLES_PER_FRAME * SAMPLE_CHUNKS)

int
main (int argc, char * argv[])
{
	FILE * audio;
	FILE * video;
	FILE * out;
	uint16_t scanline[VID_BUFSZ];
	uint16_t samples[AUD_BUFSZ];
	int i;

	if (argc > 4)
		exit (1);

	video = fopen (argv[1], "r+");

	if (video == NULL) {
		fprintf (stderr, "[%s]: ", argv[1]);
		perror ("file open failed");
		exit (1);
	}

	audio = fopen (argv[2], "r+");

        if (audio == NULL) {
		fprintf (stderr, "[%s]: ", argv[2]);
		perror ("file open failed");
		exit (1);
	}

	out = fopen (argv[3], "w");

	if (out == NULL) {
		fprintf (stderr, "[%s]: ", argv[3]);
		perror ("file open failed");
		exit (1);
	}

	/* Hack: skip the first frame. */

	for (i = 0; i < FRAME_HEIGHT; i++)
		fread (scanline, sizeof(scanline), 1, video);

	while (1) {
		if (fread (scanline, sizeof(scanline), 1, video) == 0)
			break;
		if (fread (samples, sizeof(samples), 1, audio) == 0)
			break;
		fwrite (scanline, sizeof(scanline), 1, out);
		fwrite (samples, sizeof(samples), 1, out);
	}

	fclose (video);
	fclose (audio);
	fclose (out);

	exit(0);
}
