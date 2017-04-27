#!/bin/sh

# SPQR/Ides of March Asset Build System
# 1/2017
#
# John Adams <jna@retina.net> @netik
# Bill Paul <wpaul@windriver.com>
#
# sndmp3toraw.sh
#
# Convert mp3 to 8Khz audio, then downconvert to 12bits for our DAC
#

prefix="`echo $1 | cut -d . -f 1`"

echo "$prefix"
sox $1 $prefix.u16 channels 1 rate 9216 contrast 80

mv $prefix.u16 $prefix.raw
tools/bin/snd16to12 $prefix.raw
