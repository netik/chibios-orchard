all: rgbhdr

rgbhdr: rgbhdr.c
	cc -g -o rgbhdr rgbhdr.c

images: rgbhdr
	./convertall.sh

sdcard: images midi

clean:
	rm rgbhdr

