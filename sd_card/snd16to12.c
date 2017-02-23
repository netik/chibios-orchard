#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/*
 * This program takes a file containing 16-bit unsigned audio samples
 * and converts them into 12 bit samples. Unfortunately, audio utilities
 * tend to support resolutions such as 8, 16, 24 or 32 bits, but not 12.
 * We need 12-bit resolution for the DAC in the Freescale KW01 chip.
 */

#define SAMPLE_CHUNK	1024

int
main (int argc, char * argv[])
{
	FILE * fp;
	size_t cnt;
	size_t off;
	uint16_t samples[1024];
	int i;
	float f;

	if (argc > 2)
		exit (1);

        fp = fopen (argv[1], "r+");

	if (fp == NULL) {
		perror ("file open failed");
		exit (1);
	}

	off = 0;

	while (1) {
		cnt = fread (samples, sizeof(uint16_t), SAMPLE_CHUNK, fp);
		if (cnt == 0)
			break;

		for (i = 0; i < cnt; i++) {
			f = (float)samples[i];
			f *= 0.0625;
			f = round(f);
			samples[i] = (uint16_t)f;
		}
		if (fseek (fp, off, SEEK_SET) != 0)
			break;
		cnt = fwrite (samples, sizeof(uint16_t), cnt, fp);
		off += (cnt * 2);
	}

	fclose (fp);

	exit (0);
	
}
