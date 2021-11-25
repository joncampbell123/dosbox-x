PDCurses for SDL 2.x
====================

This is a port of PDCurses for version 2.x of SDL (aka SDL2).


Building
--------

- On *nix (including Linux and Mac OS X), run "make" in the sdl2
  directory. There is no configure script (yet?) for this port. This
  assumes a working sdl-config, and GNU make. It builds the library
  libpdcurses.a (dynamic lib not implemented).

- With MinGW, edit the Makefile to point to the appropriate include and
  library paths, and then run "make".

- The makefile recognizes the optional PDCURSES_SRCDIR environment
  variable, and the option "DEBUG=Y", as with the console ports.
  "WIDE=Y" builds a version that not only uses 16-bit Unicode
  characters, but depends on the SDL2_ttf library, instead of using
  simple bitmap fonts. "UTF8=Y" makes PDCurses ignore the system locale,
  and treat all narrow-character strings as UTF-8; this option has no
  effect unless WIDE=Y is also set.


Distribution Status
-------------------

The files in this directory are released to the Public Domain.


Acknowledgements
----------------

The original SDL port was provided by William McBrine.

The initial SDL2 support patch was created by Laura Michaels.

The SDL2 port was put together and further developed by Robin Gustafsson.
