#!/bin/sh

rm -f hdr.rgb
rm -f out.raw

if [ "$1" == "" ]; then
    echo "Usage: $0 filename"
    exit
fi

filename=$1
newfilename=`basename $1`.rgb

width=`identify -format %w $filename`
height=`identify -format %h $filename`

./hdr $width $height
ffmpeg -vcodec jpegls -i $filename -vcodec rawvideo -f rawvideo -pix_fmt rgb565 out.raw

cat hdr.rgb out.raw > ${newfilename}

rm -f hdr.rgb
rm -f out.raw
