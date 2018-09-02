PDCurses for X11
================

This is a port of PDCurses for X11, aka XCurses.  It is designed to
allow existing curses programs to be re-compiled with PDCurses,
resulting in native X11 programs.


Building
--------

- Run "./configure". To build the wide-character version of the library,
  specify "--enable-widec" as a parameter. To use X Input Methods, add
  "--enable-xim". I recommend these options, but I haven't yet made
  them the defaults, for the sake of backwards compatibility.

  If your system is lacking in UTF-8 support, you can force the use of
  UTF-8 instead of the system locale via "--enable-force-utf8".

  If configure can't find your X include files or X libraries, you can
  specify the paths with the arguments "--x-includes=inc_path" and/or
  "--x-libraries=lib_path".

  By default, the library and demo programs are built with the optimizer
  switch -O2. You can turn this off, and turn on debugging (-g), by
  adding "--with-debug" to the configure command.

- Run "make". This should build libXCurses and all the demo programs.

- Optionally, run "make install". curses.h and panel.h will be renamed
  when installed (to xcurses.h and xpanel.h), to avoid conflicts with
  any existing curses installations. Unrenamed copies of curses.h and
  panel.h are installed in (by default) /usr/local/include/xcurses.


Distribution Status
-------------------

As of April 13, 2006, the files in this directory are released to the
Public Domain, except for ScrollBox*, which are under essentially the
MIT X License; config.guess and config.sub, which are under the GPL; and
configure, which is under a free license described within it.


Acknowledgements
----------------

X11 port was provided by Mark Hessling <mark@rexx.org>
