@echo off
cd deps
nmake -f sdl
nmake -f zlib
nmake -f png
nmake -f pdcurses
cd ..
