FATFSSRC+=					\
	$(FATFS)/src/ff.c			\
	$(FATFS)/src/diskio.c			\
	$(FATFS)/src/syscall.c

FATFSINC+= $(FATFS)/src

FATFSDEFS+= -DDRV_MMC=0 -UDRV_CFC
USE_COPT+= $(FATFSDEFS)
