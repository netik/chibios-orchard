#!/bin/sh
#
# SPQR/Ides of March Asset Build System
# 1/2017
#
# John Adams <jna@retina.net> @netik
# Bill Paul <wpaul@windriver.com>
#
# Script to convert a video file into the format used by the
# the video player on the Ides of DEF CON badge. The format isn't
# very complicated: a .VID file contains raw RGB565 pixel data
# and raw 12-bit audio sample data. The player just loads the data
# and transfers it to the screen and DAC as fast as it can.
#
# Usage is:
# encode_video.sh file.mp4 destination/directory/path
#
# Required utilities:
# ffmpeg
# sox
#
# The destination directory must already exist
#
# The video will have an absolute resolution of 128x96 pixels and
# a frame rate of 8 frames per second. These values have been chosen
# to coincide with the audio sample rate of 9216Hz. The video
# and audio will be synchronized during playback.
#

rm -f $2/video.bin

# Convert video to raw rgb565 pixel frames at 8 frames/sec
ffmpeg -i "$1" -r 8 -s 128x96 -f rawvideo -pix_fmt rgb565 $2/video.bin

# Extract audio
ffmpeg -i "$1" -ac 1 -ar 9216 $2/sample.wav

# Convert WAV to to raw unsigned 16 bit samples, boost gain a little
sox $2/sample.wav $2/sample.u16 contrast 80

# Convert 16-bit audio samples to 12-bit samples for DAC
mv $2/sample.u16 $2/sample.raw
./bin/snd16to12 $2/sample.raw

rm -f $2/sample.wav

# Now merge the video and audio into a single file
./bin/videomerge $2/video.bin $2/sample.raw $2/video.vid

rm -f $2/video.bin $2/sample.raw
