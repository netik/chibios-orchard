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
	orientation_t o;

	(void)context;

	/*
	 * We need to use portrait orientation otherwise the playfield
	 * won't be fully visible. Once the game is over, we restore the
	 * orientation back to its previous setting.
	 */

	o = gdispGetOrientation ();
	gdispSetOrientation (0);
	tetrisStart ();
	gdispSetOrientation (o);

	chThdSleepMilliseconds (1000);

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
