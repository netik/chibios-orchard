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

#include "ch.h"
#include "hal.h"
#include "flash.h"

#include "SSD_FTFx.h"

static const pFLASHCOMMANDSEQUENCE g_FlashLaunchCommand = FlashCommandSequence;

/* Freescale's Flash Standard Software Driver Structure */
static const FLASH_SSD_CONFIG flashSSDConfig =
{
    FTFx_REG_BASE,          /* FTFx control register base */
    P_FLASH_BASE,           /* base address of PFlash block */
    P_FLASH_SIZE,           /* size of PFlash block */
    FLEXNVM_BASE,           /* base address of DFlash block */
    0,                      /* size of DFlash block */
    EERAM_BASE,             /* base address of EERAM block */
    0,                      /* size of EEE block */
    DEBUGENABLE,            /* background debug mode enable bit */
    NULL_CALLBACK           /* pointer to callback function */
};

void
flashStart (void)
{
	FlashInit ((PFLASH_SSD_CONFIG)&flashSSDConfig);
	return;
}

int8_t
flashErase (uint32_t offset, uint16_t count)
{
	int8_t r;
	uint8_t * buf;

	buf = (uint8_t *) (P_FLASH_BASE + (FTFx_PSECTOR_SIZE * offset));

	chSysLock ();
	r = FlashEraseSector ((PFLASH_SSD_CONFIG)&flashSSDConfig,
	    (uint32_t)buf, FTFx_PSECTOR_SIZE * count,
	    g_FlashLaunchCommand);
	chSysUnlock ();

	return (r);
}

int8_t
flashProgram (uint8_t * src, uint8_t * dst, uint32_t count)
{
	int8_t r;

	chSysLock ();
 	r = FlashProgram ((PFLASH_SSD_CONFIG)&flashSSDConfig,
	    (uint32_t) dst, count, src, g_FlashLaunchCommand);
	chSysUnlock ();

	return (r);
}

uint32_t
flashGetSecurity (void)
{
	uint8_t  securityStatus; 
	uint32_t ret;

	securityStatus = 0x0;
	ret = FlashGetSecurityState ((PFLASH_SSD_CONFIG)&flashSSDConfig,
	    &securityStatus);

	return ret;
}
