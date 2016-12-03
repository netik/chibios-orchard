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

__attribute__((aligned(0x100))) isr vectors[48];

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
	uint32_t * fixup;
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

	for (i = 0; i < 48; i++)
		vectors[i] = origvectors[i];

	/* Turn off all IRQs. */

	for (i = 0; i < 32; i++)
		nvicDisableVector (i);

	/* Now shift the vector table address */

	SCB->VTOR = (uint32_t)vectors;

	/* Now safe to re-enable interrupts */

	__enable_irq();

	/* Make sure the SD card chip select is in the right state */

	palSetPad (GPIOB, 1);	/* MMC/SD card */

	/* Enable the PIT (for the fatfs timer) */

	pitStart (&PIT1, disk_timerproc);

	/* Initalize flash library */

	flashStart ();

	/* Initialize ahd mount the SD card */

	mmc_disk_initialize ();

	f_mount (fs, "0:", 1);

	if (f_open (&f, "BADGE.BIN", FA_READ) != FR_OK)
		while (1) {}

	while (1) {
		f_read (&f, src, UPDATE_SIZE, &br);

		/*
		 * This fixup is needed to ensure that the flash
		 * security bits are set correctly. In the compiled
	 	 * firmware image, the bytes at address 0x400 are all
		 * zeros. There are actually four registers here which
	 	 * are used to set the security state of the processor.
		 * If all the bits get zeroed out during a firmware
		 * update, the chip will be locked and we won't be
		 * able to debug anymore. To avoid this, we force
		 * these locations to the unsecured state before
		 * writing the block at address 0x400.
		 */

		if (dst == (uint8_t *)0x400) {
			fixup = (uint32_t *)src;
			fixup[0] = 0xFFFFFFFF;
			fixup[1] = 0xFFFFFFFF;
			fixup[2] = 0xFFFFFFFF;
			fixup[3] = 0xFFFFFFFE;
		}

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
