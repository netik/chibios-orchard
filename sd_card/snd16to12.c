#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/*
 * This program takes a file containing 16-bit unsigned audio samples
 * and converts them into 12 bit samples. Unfortunately, audio utilities
 * tend to support resolutions such as 8, 16, 24 or 32 bits, but not 12.
 */

int
main (int argc, char * argv[])
{
	FILE * fp;
	uint16_t sample;
	float f;

	if (argc > 2)
		exit (1);

        fp = fopen (argv[1], "r+");

	if (fp == NULL) {
		perror ("file open failed");
		exit (1);
	}

	while (1) {
		if (fread (&sample, sizeof(sample), 1, fp) == 0)
			break;

		f = (float)sample;
		f *= 0.0625;
		f = round(f);
		sample = (uint16_t)f;

		fseek (fp, -2, SEEK_CUR);
		fwrite (&sample, sizeof(sample), 1, fp);
	}

	fclose (fp);

	exit (0);
	
}
