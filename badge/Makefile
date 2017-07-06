##############################################################################
# Build global options
# NOTE: Can be overridden externally.
#

# Compiler options here.
ifeq ($(USE_OPT),)
  USE_OPT = -Os -ggdb -fomit-frame-pointer -falign-functions=16 -nostdlib
  USE_OPT += -DBIG_ENDIAN=0 -DLITTLE_ENDIAN=1 -Dasm=__asm__
  USE_OPT += -ansi  # -pedantic

# The following is a workaround for a broken rev2 prototype board where
# the radio DIO0 pin was not soldered correctly. This prevents us from
# getting RX interrupts from the radio. Enabling the below option hijacks
# one of the TPM timers so that we can use it to poll the radio instead.
# The polling frequency controls how many times per second we'll poll
# for traffic. This does not need to be enabled for any fully functional
# badges.
#
#USE_OPT += -DREV2_RADIO_WAR -DRADIO_POLL_FREQ=1000

# Enable joypad buttons. This should be turned on for production builds.
# This is optional because the Freescale/NXP Freedom KW019032 reference
# board has the pins wired up in such a way that if we enable the joypad
# interrupts, we'll get stuck forever thinking the buttons are being
# pressed. Only the real badge has the right joypad buttons anyway.
#
USE_OPT += -DENABLE_JOYPAD

# Enable this to support single player mode. You will have to generate
# enemies with the 'peersim' command or hold down RIGHT during boot.
USE_OPT += -DSINGLE_PLAYER

# Enable this to bypass startup splash page and sponsors
#
# USE_OPT += -DFAST_STARTUP

# Enable tilt sensor. Also only present on production board.
#
# USE_OPT+= -DENABLE_TILT_SENSOR

# enable this to enable admin and development modes. Enables all
# configuration utilities and other goodies reserved. Should not be
# used on production badges.
#
#USE_OPT += -DBLACK_BADGE

# This module will write out DTMF digits as sound files to the SD card.
# I don't know if we need this or not yet but I've finished the code,
# so I am checking it in. It consumes a fair amount of flash storage
# to do this, and it takes time to generate the DTMF audio files
# USE_OPT += -DMAKE_DTMF

# Enabling this will cause a (factory default) device to stop sending
# out pings so it cannot be identified. The device will also monitor
# and dump all pings as JSON for collection purposes. A similar
# feature is available by getting enabling the unlock UL_PINGDUMP on
# generic badges, minus the ability to disable pings.
#
#USE_OPT += -DLEADERBOARD_AGENT

endif

# C specific options here (added to USE_OPT).
ifeq ($(USE_COPT),)
  USE_COPT = -std=c99
endif

# C++ specific options here (added to USE_OPT).
ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -fno-rtti
endif

# Enable this if you want the linker to remove unused code and data
ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

# Linker extra options here.
ifeq ($(USE_LDOPT),)
  USE_LDOPT = 
endif

# Enable this if you want link time optimizations (LTO)
ifeq ($(USE_LTO),)
  USE_LTO = yes
endif

# If enabled, this option allows to compile the application in THUMB mode.
ifeq ($(USE_THUMB),)
  USE_THUMB = yes
endif

# Enable this if you want to see the full log while compiling.
ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

#
# Build global options
##############################################################################

##############################################################################
# Architecture or project specific options
#

# Stack size to be allocated to the Cortex-M process stack. This stack is
# the stack used by the main() thread.
ifeq ($(USE_PROCESS_STACKSIZE),)
  USE_PROCESS_STACKSIZE = 0x400
endif

# Stack size to the allocated to the Cortex-M main/exceptions stack. This
# stack is used for processing interrupts and exceptions.
ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  USE_EXCEPTIONS_STACKSIZE = 0x100
endif

#
# Architecture or project specific options
##############################################################################

##############################################################################
# Project, sources and paths
#

# Define project name here
PROJECT = badge

# Imported source files and paths
CHIBIOS = ..
# Startup files.
include $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC/mk/startup_kl1x.mk
# HAL-OSAL files (optional).
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/hal/ports/KINETIS/KL1x/platform.mk
include $(CHIBIOS)/os/hal/boards/NONSTANDARD_KW01_IDES_OF_MARCH/board.mk
include $(CHIBIOS)/os/hal/osal/rt/osal.mk
# RTOS files (optional).
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/rt/ports/ARMCMx/compilers/GCC/mk/port_v6m.mk
# Other files (optional).
include $(CHIBIOS)/test/rt/test.mk

# uGFX includes
GFXLIB =$(CHIBIOS)/ext/ugfx
include $(GFXLIB)/gfx.mk
include $(GFXLIB)/boards/base/Nonstandard-KW01-Ides-Of-March/board.mk
include $(GFXLIB)/drivers/gdisp/ILI9341/driver.mk
include $(GFXLIB)/drivers/ginput/touch/MCU/driver.mk

#C90TFS Freescale Flash drivers
LIBC90TFS = $(CHIBIOS)/ext/C90TFS
include $(LIBC90TFS)/build.mk

#FatFS library
FATFS = $(CHIBIOS)/ext/fatfs
include $(FATFS)/build.mk

# Define linker script file here
LDSCRIPT= $(STARTUPLD)/KL16Z128.ld

# C sources that can be compiled in ARM or THUMB mode depending on the global
# setting.

