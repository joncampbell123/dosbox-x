PDCurses for Windows console
============================

This directory contains PDCurses source code files specific to the
Microsoft Windows console. Although historically called "Win32", this
port can just as easily be built for 64-bit systems. Windows 95 through
Windows 10 are covered. (Some features require later versions.)


Building
--------

- Choose the appropriate makefile for your compiler:

        Makefile      - GNU Compiler (MinGW or Cygnus)
        Makefile.bcc  - Borland C++ 7.0+
        Makefile.vc   - Microsoft Visual C++ 2.0+
        Makefile.wcc  - Open Watcom 1.8+

- Optionally, you can build in a different directory than the platform
  directory by setting PDCURSES_SRCDIR to point to the directory where
  you unpacked PDCurses, and changing to your target directory:

        set PDCURSES_SRCDIR=c:\pdcurses

- Build it:

        make -f makefilename

  (For Watcom, use "wmake" instead of "make"; for MSVC, "nmake".) You'll
  get the libraries (pdcurses.lib or .a, depending on your compiler),
  the demos (*.exe), and a lot of object files.

  You can also give the optional parameter "WIDE=Y", to build the
  library with wide-character (Unicode) support:

        wmake -f Makefile.wcc WIDE=Y

  When built this way, the library is not compatible with Windows 9x,
  unless you also link with the Microsoft Layer for Unicode (not
  tested).

  Another option, "UTF8=Y", makes PDCurses ignore the system locale, and
  treat all narrow-character strings as UTF-8. This option has no effect
  unless WIDE=Y is also set. Use it to get around the poor support for
  UTF-8 in the Windows console:

        make -f Makefile.bcc WIDE=Y UTF8=Y

  You can also use the optional parameter "DLL=Y" with Visual C++,
  MinGW or Cygwin, to build the library as a DLL:

        nmake -f Makefile.vc WIDE=Y DLL=Y

  When you build the library as a Windows DLL, you must always define
  PDC_DLL_BUILD when linking against it. (Or, if you only want to use
  the DLL, you could add this definition to your curses.h.)


Distribution Status
-------------------

The files in this directory are released to the Public Domain.


Acknowledgements
----------------

Windows console port was originally provided by Chris Szurgot
<szurgot@itribe.net>
