#!/bin/bash
file="$1"
bitrate=15000000
aspect_ratio=4:3
overscan_percent=10
final_height=720
final_width=960

# non-editable part
render_width=$((($final_width * (100-$overscan_percent))/100))
render_height=$((($final_height * (100-$overscan_percent))/100))

# announce
echo "Rendering as $render_width x $render_height encoding to $final_width x $final_height"

# go
ffmpeg -i "$file" -acodec aac -ab 384000 -vcodec libx264 -pix_fmt yuv420p -r 300000/1001 -vsync vfr -bsf:a aac_adtstoasc -vf "scale=$render_width:$render_height,pad=width=$final_width:height=$final_height:"'x=(ow-iw)/2:y=(oh-ih)/2' -vb "$bitrate" -minrate "$bitrate" -maxrate "$bitrate" -bufsize 8000000 -g 15 -bf 2 -threads 0 -aspect "$aspect_ratio" -y -f mp4 "$file.mp4" || exit 1

