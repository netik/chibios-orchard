all: rgbhdr 

rgbhdr: rgbhdr.c
	cc -g -o rgbhdr rgbhdr.c

images: rgbhdr
	./convertall.sh

sdcard: cleansd images midi
	@cp rgb/*.rgb sdcard
	@if [ ! -d "sdcard" ]; then mkdir sdcard; fi
	@find midi -name \*.bin -exec cp {} sdcard \;
	@cp ../updater/updater.bin sdcard
	@cp ../badge/build/badge.bin sdcard
	@mv -f sdcard/credits_mario.rgb sdcard/credits.rgb
	@echo
	@echo 'sdcard built!'
	@echo 
	du -kh sdcard

# copy the card to the right folder on osx
osxsd:	sdcard
	@echo "Copying to /Volumes/NO\ NAME/"
	@cp sdcard/* /Volumes/NO\ NAME/
	@sync

firmware:
	@if [ ! -d "sdcard" ]; then mkdir sdcard; fi
	@cp ../updater/updater.bin sdcard
	@cp ../badge/build/badge.bin sdcard

cleansd:
	@if [ -d "sdcard" ]; then rm -rf sdcard; fi; mkdir sdcard
	@if [ -d "rgb" ]; then rm -rf rgb; fi; mkdir rgb
	touch rgb/"THIS FOLDER IS AUTOMATICALLY GENERATED"

clean:
	rm rgbhdr

