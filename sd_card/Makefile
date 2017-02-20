#
# SPQR / Ides of March asset build system
#
# John Adams 
# jna@retina.net
# 1/2017
#

# This makefile will build the tools and media that will be placed on
# the SD cards
#
# rgbhdr, ImageMagick, and FFMPeg are required to convert images. 
#

all: rgbhdr snd16to12

rgbhdr: rgbhdr.c
	cc -o rgbhdr rgbhdr.c

snd16to12: snd16to12.c
	cc -o snd16to12 snd16to12.c -lm

# This rule will build the mirrored and standard images only if the
# jpgs have changed. Rebuilding them all the time was time consuming.
rgb/%.rgb : jpg/%.jpg 
	convert -flop $< $(subst .jpg,r.jpg,$(subst jpg/,jpg/mirrored/,$<))
	./convert_to_rgb.sh $<
	mv $(subst rgb/,jpg/,$(@)) rgb/
	./convert_to_rgb.sh $(subst .jpg,r.jpg,$(subst jpg/,jpg/mirrored/,$<))
	mv $(subst .jpg,r.rgb,$(subst jpg/,jpg/mirrored/,$<)) rgb/

rgb/%.rgb : tif/%.tif
	convert -flop $< $(subst .tif,r.tif,$(subst tif/,tif/mirrored/,$<))
	./convert_to_rgb.sh $<
	mv $(subst rgb/,tif/,$(@)) rgb/
	./convert_to_rgb.sh $(subst .tif,r.tif,$(subst tif/,tif/mirrored/,$<))
	mv $(subst .tif,r.rgb,$(subst tif/,tif/mirrored/,$<)) rgb/

clean_images:
	$(shell rm jpg/mirrored/*)
	$(shell rm tif/mirrored/*)
	$(shell	rm rgb/*)

images: rgbhdr $(addprefix rgb/, $(notdir  $(addsuffix .rgb, $(basename $(wildcard jpg/*.jpg tif/*.tif)))))

sdcard: images midi
	@cp rgb/*.rgb sdcard
	@if [ ! -d "sdcard" ]; then mkdir sdcard; fi
	@find midi -name \*.bin -exec cp {} sdcard \;
	@cp ../updater/updater.bin sdcard
	@cp ../badge/build/badge.bin sdcard
	@[ -d $(dir sdcard/video) ] || mkdir -p sdcard/video
	@cp -R video/* sdcard/video
	@cp dac/*.raw sdcard
	@mv -f sdcard/credits_mario.rgb sdcard/credits.rgb
	@echo
	@echo 'sdcard built!'
	@echo
	@echo 'build #'
	@strings sdcard/badge.bin  | grep -1 gitrev | head -1
	du -kh sdcard

# copy the card to the right folder on my osx box
osxsd:	sdcard
	@echo "Copying to /Volumes/NO\ NAME/"
	@cp sdcard/* /Volumes/NO\ NAME/
	@sync

# only update the binaries to save time.
osxsdfw:	sdcard
	@echo "Updating binaries on /Volumes/NO\ NAME/"
	@cp ../updater/updater.bin /Volumes/NO\ NAME/
	@cp sdcard/badge.bin /Volumes/NO\ NAME/
	@sync

cleansd:
	@if [ -d "sdcard" ]; then rm -rf sdcard; fi; mkdir sdcard
	@if [ -d "rgb" ]; then rm -rf rgb; fi; mkdir rgb
	touch rgb/"THIS FOLDER IS AUTOMATICALLY GENERATED"

clean: clean_images
	rm rgbhdr
	rm snd16to12
