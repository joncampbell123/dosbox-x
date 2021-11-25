PDCurses Demos
==============

This directory contains demonstration programs to show and test the
capabilities of curses libraries. Some of them predate PDCurses,
PCcurses or even pcurses/ncurses. Although some PDCurses-specific code
has been added, all programs remain portable to other implementations
(at a minimum, to ncurses).


Building
--------

The demos are built by the platform-specific makefiles, in the platform
directories. Alternatively, you can build them manually, individually,
and link with any curses library; e.g., "cc -lcurses -orain rain.c".
There are no dependencies besides curses and the standard C library, and
no configuration is needed.


Distribution Status
-------------------

Public Domain, except for rain.c and worm.c, which are under the ncurses
license (MIT-like).
