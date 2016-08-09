#!/bin/bash
src="$1"
dst="$1.1080p.mp4"

fwidth=1440
fheight=1080
# 95% title safe area
swidth=$((($fwidth*90)/100))
sheight=$((($fheight*90)/100))

ffmpeg -i "$src" -bsf:a aac_adtstoasc -acodec copy -vcodec libx264 -g 15 -bf 2 -vf "scale=$swidth:$sheight,pad=$fwidth:$fheight:(ow-iw)/2:(oh-ih)/2:black,setdar=4:3" -vb 15000000 -preset superfast -pix_fmt yuv420p -y -f mp4 "$dst"

