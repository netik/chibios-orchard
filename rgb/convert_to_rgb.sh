#!/bin/sh

rm -f hdr.rgb
rm -f out.raw

width=`identify -format %w $1`
height=`identify -format %h $1`

./hdr $width $height
ffmpeg -vcodec jpegls -i $1 -vcodec rawvideo -f rawvideo -pix_fmt rgb565 out.raw

cat hdr.rgb out.raw > out.rgb

rm -f hdr.rgb
rm -f out.raw
