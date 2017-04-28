#
# SPQR / Ides of March asset build system
#
# John Adams 
# jna@retina.net
# 1/2017
#
# Master makefile
#
# This makefile will build the tools and media that will be placed on
# the SD cards
#
# ImageMagick, and FFMPeg are required to convert images.
# the Makefile in the tools/ subdir will build our required tools (rgbheader, snd
#
# videos will not be built by this script. We will only copy .vid
# files to the sdcard from the video/ subdir.
#

# The tools dir contains our conversion tools and scripts
TOOLS_DIR = tools
.PHONY: tools_dep

tools_dep:
	$(MAKE) -C $(TOOLS_DIR)

all: dirs tools_dep images sounds video

# These rules will build the mirrored and standard images only if the
# jpgs have changed.
#mv: rename jpg/font/led/font/led/20.rgb to rgb/font/led/20.rgb: No such file or directory

rgb/font/led/%.rgb : jpg/font/led/%.jpg 
	tools/scripts/convert_to_rgb.sh $<
	mv $(subst .jpg,.rgb,$<) $(@)

rgb/%.rgb : jpg/%.jpg 
	convert -flop $< $(subst .jpg,r.jpg,$(subst jpg/,jpg/mirrored/,$<))
	tools/scripts/convert_to_rgb.sh $<
	mv $(subst rgb/,jpg/,$(@)) rgb/
	tools/scripts/convert_to_rgb.sh $(subst .jpg,r.jpg,$(subst jpg/,jpg/mirrored/,$<))
	mv $(subst .jpg,r.rgb,$(subst jpg/,jpg/mirrored/,$<)) rgb/

rgb/%.rgb : tif/%.tif
	convert -flop $< $(subst .tif,r.tif,$(subst tif/,tif/mirrored/,$<))
	tools/scripts/convert_to_rgb.sh $<
	mv $(subst rgb/,tif/,$(@)) rgb/
	tools/scripts/convert_to_rgb.sh $(subst .tif,r.tif,$(subst tif/,tif/mirrored/,$<))
	mv $(subst .tif,r.rgb,$(subst tif/,tif/mirrored/,$<)) rgb/

dac/%.raw: dac/%.mp3
	tools/scripts/sndmp3toraw.sh $<

dac/%.raw: dac/%.wav
	tools/scripts/sndmp3toraw.sh $<

dac/fight/%.raw: dac/fight/%.mp3
	tools/scripts/sndmp3toraw.sh $<

# please stop adding non-TIF images to the collection, we want uncompressed media when possible.
images: tools_dep $(addprefix rgb/, $(notdir  $(addsuffix .rgb, $(basename $(wildcard tif/*.tif))))) \
	          $(addprefix rgb/font/led/, $(notdir  $(addsuffix .rgb, $(basename $(wildcard jpg/font/led/*.jpg))))) \
	          $(addprefix rgb/, $(notdir  $(addsuffix .rgb, $(basename $(wildcard jpg/*.jpg)))))

# please stop adding MP3s to the collection, we want uncompressed audio. It's already shitty enough.
sounds: tools_dep $(addprefix dac/, $(notdir  $(addsuffix .raw, $(basename $(wildcard dac/fight*.mp3))))) \
                  $(addprefix dac/, $(notdir  $(addsuffix .raw, $(basename $(wildcard dac/*.wav))))) \
	          $(addprefix dac/fight/, $(notdir  $(addsuffix .raw, $(basename $(wildcard dac/fight/*.mp3)))))


sdcard: dirs tools_dep images midi sounds
	@if [ ! -d "sdcard" ]; then mkdir sdcard; fi
	@cp rgb/*.rgb sdcard
	@if [ ! -d "sdcard/font/led" ]; then mkdir -p sdcard/font/led; fi
	@cp rgb/font/led/*.rgb sdcard/font/led
	@find midi -name \*.bin -exec cp {} sdcard \;
	@cp ../updater/updater.bin sdcard
	@cp ../badge/build/badge.bin sdcard
	@cp dac/*.raw sdcard
	@if [ ! -d "sdcard/fight" ]; then mkdir sdcard/fight; fi
	@cp dac/fight/*.raw sdcard/fight
	@cp video/*.vid sdcard
	@echo
	@echo 'sdcard built!'
	@echo
	@echo 'build #'
	@strings sdcard/badge.bin  | grep -1 gitrev | head -1
	du -kh sdcard

# copy the card to the right folder on my osx box
osxsd:	sdcard
	@echo "Copying to /Volumes/SPQR_DC25/"
	time cp -R sdcard/* /Volumes/SPQR_DC25/
	@sync

# copy only binaries and the updater to the card (reduces dev time
# when needed)
osxsdfw:	sdcard
	@echo "Updating binaries on /Volumes/SPQR_DC25"
	@cp ../updater/updater.bin /Volumes/SPQR_DC25/
	@cp sdcard/badge.bin /Volumes/SPQR_DC25
	@sync

# cleanups
clean_images:
	-rm -f jpg/mirrored/*
	-rm -f tif/mirrored/*
	-rm -f rgb/*

dirs:
	-mkdir sdcard
	-mkdir sdcard/font
	-mkdir sdcard/font/led
	-mkdir rgb
	-mkdir rgb/font
	-mkdir rgb/font/led

cleansd:
	@if [ -d "sdcard" ]; then rm -rf sdcard; fi; 
	@if [ -d "rgb" ]; then rm -rf rgb; fi; 
	touch rgb/"THIS FOLDER IS AUTOMATICALLY GENERATED"

# remove tools, sounds, images, and video
clean: clean_images
	$(MAKE) -C $(TOOLS_DIR) clean
	-rm -f dac/*.raw
	-rm -f dac/fight/*.raw

