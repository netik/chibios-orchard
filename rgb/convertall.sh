#!/bin/bash

# take everything from my art folder and convert it.

cp /Volumes/Saferoom/Artwork/Badge/*.jpg jpg

for i in jpg/*.jpg;
do
    ./convert_to_rgb.sh $i
    mv jpg/`basename $i .jpg`.rgb rgb/
done

