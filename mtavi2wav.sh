#!/bin/bash
#
# Stupid multitrack audio hack for stupid Premiere.
# Turns a DOSBox-X multitrack AVI capture into several WAV files.
# Requires FFMPEG 3.0 or higher.

# get the track names. DOSBox-X always writes track names
declare -a NAMES
for x in `ffprobe dosbox_000.mt.avi 2>&1 | grep title | cut -d ':' -f 2 | sed -e 's/^ *//'`; do
    NAMES=("${NAMES[@]}" "$x")
done

# do it
i=0
for name in ${NAMES[@]}; do
    name=`echo "$name" | sed -e 's/ /_/'` # in case of spaces
    ffmpeg -i "$1" -acodec copy -map 0:$i -vn -y -f wav "$1.$i-$name.wav" || break
    i=$(($i+1))
done

