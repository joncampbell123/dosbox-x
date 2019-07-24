@echo off
cd deps
nmake -f sdl clean
nmake -f zlib clean
nmake -f png clean
nmake -f pdcurses clean
cd ..
