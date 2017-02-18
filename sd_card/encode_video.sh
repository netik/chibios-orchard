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
# a frame rate of 5 frames per second. These values have been chosen
# to coincide will with the audio sample rate of 8820Hz. The synchronization
# is not perfect: short videos should be used, because the loss of
# sync will become more pronounced the longer the video plays.
#

ffmpeg -i "$1" -vf fps=5 $2/out%05d.jpg

for i in `ls $2/out*.jpg`
do
	convert -resize 150x112\! $i $i
	./convert_to_rgb.sh $i
done

rm $2/*.jpg

ffmpeg -i "$1" -q:a 0 -map a $2/sample.wav
./sndmp3toraw.sh $2/sample.wav

rm $2/sample.wav
