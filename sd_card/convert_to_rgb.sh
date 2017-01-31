#!/bin/sh

#
# convert any readable image into a rgb565 image with proper header.
#
# requires ffmpeg, imagemagick, and rgbhdr
#

FFMPEG=/usr/local/bin/ffmpeg
IDENTIFY=/usr/local/bin/identify
RGBHDR=./rgbhdr

rm -f hdr.rgb
rm -f out.raw

if [ "$1" == "" ]; then
    echo "Usage: $0 filename"
    exit
fi

filename=$1
newfilename=${filename%.*}.rgb
newfilename=`echo ${newfilename} | tr A-Z a-z`
width=`${IDENTIFY} -format %w $filename`
height=`${IDENTIFY} -format %h $filename`

echo $newfilename
${RGBHDR} $width $height

CODEC=jpegls

if [[ $filename == *.tif ]];
then
    CODEC=tiff
fi

   


${FFMPEG} -loglevel panic -vcodec ${CODEC} -i $filename -vcodec rawvideo -f rawvideo -pix_fmt rgb565 out.raw 

cat hdr.rgb out.raw > ${newfilename}
    
rm -f hdr.rgb
rm -f out.raw

