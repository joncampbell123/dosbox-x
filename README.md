**README for DOSBox-X** (official website: [dosbox-x.com](http://dosbox-x.com))

**Introduction to DOSBox-X**
---------------

DOSBox-X is a cross-platform DOS emulator based on the DOSBox project (www.dosbox.com)

Like DOSBox, it emulates a PC necessary for running many MS-DOS games and applications that simply cannot be run on modern PCs and operating systems. However, while the main focus of DOSBox is for running DOS games, DOSBox-X goes much further than this. Started as a fork of the DOSBox project, it retains compatibility with the wide base of DOS games and DOS gaming DOSBox was designed for. But it is also a platform for running DOS applications, including emulating the environments to run Windows 3.x, 9x and ME and software written for those versions of Windows.

DOSBox-X is open-source and totally free of charge to use and distribute. It is released under the GNU General Public License, version 2.

For more information about DOSBox-X, please read the user guide in the [DOSBox-X Wiki](https://github.com/joncampbell123/dosbox-x/wiki).

(Please always use the latest version of DOSBox-X from the [Releases page](https://github.com/joncampbell123/dosbox-x/releases))

This project has a Code of Conduct in [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md), please read it.

Brought to you by: joncampbell123 (Jonathan Campbell)


Notable features in DOSBox-X
----------------------------

Although based on the DOSBox project, DOSBox-X is now a separate project because both have their own schedules and development priorities. At this time DOSBox-X has quite a few features that do not exist in DOSBox. Examples of such features include:

* GUI menu bar and configuration tool

* Save and load state support

* Japanese NEC PC-98 mode support

* Better compatibility with DOS applications

* Support for more DOS commands and built-in external tools

* Support for printer output, either a real or virtual printer

* Support for long filenames and FAT32 disk images for DOS version 7+

* Support for 3dfx Glide and Voodoo chip emulation

* Support for NE2000 Ethernet for networking features

* Support for cue files with FLAC, Opus, Vorbis, and MP3 CD-DA tracks

* Support for FluidSynth MIDI synthesizer and Innovation SSI-2001 emulation

* Support for features such as V-Sync, overscan border and stereo swapping

* Plus many more..

DOSBox-X also has several important platform-dependent features, such as support for automatic drive mounting and clipboard copy & paste on the Windows platform.

DOSBox-X officially supports both SDL 1.2 and SDL 2.0; both 32-bit and 64-bit builds are also supported.


Open source development
-----------------------

Ideas and patches are always welcome, though not necessarily accepted.

If you really need that feature or change, and your changes are not
accepted into this main project (or you just want to mess around with
the code), feel free to fork this project and make your changes in
your fork.

As joncampbell123 only has limited time to work on DOSBox-X, help is
greatly appreciated:

  - Testing
    - Features
    - Hardware accuracy
    - Software accuracy
    - Games, applications, demoscene executables
    - Windows 1.x through Millenium guest OS support
    - Retro development
  - Bug fixes, patches, improvements, refinements
  - Suggestions, ideas, general conversation
  - Platform support (primarily Linux and Windows, but others are welcome)
  - Documentation
  - Notes regarding games, applications, hacks, weird MS-DOS tricks, etc.

If you want to tweak or write some code and you don't know what to work
on, feel free to visit the issue tracker to get some ideas.

For more information about the source code, please take a look at the
[source code description](README.source-code-description) page.

Information about the debugger is also available in the
[DOSBox-X Debugger](README.debugger) page.


Future development experiments
------------------------------

Scattered experiments and small projects are in experiments/ as
proving grounds for future revisions to DOSBox-X and it's codebase.

These experiments may or may not make it into future revisions
or the next version.

Comments are welcome on the experiments, to help improve the code
overall.

There are also patches in patch-integration/ for possible feature
integations in the future.


Comments on DOSBox-X development
--------------------------------

The four major operating systems and platforms of DOSBox-X are (in this order):

1. Linux (with X11) 32-bit and 64-bit x86.

2. Windows 10 (followed by Windows 8, and Windows 7) for 32-bit and 64-bit x86.

3. Linux (with X11) on a Raspberry Pi 3 (arm 7).

4. Mac OS X Sierra 10.12 or higher 64-bit.

Linux and MinGW Windows builds are expected to compile with the GNU autotools.

A preliminary CMake build system is available, see README.cmake.md for details.

Straight Windows builds are expected to compile using the free community edition
of Visual Studio 2015 or Visual Studio 2017 and the DirectX 2010 SDK.

Mac OS X builds are expected to compile on the terminal using GNU autotools
and the LLVM/Clang compiler provided by XCode.

In all cases, the code requires a C++ compiler that can support the C++11
standard.


Note that DOSBox-X is written to compile against the in-tree copy of the
SDL 1.x (Simple Directmedia Libary), or against the SDL 2.x library provided
by your Linux distribution.

For Visual Studio and MinGW compilation, the in-tree copy of SDL is always
used.


The in-tree SDL 1.x library has been HEAVILY MODIFIED from the original
SDL 1.x source code and is somewhat incompatible with the stock library.

The modifications provide additional functions needed to improve DOSBox-X
and fix many issues with keyboard input, window mangement, and display
management that previously required terrible kludges within the DOSBox
and DOSBox-X source code.

In Windows, the modifications also permit the emulation to run independent
of the main window so that moving, resizing, or using menus does not cause
emulation to pause.

In Mac OS X, the modifications provide an interface to allow DOSBox-X to
replace and manage the OS X menu bar.


Software security comments
--------------------------

DOSBox-X cannot claim to be a "secure" application. It contains a lot of
code designed for performance, not security. There may be vulnerabilities,
bugs, and flaws in the emulation that could permit malicious DOS executables
within to cause problems or exploit bugs in the emulator to cause harm.
There is no guarantee of complete containment by DOSBox-X of the guest
operating system or application.

If security is a priority, then:

Do not use DOSBox-X on a secure system.

Do not run DOSBox-X as root or Administrator.

If you need to use DOSBox-X, run it under a lesser privileged user, or in
a chroot jail or sandbox.

If your Linux distribution has it enabled, consider using the auditing
system to limit what the DOSBox-X executable is allowed to do.


Comments on what DOSBox-X is lacking
------------------------------------

DOSBox-X aims for accuracy in emulation however there are some things the
design as implemented now cannot accomodate.

* Cycle-accurate timing of x86 instructions and execution.

  Instructions generally run one per cycle in DOSBox-X, except for I/O
  and memory access.

  If accurate emulation of cycles per instruction is needed, please
  consider using PCem, 86Box, or VARCem instead.

* Full precision floating point emulation.

  Unless using the dynamic core, DOSBox and DOSBox-X emulate the FPU
  registers using the "double" 64-bit floating point data type.

  The Intel FPU registers are 80-bit "extended precision" floating
  point values, not 64-bit double precision, so this is effectively
  12 bits of precision loss and 5 bits of range loss (64 to 53 mantissa
  bits and 16 to 11 exponent bits).

  This slight loss of precision is perfectly fine considering DOSBox's
  original goal in supporting DOS games, but may cause problems in
  other cases that need the full precision.

  It is known at this time that this lack of precision is enough to
  cause otherwise straightforward comparisons against integers to
  fail in DOS applications originally written in QBasic or Turbo Basic.
  There are such DOS games written that check their file size using
  a floating point compare that will fail in this manner. To run these
  games, you will need to disable FPU emulation (fpu=false) to force
  the QBasic/TurboBasic runtime to use software emulation instead.

* Pentium II or higher CPU level emulation.

  DOSBox-X contains code only to emulate the 8088 through the Pentium Pro.

  If Pentium II or higher emulation is desired, consider using Bochs
  or QEMU instead. DOSBox-X may eventually develop Pentium II emulation,
  if wanted by the DOSBox-X community in general.

* Emulation of PC hardware 2001 or later.

  The official cutoff for DOSBox-X is 2001, when updated "PC 2001"
  specifications from Microsoft mandated the removal of the ISA slots
  from motherboards. 

  The focus is on implementing hardware emulation for hardware made
  before that point.

  Contributers are free to focus on emulating hardware within the
  1980-2001 timeframe of their choice.

* Windows guest emulation, Windows XP or later.

  DOSBox-X emulation, in terms of running Windows in DOSBox-X, will
  focus primarily on Windows 1.0 through Windows Millenium Edition,
  and then on Windows NT through Windows 2000. Windows XP and later
  versions are not a priority and will not be considered at this time.

  If you need to run Windows XP and later, please consider using
  QEMU, Bochs, or VirtualBox.

* Any MS-DOS system other than IBM PC/XT/AT, Tandy, PCjr, and NEC PC-98.

  Only the above listed systems will be considered for development
  in DOSBox-X. This restriction prevents stretching of the codebase
  to an unmanageable level and helps keep the code base organized.

  However, if adding emulation of the system requires only small
  minimal changes, then the new system in question may be considered.

  You are strongly encouraged to fork this project and implement
  your own variation if you need to develop MS-DOS emulation for
  any other system or console. In doing that, you gain the complete
  freedom to focus on implementing the particular MS-DOS based
  system of interest, and if desired, the ability to strip away
  conflicting IBM PC/XT/AT emulation and unnecessary code to keep
  your branch's code manageable and maintainable.

  It would be easier on myself and the open source community if
  developers could focus on emulating their platform of interest in
  parallel instead of putting everything into one project that,
  most likely, will do a worse job overall emulating all platforms.

  If you are starting a fork, feel free to let me know where your
  fork is and what system it is emulating, so I can list it in
  this README file for others seeking emulation of that system.

  To help, I have added machine and video mode enumerations as
  "stubs" to provide a starting point for your branch's implementation
  of the platform.

  Stubs implemented so far:
    - FM Towns emulation (machine=fm_towns)


Known DOSBox-X forks
--------------------

DOSBox-X Emscripten port (runnable in a web browser) by Yksoft1.
Significant changes are made in order to run efficiently within the web browser when compiled using LLVM/Emscripten.
These significant changes require dropping some useful features (including the menus) but are required for performance.

url: https://github.com/yksoft1/dosbox-x-vanilla-sdl/tree/emscripten (look for clone URL and use the emscripten branch)


Origins, and crediting of source code
-------------------------------------

by Jonathan Campbell.

As the developer of DOSBox-X, I cannot legitimately claim to have
written all of the code in this project.

DOSBox-X started as a fork of the original DOSBox project sometime
mid 2011. It was started out of a desire to improve the emulator
without having to fight with or worry about submitting patches
upstream.

As the forums make it clear, DOSBox's main focus is on DOS games.
This is evident by the fact that much of the code is somewhat
accurate code with kludges to make DOS games run, instead of
focusing on what hardware actually does.

Many of the changes I wanted to make were non-game related, and
therefore were unlikely to be accepted by the developers.

Since then, I have been modifying the source code over time to
improve emulation, fix bugs, and resolve incompatibilities with
Windows 95 through ME. I have added options so that DOSBox-X
by default can emulate a wider variety of configurations more
accurately, while allowing the user to enable hacks if needed
to run their favorite DOS game. I have also been cleaning up
and organizing the code to improve stability and portability
where possible.

It's more accurate to say then, that I wrote *some* of the code,
that I rewrote other parts of the code, and the rest is the DOSBox
SVN code as it existed since mid 2011.

The purpose of this section, is to try and build a comprehensive
list of source code in this repository that was borrowed from
other projects.

Some of the code is DOSBox SVN code in which some of the SVN
commits made since 2011 were incorporated into DOSBox-X.

The original DOSBox project was not written by one programmer. It
has been under development since late 2000 with patches, fixes,
and improvements from members all over the Vogons forums. Despite
not having a major official release since DOSBox 0.74 over 10
years ago, the project is still in active development today. Some
of the changes themselves incorporated code from other projects,
which are also credited in the list below.

Some of the code in this source tree also came from another
branch of DOSBox known as DOSBox Daum (http://ykhwong.x-y.net)
which itself incorporated code from the original DOSBox project,
DOSBox-X, and many experimental patches. Although the Daum
branch seems to be dead, the code borrowed from it still
exists in DOSBox-X.

Later on, DOSBox-X also incorporates code from other projects such
as DOSBox ECE, DOSBox-staging and vDosPlus for more features, with
major improvements from contributors such as Wengier and rderooy.

This is my attempt to properly credit the code and it's
sources below. Feel free to revise and correct this list
if there are errors.

NE2000 network card emulation (Bochs; LGPLv2+)
  src/hardware/ne2000.cpp

MT32 synthesizer (MUNT; LGPLv2.1+)
  src/mt32/*.cpp
  src/mt32/*.h

AVI writer with OpenDML support (written by myself; GPLv2+)
  src/aviwriter/*.cpp
  src/aviwriter/*.h

Framework-agnostic GUI toolkit (Jorg Walter; GPLv3+)
  src/libs/gui_tk/*.cpp
  src/libs/gui_tk/*.h

Porttalk library, to read/write I/O ports directly (Unknown source)
  src/libs/porttalk/*.cpp
  src/libs/porttalk/*.h

FreeDOS utilities as binary blobs (FreeDOS; no license)
  src/builtin/*.cpp

NukedOPL OPL3 emulation (Alexey Khokholov; GPLv2+)
  src/hardware/nukedopl.cpp

OPL emulation based on Ken Silverman OPL2 emulation (LGPLv2.1+)
  src/hardware/opl.cpp

MOS6581 SID emulation (GPLv2+)
  src/hardware/reSID/*.cpp
  src/hardware/reSID/*.h

SN76496 emulation (MAME project; GPLv2+)
  src/hardware/sn76496.h
  src/hardware/tandy_sound.cpp

PC-98 video rendering and I/O handling code (written by myself; GPLv2+)
  src/hardware/vga_pc98*.cpp

3dfx Voodoo Graphics SST-1/2 emulation (Aaron Giles; BSD 3-clause)
  src/hardware/voodoo_emu.cpp

PC-98 FM board emulation (Neko Project II; BSD 3-clause)
  src/hardware/snd_pc98/*

QCOW image support (Michael Greger; GPLv2+)
  src/ints/qcow2_disk.cpp

HQ2X and HQ3X render scaler (ScummVM, Maxim Stepin; GPLv2+)
  src/gui/render_templates_hq2x.h


Foreign keyboard layouts
------------------------

DOSBox-X was developed around the US keyboard layout,
with only a few additional layouts natively supported.

To add additional layouts, see file "README.keyboard-layout-handling"
on how to do so as a developer.

