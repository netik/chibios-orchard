# Copyright (c) 2016
#      Bill Paul <wpaul@windriver.com>.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#      This product includes software developed by Bill Paul.
# 4. Neither the name of the author nor the names of any co-contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.


VPATH= ../os/hal/ports/KINETIS/KL1x ../os/hal/ports/common/ARMCMx	\
	../os/hal/src ../ext/fatfs/src ../ext/C90TFS/source ../badge

OBJS=	entry.o updater.o nvic.o pit.o ff.o diskio.o mmc_kinetis_spi.o	\
	flash.o FlashInit.o FlashProgram.o FlashEraseSector.o		\
	FlashCommandSequence.o FlashProgramCheck.o

CC=	arm-none-eabi-gcc
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

ASFLAGS= $(CFLAGS)

updater: $(OBJS)
	$(CC) $(OBJS) -ggdb -nostdlib -Wl,-Tlink.lds,--gc-sections	\
	-mcpu=cortex-m0plus -msoft-float -mthumb -flto 			\
	-nostartfiles -o $@ -lgcc
	$(OBJCOPY) -O binary $@ updater.bin

clean:
	rm -f updater *.o *.bin

all: updater
