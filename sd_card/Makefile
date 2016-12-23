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
	@echo
	@echo 'sdcard built!'
	@echo 
	du -kh sdcard

cleansd:
	@if [ -d "sdcard" ]; then rm -rf sdcard; fi; mkdir sdcard
	@if [ -d "rgb" ]; then rm -rf rgb; fi; mkdir rgb
	touch rgb/"THIS FOLDER IS AUTOMATICALLY GENERATED"

clean:
	rm rgbhdr

