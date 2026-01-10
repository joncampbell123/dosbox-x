#!/bin/sh
src="${1}"
dst="${1}.2x.mp4"

ffmpeg -i "${src}" -acodec copy -vcodec libx264 -g 15 -bf 2 -vf "scale=iw*2:ih*2:flags=neighbor" -vb 15000000 -preset superfast -g 15 -bf 2 -crf 8 -pix_fmt yuv422p -y -f mov "${dst}"

