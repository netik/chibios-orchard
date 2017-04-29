#include "orchard-app.h"
#include "orchard-ui.h"

#include "tetris.h"

static uint32_t
tetris_init (OrchardAppContext *context)
{
	(void)context;

	/*
	 * We don't want any extra stack space allocated for us.
	 * We'll use the heap.
	 */

	tetrisInit ();
	return (0);
}

static void
tetris_start (OrchardAppContext *context)
{
	(void)context;

	tetrisStart ();
	orchardAppExit ();

	return;
}

static void
tetris_event (OrchardAppContext *context,
	const OrchardAppEvent *event)
{
	(void)event;
	(void)context;
	return;
}

static void
tetris_exit (OrchardAppContext *context)
{
	(void)context;

	return;
}

orchard_app("Tetris", "tetris.rgb", 0, tetris_init,
    tetris_start, tetris_event, tetris_exit, 9999);
