#define _WITH_GETLINE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <stdint.h>

#define MIDI_NOTES	128
#define A_FREQ		440
#define DURATION_SCALE	(1/8.0)

#define CHORD_NOTES	256

#define PWM_DURATION_END        0xFF
#define PWM_DURATION_LOOP       0xFE
#define PWM_NOTE_PAUSE          0xFF
#define PWM_NOTE_OFF            0xFE

#define EVENT_TYPE_ON		1
#define EVENT_TYPE_OFF		2

typedef struct _chord {
	uint8_t			note_val;
} CHORD;

typedef struct _event {
	uint64_t		timestamp;
	uint8_t			notecnt;
	uint8_t			event_type;
	CHORD			chord[CHORD_NOTES];
	struct _event *		next;
} EVENT;

EVENT * pList = NULL;

char buf[BUFSIZ];

static uint64_t duration_get(uint64_t on, uint64_t off)
{
	uint64_t duration;
	double d;

	d = (float)(on - off);
	d *= DURATION_SCALE;
	d = round(d);

	return ((uint64_t)d);
}

static int
event_add (uint64_t timestamp, uint8_t event_type,
	   uint8_t note_val)
{
	EVENT * pNew;
	EVENT * pCur;
	EVENT * pLast;

	pCur = pList;

	while (pCur != NULL) {
		if (pCur->timestamp == timestamp &&
		    pCur->event_type == event_type)
			break;
		pCur = pCur->next;
	}

	/*
	 * Found an existing event of same type with same
	 * characteristics. This is a chord event: merge
	 * the note.
	 */

	if (pCur != NULL) {
		pCur->notecnt++;
		pCur->chord[pCur->notecnt - 1].note_val = note_val;
		return (0);
	}

	/* This is a new note */

	pNew = malloc (sizeof(EVENT));
	pNew->timestamp = timestamp;
	pNew->event_type = event_type;
	pNew->notecnt = 1;
	pNew->chord[0].note_val = note_val;
	pNew->next = NULL;

	/* Add it to the end of the list. */

	if (pList == NULL)
		pList = pNew;
	else {
		pCur = pList;
		while (pCur->next != NULL) {
			pCur = pCur->next;
		}
		pCur->next = pNew;
	}

	return (0);
}

#ifdef notdef
static void
chord_merge (void)
{
	EVENT * pCur;
	int i;
	double avg;

	pCur = pList;

	while (pCur != NULL) {
		if (pCur->notecnt > 1) {
			avg = 0;
			for (i = 0; i < pCur->notecnt; i++)
				avg += pCur->chord[i].note_val;
			avg /= pCur->notecnt;
			avg = round (avg);
			pCur->notecnt = 1;
			pCur->chord[0].note_val = avg;
		}
		pCur = pCur->next;
	}

	return;
}

#endif
static void
chord_merge (void)
{
	EVENT * pCur;
	int i;
	uint8_t smallest;

	pCur = pList;

	while (pCur != NULL) {
		if (pCur->notecnt > 1) {
			smallest = 255;
			for (i = 0; i < pCur->notecnt; i++) {
				if (pCur->chord[i].note_val < smallest)
					smallest = pCur->chord[i].note_val;
			}
			pCur->notecnt = 1;
			pCur->chord[0].note_val = smallest;
		}
		pCur = pCur->next;
	}

	return;
}

static void
event_dump (void)
{
	EVENT * pCur;
	FILE * out;
	uint8_t note[2];
	uint64_t note_time_on;
	uint64_t note_time_off;
	uint64_t tmp;
	int i;

	pCur = pList;

	out = fopen ("midi.out", "w");
	if (out == NULL) {
		perror ("file open failed");
		exit (1);
	}

	note_time_on = 0;
	note_time_off = 0;

	while (pCur != NULL) {
		if (pCur->event_type == EVENT_TYPE_ON) {
			note_time_on = pCur->timestamp;

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
						note[0] = PWM_NOTE_OFF;
						note[1] = 248;
						fwrite (&note, sizeof(note),
						    1, out);
                                                tmp -= 248;
                                                if (tmp == 0 || tmp < 248)
                                                        break;
                                        }
                                }
                                if (tmp != 0) {
                                        printf ("  { PWM_NOTE_OFF, %llu },\n",
                                            (unsigned long long)tmp);
					note[0] = PWM_NOTE_OFF;
					note[1] = tmp;
					fwrite (&note, sizeof(note), 1, out);
				}
			}
		}

		if (pCur->event_type == EVENT_TYPE_OFF) {
			note_time_off = pCur->timestamp;

			/* Node ended, print it out frequency and duration */

                        tmp = duration_get (note_time_off, note_time_on);
                        if (tmp > 248) {
                                 while (1) {
                                       printf ("  { %d, 248 },\n",
                                            pCur->chord[0].note_val);
					note[0] = pCur->chord[0].note_val;
					note[1] = 248;
					fwrite (&note, sizeof(note), 1, out);
                                        tmp -= 248;
                                        if (tmp == 0 || tmp < 248)
                                                break;
                                }
                        }
                        if (tmp != 0) {
                                printf ("  { %d, %llu },\n",
				    pCur->chord[0].note_val,
				    (unsigned long long)tmp);
				note[0] = pCur->chord[0].note_val;
				note[1] = tmp;
				fwrite (&note, sizeof(note), 1, out);
			}

		}
		pCur = pCur->next;
	}

	note[0] = 0;
	note[1] = PWM_DURATION_END;
	fwrite (&note, sizeof(note), 1, out);
	printf ("  { 0, PWM_DURATION_END }\n");

	fclose (out);

	return;
}


int main (int argc, char * argv[])
{
	FILE * fp;
	size_t linecap;
	ssize_t linelen;
	char * line;
	int dummy0;
	int dummy1;
	int dummy2;
	uint8_t event_type;
	unsigned long long note_time;
	int note_val;

	fp = fopen ("midi.csv", "r");

	if (fp == NULL) {
		perror ("file open failed");
		exit (1);
	}


	linecap = BUFSIZ;

	while (1) {
		line = buf;
		linelen = getline (&line, &linecap, fp);

		/* End of file */
		if (linelen == -1)
			break;

		buf[linelen - 1] = '\0';

		if (strstr (buf, "Note_on_c") != NULL) {
			sscanf (buf, "%d, %llu, Note_on_c, %d, %d, %d\n",
			    &dummy0, &note_time, &dummy1, &note_val,
			    &dummy2);
			if (dummy2 == 0)
				event_type = EVENT_TYPE_OFF;
			else
				event_type = EVENT_TYPE_ON;
			event_add (note_time, event_type,
				(uint8_t)(note_val & 0xFF));
		}

		if (strstr (buf, "Note_off_c") != NULL) {
			sscanf (buf, "%d, %llu, Note_off_c, %d, %d, %d\n",
			    &dummy0, &note_time, &dummy1, &note_val,
			    &dummy2);
			event_type = EVENT_TYPE_OFF;
			event_add (note_time, event_type,
				(uint8_t)(note_val & 0xFF));
		}

	}

	fclose (fp);

	chord_merge();

	event_dump();

	exit (0);
}
