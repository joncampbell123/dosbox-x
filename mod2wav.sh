#!/bin/bash
mikmod -o 16s -f 48000 -hq -p 0 -d wav,file="$1.wav" "$1"
