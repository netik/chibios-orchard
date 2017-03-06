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

#include "orchard-app.h"
#include "orchard-ui.h"
#include "video_lld.h"
#include "rand.h"


static uint32_t
hackers_init(OrchardAppContext *context)
{
	(void)context;
	return (0);
}

static void
hackers_start(OrchardAppContext *context)
{
	char * track;
	int i;

	(void)context;

	i = rand () % 5;

	switch (i) {
		case 0:
			track = "hackers.vid";
			break;
		case 1:
			track = "bill.vid";
			break;
		case 2:
			track = "drwho.vid";
			break;
		case 3:
			track = "hypnotod.vid";
			break;
		case 4:
		default:
			track = "hackplnt.vid";
			break;
	}

	videoPlay (track);

	orchardAppExit ();

	return;
}

static void
hackers_event(OrchardAppContext *context, const OrchardAppEvent *event)
{
	(void)context;
	(void)event;

	return;
}

static void
hackers_exit(OrchardAppContext *context)
{
	(void)context;
	return;
}

orchard_app("Hackers", /*APP_FLAG_HIDDEN|*/APP_FLAG_AUTOINIT,
	hackers_init, hackers_start, hackers_event, hackers_exit);
