#!/bin/bash
file=cma_shrt_000.mts
bitrate=15000000
aspect_ratio=4:3
ffmpeg -i "$file" -acodec copy -vcodec libx264 -pix_fmt yuv422p -r 300000/1001 -vsync vfr -bsf:a aac_adtstoasc -vf 'scale=864:648,pad=width=960:height=720:x=(ow-iw)/2:y=(oh-ih)/2' -vb "$bitrate" -minrate "$bitrate" -maxrate "$bitrate" -bufsize 8000000 -g 15 -bf 2 -threads 0 -aspect "$aspect_ratio" -y -f mp4 "$file.mp4" || exit 1

