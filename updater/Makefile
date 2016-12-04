
VPATH= ../os/hal/ports/KINETIS/KL1x ../os/hal/ports/common/ARMCMx	\
	../os/hal/src ../ext/fatfs/src ../ext/C90TFS/source ../badge

OBJS=	entry.o updater.o nvic.o pit.o ff.o diskio.o mmc_kinetis_spi.o	\
	flash.o FlashInit.o FlashProgram.o FlashEraseSector.o		\
	FlashCommandSequence.o FlashProgramCheck.o



CC=	arm-none-eabi-gcc
AS=	arm-none-eabi-as
LD=	arm-none-eabi-ld
OBJCOPY= arm-none-eabi-objcopy


CFLAGS+=  -mcpu=cortex-m0plus -Os -ggdb -fomit-frame-pointer		\
	-falign-functions=4 -nostdlib -DBIG_ENDIAN=0 -DLITTLE_ENDIAN=1	\
	-Dasm=__asm__ -ansi -pedantic -ffunction-sections		\
	-DUPDATER -fdata-sections -fno-common -std=c99			\
	-DCPU_MKL25Z128VLK4 -Wall -Wextra -Wstrict-prototypes		\
	-DCORTEX_USE_FPU=FALSE -DTHUMB_PRESENT -mno-thumb-interwork	\
	-DTHUMB_NO_INTERWORKING -mthumb -DTHUMB -msoft-float -I.	\
	-I../badge -I../os/common/ports/ARMCMx/devices/KL1x		\
	-I../os/ext/CMSIS/include -I../os/ext/CMSIS/KINETIS		\
	-I../os/rt/include -I../os/rt/ports/ARMCMx			\
	-I../os/rt/ports/ARMCMx/compilers/GCC				\
	-I../os/hal/osal/rt  -I../ext/C90TFS/include			\
	-I../os/hal/include -I../os/hal/ports/common/ARMCMx		\
	-I../os/hal/ports/KINETIS/KL1x -D__VTOR_PRESENT=1		\
	-I../ext/fatfs/src -DDRV_MMC=0 -UDRV_CFC -flto

updater: $(OBJS)
	$(CC) $(OBJS) -ggdb -nostdlib -Wl,-Tlink.lds,--gc-sections	\
	-mcpu=cortex-m0plus -msoft-float -mthumb -flto 			\
	-nostartfiles -o $@ -lgcc
	$(OBJCOPY) -O binary $@ updater.bin

clean:
	rm -f updater *.o *.bin

all: updater
