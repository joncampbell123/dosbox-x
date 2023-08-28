#!/usr/bin/env bash
src="${1}"
dst="${1}.2x.mp4"

ffmpeg -i "${src}" -bsf:a aac_adtstoasc -acodec copy -vcodec libx264 -g 15 -bf 2 -vf "scale=iw*2:ih*2:flags=neighbor" -vb 15000000 -preset superfast -pix_fmt yuv420p -y -f mp4 "${dst}"

