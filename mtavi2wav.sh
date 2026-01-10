#!/bin/sh
#
# Stupid multitrack audio hack for stupid Premiere.
# Turns a DOSBox-X multitrack AVI capture into several WAV files.
# Requires FFMPEG 3.0 or higher.

# get the track names. DOSBox-X always writes track names
NAMES=""
while IFS= read -r line; do
    line=$(echo "$line" | grep title | cut -d ':' -f 2 | sed -e 's/^ *//')
    [ -n "$line" ] && NAMES="${NAMES}${NAMES:+ }${line}"
done <<EOF
$(ffprobe "${1}" 2>&1)
EOF

# do it
i=0
for name in $NAMES; do
    name=$(echo "$name" | tr ' ' '_') # in case of spaces
    ffmpeg -i "${1}" -acodec copy -map "0:${i}" -vn -y -f wav "${1}.${i}-${name}.wav" || break
    i=$((i+1))
done
