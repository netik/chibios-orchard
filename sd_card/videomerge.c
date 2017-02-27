#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define SAMPLE_CHUNKS		2

#define FRAME_WIDTH		(128 * SAMPLE_CHUNKS)
#define FRAME_HEIGHT		96
#define SAMPLES_PER_FRAME	(12 * SAMPLE_CHUNKS)

int
main (int argc, char * argv[])
{
	FILE * audio;
	FILE * video;
	FILE * out;
	uint16_t scanline[FRAME_WIDTH];
	uint16_t samples[SAMPLES_PER_FRAME];
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
