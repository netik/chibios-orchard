#!/bin/bash

# take everything from my art folder and convert it.

cp /Volumes/Saferoom/Artwork/Badge/*.jpg jpg

# clean out the dir
rm jpg/mirrored/*
rm rgb/*

# update mirrors (Right-facing to left-facing)
for i in jpg/*.jpg; 
do
    newfn=jpg/mirrored/`basename $i .jpg`r.jpg
    newfn=`echo $newfn | tr A-Z a-z`

    convert -flop $i $newfn
    ./convert_to_rgb.sh $newfn
    mv jpg/mirrored/`basename $i .jpg`r.rgb rgb/${destfn}

    lh=`basename $i .jpg`r
    if [ ${#lh} -gt 8 ]; then
	echo "*** Warning $newfn is not 8.3 compatible!"
    fi
done

for i in jpg/*.jpg;
do
    ./convert_to_rgb.sh $i
    destfn=`basename $i .jpg`.rgb
    destfn=`echo $destfn | tr A-Z a-z`
    mv jpg/`basename $i .jpg`.rgb rgb/${destfn}

    lh=`basename ${i} .jpg`

    if [ ${#lh} -gt 8 ]; then
	echo "*** Warning $lh is not 8.3 compatible!"
    fi
done