CSRC = main.c \
       strtoul.c \
       strstr.c \
       strcmp.c \
       dec2romanstr.c \
       datetime.c \
       strcasecmp.c \
       userconfig.c \
       orchard-shell.c \
       orchard-app.c \
       orchard-ui.c \
       orchard-vectors.c \
       cmd-mem.c \
       cmd-radio.c \
       cmd-flash.c \
       cmd-threads.c \
       cmd-buzzer.c \
       cmd-caesar.c \
       cmd-config.c \
       cmd-reset.c \
       cmd-peer.c \
       cmd-unix.c \
       ringbuf.c \
       proto.c \
       make-dtmf.c \
       app-launcher.c \
       app-name.c \
       app-notify.c \
       app-choosetype.c \
       app-fight.c \
       app-unlock.c \
       app-update.c \
       app-credits.c \
       app-dialer.c \
       app-ledsign.c \
       app-spectrum.c \
       app-video.c \
       app-music.c \
       app-photos.c \
       app-shout.c \
       app-chat.c \
       app-default.c \
       app-setup.c \
       app-email.c \
       tetris.c \
       app-tetris.c \
       ides_gfx.c \
       ui-keyboard.c \
       ui-list.c \
       rand.c \
       sound.c \
       led.c \
       oled.c \
       orchard-events.c \
       fix_fft.c \
       scroll_lld.c \
       video_lld.c \
       radio_lld.c \
       pit_lld.c \
       tpm_lld.c \
       dac_lld.c \
       dma_lld.c \
       xpt2046_lld.c \
       hex.c \
       flash.c \
       mmc_spi_lld.c \
       $(STARTUPSRC) \
       $(PORTSRC) \
       $(KERNSRC) \
       $(HALSRC) \
       $(OSALSRC) \
       $(PLATFORMSRC) \
       $(BOARDSRC) \
       $(CHIBIOS)/os/hal/lib/streams/chprintf.c \
       $(CHIBIOS)/os/hal/lib/streams/memstreams.c \
       $(CHIBIOS)/os/various/shell.c \
       $(GFXSRC) \
       $(LIBC90TFSSRC) \
       $(FATFSSRC)

# C++ sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CPPSRC =

# C sources to be compiled in ARM mode regardless of the global setting.
# NOTE: Mixing ARM and THUMB mode enables the -mthumb-interwork compiler
#       option that results in lower performance and larger code size.
ACSRC =

# C++ sources to be compiled in ARM mode regardless of the global setting.
# NOTE: Mixing ARM and THUMB mode enables the -mthumb-interwork compiler
#       option that results in lower performance and larger code size.
ACPPSRC =

# C sources to be compiled in THUMB mode regardless of the global setting.
# NOTE: Mixing ARM and THUMB mode enables the -mthumb-interwork compiler
#       option that results in lower performance and larger code size.
TCSRC =

# C sources to be compiled in THUMB mode regardless of the global setting.
# NOTE: Mixing ARM and THUMB mode enables the -mthumb-interwork compiler
#       option that results in lower performance and larger code size.
TCPPSRC =

# List ASM source files here
ASMSRC = $(STARTUPASM) $(PORTASM) $(OSALASM) ws2812b_ll.s

INCDIR = $(STARTUPINC) $(KERNINC) $(PORTINC) $(OSALINC) \
         $(LIBC90TFSINC) \
         $(FATFSINC) \
         $(HALINC) $(PLATFORMINC) $(BOARDINC) $(TESTINC) $(GFXINC) \
         $(CHIBIOS)/os/hal/lib/streams $(CHIBIOS)/os/various


#
# Project, sources and paths
##############################################################################

##############################################################################
# Compiler settings
#

MCU  = cortex-m0plus

HOSTCC=cc
#TRGT = arm-elf-
TRGT = arm-none-eabi-
CC   = $(TRGT)gcc
CPPC = $(TRGT)g++
# Enable loading with g++ only if you need C++ runtime support.
# NOTE: You can use C++ even without C++ support if you are careful. C++
#       runtime support makes code size explode.
LD   = $(TRGT)gcc
#LD   = $(TRGT)g++
CP   = $(TRGT)objcopy
AS   = $(TRGT)gcc -x assembler-with-cpp
OD   = $(TRGT)objdump
SZ   = $(TRGT)size
HEX  = $(CP) -O ihex
BIN  = $(CP) -O binary
SREC = $(CP) -O srec

# ARM-specific options here
AOPT =

# THUMB-specific options here
TOPT = -mthumb -DTHUMB -msoft-float

# Define C warning options here
CWARN = -Wall -Wextra -Wstrict-prototypes

# Define C++ warning options here
CPPWARN = -Wall -Wextra

#
# Compiler settings
##############################################################################

##############################################################################
# Start of user section
#

# Keypad layout selection:
#     BM      burning man key layout
#     BC1     bootcamp key layout v1
KEY_LAYOUT ?= BC1

# List all user C define here, like -D_DEBUG=1
UDEFS =

# Define ASM defines here
UADEFS =

# List all user directories here
UINCDIR =

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
#ULIBS = lib/armv6-m-libgcc.a lib/libc-nano-armv6-m.a
ULIBS = -lm -lc_nano -lnosys -lgcc

#
# End of user defines
##############################################################################

.PHONY: updatebuildtime

RULESPATH = $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC
include $(RULESPATH)/rules.mk

PRE_MAKE_ALL_RULE_HOOK: gitversion.h buildtime.h

buildtime.h: updatebuildtime
	echo "#define BUILDTIME \"Built: `/bin/date` (`/bin/hostname -s`)\"" > $@
	echo "#define BUILDVER `/bin/date +%s`" >> $@
	echo "#define BUILDMAGIC 0xCAFEBABE" >> $@

gitversion.h: updatebuildtime
	echo "static const char *gitversion = \"$(shell git rev-parse HEAD)\";" > $@

#POST_MAKE_ALL_RULE_HOOK: build/factory-test

#build/factory-test: factory-test.c
#	$(HOSTCC) factory-test.c -o build/factory-test -Wall -g

#factory-test.c:
