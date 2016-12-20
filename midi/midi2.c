#define _WITH_GETLINE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MIDI_NOTES	128
#define A_FREQ		440
#define DURATION_SCALE	(3/8.0)

int midifreq[MIDI_NOTES];

char buf[BUFSIZ];

static long long duration_get(long long on, long long off)
{
	long long duration;
	double d;

	d = (float)(on - off);
	d *= DURATION_SCALE;
	d = round(d);

	return ((long long)d);
}


int main (int argc, char * argv[])
{
	FILE * fp;
	size_t linecap;
	ssize_t linelen;
	char * line;
	double ex;
	double freq;
	int i;
	long long note_time_on;
	long long note_time_off;
	long long tmp;
	long long duration;
	int note_val;
	int dummy0;
	int dummy1;
	int dummy2;

	/* Create the midi note to frequency conversion table */

	for (i = 0; i < MIDI_NOTES; i++) {
		ex = (double)(i - 9) / 12.0;
		freq = (A_FREQ / 32.0) * pow (2.0, ex);
		midifreq[i] = (int)round(freq);
	}

	fp = fopen ("midi.csv", "r");

	if (fp == NULL) {
		perror ("file open failed");
		exit (1);
	}

	linecap = BUFSIZ;
	note_time_on = 0;
	note_time_off = 0;

	while (1) {
		line = buf;
		linelen = getline (&line, &linecap, fp);

		/* End of file */
		if (linelen == -1)
			break;

		buf[linelen - 1] = '\0';

		if (strstr (buf, "Note_on_c") != NULL) {
			sscanf (buf, "%d, %lld, Note_on_c, %d, %d, %d\n",
			    &dummy0, &duration, &dummy1,
			    &note_val, &dummy2);

			if (dummy2 == 0) {
				note_time_off = duration;
				tmp = duration_get (note_time_off,
				    note_time_on);
				if (tmp > 248) {
					while (1) {
						printf ("  { %d, 248 },\n",
						    note_val);
						tmp -= 248;
						if (tmp == 0 || tmp < 248)
							break;
					}
				}
				if (tmp != 0)
					printf ("  { %d, %lld },\n", note_val,
					    tmp);
				note_time_on = duration;
			} else {
				note_time_on = duration;
			}

			/*
	 		 * If this note doesn't begin immediately after the
			 * previous one ended, then generate a pause.
			 */

			if ((note_time_on - note_time_off) != 0) {
				tmp = duration_get (note_time_on,
				    note_time_off);
				if (tmp > 248) {
					while (1) {
					printf ("  { PWM_NOTE_OFF, 248 },\n");
						tmp -= 248;
						if (tmp == 0 || tmp < 248)
							break;
					}
				}
				if (tmp != 0)
					printf ("  { PWM_NOTE_OFF, %lld },\n",
					    tmp);
			}
		}
#ifdef notdef
		if (strstr (buf, "Note_off_c") != NULL) {
			sscanf (buf, "%d, %lld, Note_off_c, %d, %d, %d\n",
			    &dummy0, &note_time_off, &dummy1, &note_val,
			    &dummy2);

			/* Node ended, print it out frequency and duration */

			printf ("  { %d, %lld },\n", note_val,
		    	    duration_get (note_time_off, note_time_on));
		}
#endif
	}

	printf ("  { 0, PWM_DURATION_END }\n");

	fclose (fp);

	exit (0);
}
