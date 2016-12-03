#include "ch.h"
#include "hal.h"
#include "flash.h"

#include "SSD_FTFx.h"

#define FTFx_REG_BASE           0x40020000
#define P_FLASH_BASE            0x00000000
#define FLEXNVM_BASE            FSL_FEATURE_FLASH_FLEX_NVM_START_ADDRESS
#define EERAM_BASE              FSL_FEATURE_FLASH_FLEX_RAM_START_ADDRESS
#define P_FLASH_SIZE            (FSL_FEATURE_FLASH_PFLASH_BLOCK_SIZE * FSL_FEATURE_FLASH_PFLASH_BLOCK_COUNT)

#define READ_NORMAL_MARGIN        0x00
#define READ_USER_MARGIN          0x01
#define READ_FACTORY_MARGIN       0x02

#define DEBUGENABLE		0

const pFLASHCOMMANDSEQUENCE g_FlashLaunchCommand = FlashCommandSequence;

// Freescale's Flash Standard Software Driver Structure
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

void flashStart (void)
{
	FlashInit ((PFLASH_SSD_CONFIG)&flashSSDConfig);
	return;
}

void flashErase (uint8_t * buf)
{
	FlashEraseSector ((PFLASH_SSD_CONFIG)&flashSSDConfig,
                           (uint32_t)buf,
                           FTFx_PSECTOR_SIZE, g_FlashLaunchCommand);
	return;
}

void flashProgram (uint8_t * src, uint8_t * dst)
{
 	FlashProgram((PFLASH_SSD_CONFIG)&flashSSDConfig,
		(uint32_t) dst, FTFx_PSECTOR_SIZE, src, g_FlashLaunchCommand);

	return;
}

