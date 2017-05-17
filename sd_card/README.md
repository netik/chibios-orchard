# Ides of DEF CON Badge: Asset Generation

This direcory contains tools to generate the assets for the badge. You can also use our tools to load your own audio, video, and images into the badge. 

To generate everything you must have GNU Make, a working C Compiler (GCC), ImageMagick, Sox, and FFMPEG available on your machine and in your path. We assume you have a Unix-like enviroment; We are building on Mac OS X and FreeBSD. 

## Building

Type `make all` to generate the binary tools.

Then type `make sdcard` to generate the assets.

To start over again, type `make clean` and optionally to erase the sdcard directory type `make cleansd`

When make finishes, you can then copy the `sdcard/` folder over to your sd card and insert it into the badge.
If you're on MacOSX and the SD Card is labelled as `/Volumes/SPQR_DC25`, you can type `make osxsd` and the Makefile will perform the copy for you. 

**NOTE!** If you remove and reinsert the SD card you must restart the entire badge (push reset!). **Our code does not detect SD card removal and replacement.**

## Audio

File extension: `.raw`

There are *two* audio facilities on the badge. A 12 bit DAC capable of playing mono audio at a sample rate of 9216 Hz, and a one-oscillator PWM line routed from PTB2 (PWM_SOUND) into the audio amplifier (LM386.) 

### For the DAC:

To convert audio, drop `.wafiles into the dac directory and run `make sdcard`

Audo playback runs in a separate thread and can be accessed with the `dacPlay("filename")` call. 

Additionally `dacStop()` stops audio, `dacWait()` halts the current thread until the current sample finishes. 

### For PWM audio

You can add short songs to sound.c like:

```
static const PWM_NOTE soundVictory[] = {
  { NOTE_D4, note16 },
  { NOTE_FS4, note16 },
  { NOTE_A4, note16 },
  { NOTE_D5, note16 },
  { PWM_NOTE_PAUSE, note16 * 2 },
  { NOTE_D4, note16 * 2 },
  { NOTE_D5, note16 * 3 },
  { 0, PWM_DURATION_END }
};
```

A MIDI-like note system is available in notes.h, and we have some (limited) conversion tools in `/chibios-orchard/sd_card/midi` as `midi.c` and `midi2.c` which will generate these structs.

Playback API

`playTone(NOTE_D4, 500);` to play one note for 500mS

`pwmToneStop();` to stop the current playing tone (does not stop thread)

`pwmThreadPlay (soundVictory);` to play the notes in the `soundNope` struct in the PWM thread.

`pwmThreadPlay (NULL);` to stop the background thread


## Video

File extension: `.vid`

The badge can play video at 8 frames per second with 12 bit mono audio. 

Video files must be converted manually. We don't have an option for auto converting them. In the tools directory, use the script `encode_video.sh` to convert video for the badge.

**Usage**

`tools/scripts/encode_video.sh yourmovie.mp4 outputdir`

Playback is done in software using the `videoPlay("filename")` call.

You can also play videos using the built-in video app.


## Images

File extension: `.rgb`

The badge can display images in the RGB565 format provided that the correct header is on the file. You can copy new TIF images into `tif/`, type make, and the Makefile will handle the conversion to `.rgb` files for you. 

You can then display these in the software by calling `putImagefile(x,y,'filename')`

## Photos App

The badge contains a photo-frame application that will let you show your own images. 

As a demo, we've loaded a few photos from our recent trip to Rome. Place JPGs into `photos_src/` and then type `make photos`.

Typing `make images` will also perform a build of the `photos/` sdcard directory.

Images must be 320x240 at 

## Icons

File extension: `.rgb`

For all of the icons, we used <http://game-icons.net> with the following color and stroke settings:

* Background color #000000
* Foreground color #F7C42B
* Shadow on, color #4A7CE6. X=0, Y=11
* Stroke color #F3C674
* Frame color #6267E3

Download them at 256x256 PNG, Convert the PNG to 80x80 TIF and save in the TIF directory. Icons are referenced at application setup time via the orchard_app API like so:

``orchard_app("Play Videos", "video.rgb", 0, video_init, video_start,video_event, video_exit, 9999);``

Each application registers itself using a call like these.

## Convenience methods

In the make file there are a few extras: 

`make osxsd` - Copy the sdcard dir to /Volumes/SPQR_DC25<br/>

`make osxsdfw` - Copy just the firmware and updater to /Volumes/SPQR_DC25

## Help?

If you have questions, feel free to @ reply or DM @spqrbadge on Twitter

