PDCurses for SDL 1.x
====================

This is a port of PDCurses for version 1.x of SDL.


Building
--------

- On *nix (including Linux and Mac OS X), run "make" in the sdl1
  directory. There is no configure script (yet?) for this port. This
  assumes a working sdl-config, and GNU make. It builds the library
  libpdcurses.a (dynamic lib not implemented).

- The makefile accepts the optional parameter "DEBUG=Y", and recognizes
  the optional PDCURSES_SRCDIR environment variable, as with the console
  ports. "WIDE=Y" builds a version that not only uses 16-bit Unicode
  characters, but depends on the SDL_ttf library, instead of using
  simple bitmap fonts. "UTF8=Y" makes PDCurses ignore the system locale,
  and treat all narrow-character strings as UTF-8; this option has no
  effect unless WIDE=Y is also set.


Distribution Status
-------------------

The files in this directory are released to the Public Domain.


Acknowledgements
----------------

Original SDL port was provided by William McBrine <wmcbrine@gmail.com>
TTF support is based on contributions by Laura Michaels.
