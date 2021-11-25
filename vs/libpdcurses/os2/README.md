PDCurses for OS/2
=================

This directory contains PDCurses source code files specific to OS/2.


Building
--------

- Choose the appropriate makefile for your compiler:

        Makefile     - EMX 0.9b+
        Makefile.bcc - Borland C++ 2.0+
        Makefile.wcc - Open Watcom 1.8+

- Optionally, you can build in a different directory than the platform
  directory by setting PDCURSES_SRCDIR to point to the directory where
  you unpacked PDCurses, and changing to your target directory:

        set PDCURSES_SRCDIR=c:\pdcurses

- Build it:

        make -f makefilename

  (For Watcom, use "wmake" instead of "make".) You'll get the libraries
  (pdcurses.lib or .a, depending on your compiler), the demos (*.exe),
  and a lot of object files.


Distribution Status
-------------------

The files in this directory are released to the Public Domain.
