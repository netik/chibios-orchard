/*-
 * Copyright (c) 2016
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
 * This module implements a firmware update utility for the Freescale/NXP
 * Freedom KW019032 board with SD card connected via SPI channel 1.
 *
 * The firmware update process works in conjunction with a firmware update
 * app in the firmware already present in KW01's flash. The updater utility
 * itself is a standalone binary image which is stored on the SD card. It's
 * linked to execute near the top of the KW01's limited RAM. To perform an
 * update, the existing firmware loads the UPDATER.BIN image into RAM and
 * then simply jumps to it. At that point, the updater takes over full
 * control of the system.
 *
 * The updater makes use of the FatFs code to access the SD card and the
 * Freescale flash library to write to the KW01's flash. The FatFs code
 * is set up to use SPI channel 1 using the mmc_kinetis_spi.c module. The
 * one complication is that the mmc_kinetis_spi.c code requires a timer.
 * Normally this is provided using the KW01's periodic interval timer (PIT).
 * The trouble is that it that the way the code is written, it depends on
 * the timer triggering periodic interrupts. On the Cortex-M0 processor,
 * interrupts are routed via the vector table which is stored at page 0
 * in flash, which connects the PIT interrupt to a handler in the firmware.
 * We need to override this somehow so that we can get the timer interrupts
 * to call an interrupt handler in the updater instead.
 *
 * Fortunately, the KW01 is a Cortex-M0+ CPU, which supports the VTOR
 * register. This allows us to change the vector table offset and set up
 * replacement vector table in RAM. We can then program this table so that
 * the PIT vector is tied to the updater's interrupt service routine.
 *
 * The process to capture the interrupts is a little tricky. We have to
 * mask off all interrupts first and then disable all interrupt event
 * sources. In addition to the NVIC IRQs, we also have to turn off the
 * SysTick timer. Once we do that, no other interrupts should occur,
 * unless we crash the system and trigger a fault.
 *
 * Once the interrupt sources are disabled, we can then remap the
 * vector table and install a new PIT interrupt handler. Then we can
 * initialize the FatFs library and access the SD card.
 *
 * Note that we assume that the SPI channel has already been initalized
 * by the firmware that loaded the updater utility (since that requires
 * the use of the SD card too, this is a safe assumption to make).
 *
 * Once the firmware is updated, the updater will trigger soft reset of
 * the CPU to restart the system.
 *
 * Once it runs, the updater takes over full control of the CPU. All
 * previous RAM contents remain, but are ignored. The is kept small
 * in order to fit in about 5KB of memory so that it will fit in RAM
 * with enough space left over for some data buffers and the stack.
 *
 * The memory map when the updater runs is shown below:
 *
 * 0x1FFF_F800			stack
 * 0x2000_0000			FatFs structure
 * 0x2000_1000			1024-byte data buffer
 * 0x2000_1400			Start of updater binary
 */

#include "ch.h"
#include "hal.h"
#include "updater.h"
#include "mmc.h"
#include "pit.h"
#include "flash.h"

#include "ff.h"
#include "ffconf.h"

#include "SSD_FTFx.h"

/*
 * Per the manual, the vector table must be aligned on an 8-bit
 * boundary.
 */

__attribute__((aligned(0x100))) isr vectors[16 + CORTEX_NUM_VECTORS];

isr * origvectors = (isr *)0;

#define UPDATE_SIZE	FTFx_PSECTOR_SIZE /* 1024 bytes */

extern uint8_t bss;
extern uint8_t end;

int updater (void)
{
	FIL f;
	UINT br;
	int i;
	int total;
	uint8_t * p;
	uint8_t * src = (uint8_t *)0x20001000;
	uint8_t * dst = (uint8_t *)0x0;
	FATFS * fs = (FATFS *)0x20000000;

	/* Disable all interrupts */

	__disable_irq();
	SysTick->CTRL = 0;

	/* Clear the BSS. */

	p = (uint8_t *)&bss;
	total = &end - &bss;

	for (i = 0; i < total; i++)
		p[i] = 0;

	/*
	 * Copy the original vector table. We do this just as a debug
	 * aid: if we crash before getting to copy the firmware,
	 * the hardfault handler will catch us and we may be able to
	 * get a stack trace.
	 */

	for (i = 0; i < 16 + CORTEX_NUM_VECTORS; i++)
		vectors[i] = origvectors[i];

	/* Turn off all IRQs. */

	for (i = 0; i < CORTEX_NUM_VECTORS; i++)
		nvicDisableVector (i);

	/* Now shift the vector table location. */

	SCB->VTOR = (uint32_t)vectors;

	/* Now safe to re-enable interrupts */

	__enable_irq();

	/* Make sure the SD card chip select is in the right state */

	palSetPad (GPIOB, 1);	/* MMC/SD card */

	/* Enable the PIT (for the fatfs timer) */

	pitStart (&PIT1, disk_timerproc);

	/* Initalize flash library */

	flashStart ();

	/*
	 * Initialize and mount the SD card
	 * If this fails, turn on the red LED to signal a problem
	 * to the user.
	 */

	if (mmc_disk_initialize () != FR_OK ||
	    f_mount (fs, "0:", 1) != FR_OK ||
	    f_open (&f, "BADGE.BIN", FA_READ) != FR_OK) {
		palClearPad (GPIOE, 16);
		while (1) {}
	}

	/* Read data from the SD card and copy it to flash. */

	while (1) {
		f_read (&f, src, UPDATE_SIZE, &br);
		flashErase (dst);
		flashProgram (src, dst);
		dst += br;
		if (br < UPDATE_SIZE)
			break;
	}

	f_close (&f);

	/* Reboot and load the new firmware */

	NVIC_SystemReset ();

	/* NOTREACHED */
	return (0);
}