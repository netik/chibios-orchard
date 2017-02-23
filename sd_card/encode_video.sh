#!/bin/sh
#
# Script to convert a video file into a collection of frames and
# an audio sample file.
#
# Usage is:
# encode_video.sh file.mp4 destination/directory/path
#
# The destination directory must already exist
# The video will have an absolute resolution of 150x112 pixels and
# a frame rate of 8 frames per second. These values have been chosen
# to coincide will with the audio sample rate of 8820Hz. The synchronization
# is not perfect: short videos should be used, because the loss of
# sync will become more pronounced the longer the video plays.
#

rm -f $2/video.bin

# Convert video to raw rgb565 pixel frames

ffmpeg -i "$1" -r 8 -s 128x96 -f rawvideo -pix_fmt rgb565 $2/video.bin

# Extract audio

ffmpeg -i "$1" -ac 1 -ar 8820 $2/sample.wav

# Convert to raw samples, boost gain a little

sox $2/sample.wav $2/sample.u16 contrast 80

# Finally, convert to 12-bit samples for DAC

mv $2/sample.u16 $2/sample.raw
./snd16to12 $2/sample.raw

rm -f $2/sample.wav
