#!/bin/sh

prefix="`echo $1 | cut -d . -f 1`"

echo "$prefix"
sox $1 $prefix.u16 channels 1 rate 9216 contrast 80

mv $prefix.u16 $prefix.raw
./snd16to12 $prefix.raw
