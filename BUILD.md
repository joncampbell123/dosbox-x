Building DOSBox-X
=================

The page is about manually building the DOSBox-X source code. Automated
development (preview) builds intended for testing purposes for various
platforms are also available from the [DOSBox-X Development Builds](https://dosbox-x.com/devel-build.html) page.
Released builds are available from the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page.

For instructions on installing and using DOSBox-X, please look at the
[INSTALL](INSTALL.md) page and the [DOSBox-X Wiki](https://dosbox-x.com/wiki).

General information on source code compilation
----------------------------------------------

The four major operating systems and platforms of DOSBox-X are:

1. Windows 11, 10, 8, 7, Vista and XP for 32-bit and 64-bit x86/x64 and ARM

2. Linux (with X11) 64-bit x86/x64, and on a Raspberry Pi 3/4

3. macOS (Mac OS X) recent version, 64-bit Intel, ARM-based, and Universal

4. DOS (MS-DOS 5.0+ or compatible)

Straight Windows builds are expected to compile using the free community edition
of Visual Studio 2017 to Visual Studio 2022 and the DirectX 2010 SDK.

Linux and MinGW Windows builds are expected to compile with the GNU autotools.

macOS builds are expected to compile on the terminal using GNU autotools and
the LLVM/Clang compiler provided by XCode. Universal macOS builds are only
possible when building on a host machine powered by an Apple Silicon CPU, due to
requiring parallel Homebrew installations running natively *and* under
Rosetta 2.

In all cases, the code requires a C++ compiler that can support the C++11
standard.

Note that DOSBox-X supports both SDL 1.x and 2.x, and it is written to compile
against the in-tree copy of the SDL 1.x (Simple Directmedia Libary), or against
the SDL 2.x library provided by your Linux distribution.

For Visual Studio and MinGW compilation, the in-tree copy of SDL is always
used. Note that the in-tree SDL 1.x library has been heavily modified from
the original SDL 1.x source code and is thus somewhat incompatible with the
stock library.

The modifications provide additional functions needed to improve DOSBox-X
and fix many issues with keyboard input, window management, and display
management that previously required terrible kludges within the DOSBox
and DOSBox-X source code.

In Windows, the modifications also permit the emulation to run independent
of the main window so that moving, resizing, or using menus does not cause
emulation to pause.

In macOS, the modifications provide an interface to allow DOSBox-X to
replace and manage the macOS menu bar.

Please look at the [README.source-code-description](README.source-code-description) file for more information
and descriptions on the source code.

How to compile the source code (cross-platform)
-----------------------------------------------

* General Linux or BSD compile (SDL1)
```
./build-debug
sudo make install
```

* General Linux or BSD compile (SDL2)
```
./build-debug-sdl2
sudo make install
```

* macOS compile (SDL1)
  * Build natively for the host architecture
    ```
    ./build-macos
    ```
  * Build a Universal Binary on an Apple Silicon CPU (will *not* work on Intel)
    ```
    ./build-macos universal
    ````
    You can build an App Bundle from the result of this build with
    ```
    make dosbox-x.app
    ```

* macOS compile (SDL2)
  * Build natively for the host architecture
    ```
    ./build-macos-sdl2
    ```
  * Build a Universal Binary on an Apple Silicon CPU (will *not* work on Intel)
    ```
    ./build-macos-sdl2 universal
    ````
    You can build an App Bundle from the result of this build with
    ```
    make dosbox-x.app
    ```

* MinGW compile (using MinGW-w64) for Windows Vista/7 or later (SDL1)
```
./build-mingw
```

* MinGW compile (using MinGW-w64) for Windows Vista/7 or later (SDL2)
```
./build-mingw-sdl2
```

* MinGW compile (using MinGW32, not MinGW-w64) for lower-end systems including Windows XP or later (SDL1)
```
./build-mingw-lowend
```

* MinGW compile (using MinGW32, not MinGW-w64) for lower-end systems including Windows XP or later (SDL2)
```
./build-mingw-lowend-sdl2
```

* MinGW compile (using MinGW32, not MinGW-w64) on Windows to target the DOS platform (MS-DOS or compatible with HX DOS Extender)
```
./build-mingw-hx-dos
```

NOTICE: Use the 32-bit toolchain from the original MinGW project for the lowend and HX-DOS builds, not the MinGW-w64 project.
        Binaries compiled with MinGW-w64 have extra dependencies which are not supported by Windows XP or the HX DOS Extender.

macOS: If you want to make an .app bundle you can run from the Finder, compile the program as instructed then run ``make dosbox-x.app``.

XCode (on macOS, from the Terminal) to target macOS
```
./build-debug
```

## To compile DOSBox-X in Ubuntu (tested with 20.04 and 20.10):

First install the development tools, headers and libraries needed

```
sudo apt install automake gcc g++ make libncurses-dev nasm libsdl-net1.2-dev libsdl2-net-dev libpcap-dev libslirp-dev fluidsynth libfluidsynth-dev libavdevice58 libavformat-dev libavcodec-dev libavcodec-extra libavcodec-extra58 libswscale-dev libfreetype-dev libxkbfile-dev libxrandr-dev
```

Then change to the directory where you unpacked the DOSBox-X source code, and run the following commands:

```
./build-debug
sudo make install
```

Alternatively you can also compile the SDL2 version by running the ``./build-debug-sdl2`` script.

## To compile DOSBox-X in Fedora Workstation:

First install the development tools, headers and libraries needed

```
 sudo dnf group install "C Development Tools and Libraries"
 sudo dnf install SDL_net-devel SDL2_net-devel libxkbfile-devel ncurses-devel libpcap-devel libslirp-devel libpng-devel fluidsynth-devel freetype-devel nasm
```

If you want to be able to record video, you will also need to install ffmpeg-devel which you can get from the optional rpmfusion-free repository.
Then change to the directory where you unpacked the DOSBox-X source code, and run the following commands:

```
 ./build-debug
 sudo make install
```

Alternatively you can also compile the SDL2 version by running the ``./build-debug-sdl2`` script.

## To create a DOSBox-X RPM for use in RHEL, CentOS or Fedora:

First ensure that your system has all the necessary development tools and libraries installed, such as by following the dnf steps in the above "To compile DOSBox-X in Fedora Workstation".
Then run the following commands:

```
 sudo dnf group install "RPM Development Tools"
 ./make-rpm.sh
```

After a successful compile, the RPM can be found in the releases directory.

Compiling the source code using Visual Studio (Windows)
-------------------------------------------------------

You can build the source code with Visual Studio 2017, 2019, and 2022.
(The code currently cannot be built with Visual Studio 2015)
The executables will work on 32-bit and 64-bit Windows Vista or higher.
If you want the executables to work on Windows XP, you can patch the PE header
using a tool included in the source code.

```./contrib/windows/installer/PatchPE.exe path-to-your-exe-file/dosbox-x.exe```

Use the ```./vs/dosbox-x.sln``` "solution" file and build the source code.
You will need the DirectX 2010 SDK for Direct3D9 support.

By default the targeted platform is v142 (Visual Studio 2019).
For building the source code in Visual Studio 2017 or 2022,
you may change the platform toolset to v141 or v143 respectively.
For Visual Studio 2017, you have to set ``WindowsTargetPlatformVersion`` to whatever
Windows SDK version installed in your PC, for example ``10.0.22000.0``.
(Visual Studio 2019 and beyond will pick the latest Windows SDK version if you set the value to `10.0`)

Libraries such as SDL, freetype, libpdcurses, libpng and zlib are already included,
and as of DOSBox-X 0.83.6 support for FluidSynth MIDI Synthesizer is also included
for Windows builds (set ``mididevice=fluidsynth`` in the [midi] section of DOSBox-X's
configuration file (dosbox-x.conf) along with required soundfont file [e.g.
``FluidR3_GM.sf2`` or ``GeneralUser_GS.sf2``] to use it).

The slirp backend for the NE2000 network emulation is only supported by MinGW builds
but not Visual Studio builds.

Build the source code for your platform (Win32, x64, ARM and ARM64 are supported).

As of 2018/06/06, Visual Studio 2017 builds (32-bit and 64-bit) explicitly require
a processor that supports the SSE instruction set. As of version 2022.09.01, Visual
Studio ARM/ARM64 builds require a Windows SDK that includes the OpenGL library.

Visual Studio Code is supported, too.

Check the [README.development-in-Windows](README.development-in-Windows) file for more information about this platform.

Libraries used by DOSBox-X
--------------------------

The following libraries are used by DOSBox-X:

* SDL 1.2.x or SDL 2.x (in-tree)

    The Simple DirectMedia Library available at https://www.libsdl.org

    The SDL1 library distributed with DOSBox-X had been heavily modified
    from the original to support for example native OS menus.
    
    Note that only version 1.2.x (SDL1 version) and version 2.x
    (SDL2 version) are currently supported.
    
    License: LGPLv2+

* Curses (optional)

    If you want to enable the debugger you need a curses library.
    
    ncurses should be installed on just about every Unix/Linux distro.
    
    For Windows get pdcurses at https://pdcurses.org/
    
    License: Public Domain

* Libpng (in-tree; optional)

    Needed for the screenshots.
    
    For Windows get libpng from https://gnuwin32.sourceforge.net/packages.html
    
    See http://www.libpng.org/pub/png/ for more details.
    
    License: zlib/libpng

* Zlib (in-tree)

    Needed by libpng, and for save-state and CHD support.
    
    For Windows get libz (rename to zlib) from https://gnuwin32.sourceforge.net/packages.html
    
    See https://www.zlib.net/ for more details.
    
    License: zlib

* FreeType (in-tree; optional)

    Needed for TrueType font (TTF) output and printing support.
    
    It is available from https://www.freetype.org/download.html
    
    License: FTL or GPLv2+

* FluidSynth (optional)

    For soundfont support.
    
    It is available from https://www.fluidsynth.org/download/
    
    License: LGPLv2+
    
* libpcap (optional)

    For pcap backend of NE2000 networking support.
    
    Get it from https://www.tcpdump.org/index.html#latest-releases
    
    For Windows get Npcap (WinPcap for Windows 10) from https://nmap.org/download.html

    License: 3-clause BSD license

* libslirp (optional)

    For slirp backend of NE2000 networking support.
    
    Get it from https://gitlab.freedesktop.org/slirp/libslirp
    
    License: Modified 4-clause BSD license

* SDL_Net (in-tree; optional)

    For Modem/IPX support.
    
    Get it from https://www.libsdl.org/projects/SDL_net/release-1.2.html
    
    License: LGPLv2+

* SDL_Sound (in-tree; optional)
    
    For compressed audio on diskimages (cue sheets) support.
    
    This is for cue/bin CD-ROM images with compressed (MP3/OGG/FLAC) audio tracks.
    
    Get it from https://icculus.org/SDL_sound
    
    Licence: LGPLv2+

* ALSA_Headers (optional)
    
    For ALSA support under Linux. Part of the Linux kernel sources.
    
    License: LGPLv2+

Configure script options
------------------------

The DOSBox-X configure script accepts the following switches, which you can use to customize the code compilation:

* --enable-FEATURE[=ARG]
        
        Includes FEATURE [ARG=yes]

* --enable-silent-rules
        
        Less verbose build output (undo: "make V=1")

* --enable-dependency-tracking
        
        Do not reject slow dependency extractors

* --enable-force-menu-sdldraw
        
        Forces SDL drawn menus

* --enable-hx-dos
        
        Enables HX-DOS target

* --enable-emscripten
        
        Enables Emscripten target

* --enable-sdl
        
        Enables SDL 1.x

* --enable-sdl2
        
        Enables SDL 2.x

* --enable-xbrz
        
        Compiles with xBRZ scaler (default yes)

* --enable-scaler-full-line
        
        Scaler render full line instead of detecting
        changes, for slower systems

* --enable-alsa-midi
        
        Compiles with ALSA MIDI support (default yes)

* --enable-d3d9
        
        Enables Direct3D 9 support

* --enable-d3d-shaders
        
        Enables Direct3D shaders

* --enable-debug 
        
        Enables the internal debugger. --enable-debug=heavy enables even more 
        debug options. To use the debugger, DOSBox-X should be run from an xterm
        and when the sdl-window is active press alt-pause to enter the
        debugger.

* --disable-FEATURE
        
        Do not include FEATURE (same as --enable-FEATURE=no)

* --disable-silent-rules
        
        Verbose build output (undo: "make V=0")

* --disable-dependency-tracking
        
        Speeds up one-time build

* --disable-largefile
        
        Omits support for large files

* --disable-x11
        
        Do not enable X11 integration

* --disable-optimize
        
        Do not enable compiler optimizations

* --disable-sdl2test
        
        Do not try to compile and run a test SDL 2.x program

* --disable-sdltest
        
        Do not try to compile and run a test SDL 1.x program

* --disable-alsatest
        
        Do not try to compile and run a test ALSA program

* --disable-freetype
        
        Disables FreeType support

* --disable-printer
        
        Disables printer emulation

* --disable-mt32
        
        Disables MT32 emulation

* --disable-screenshots
        
        Disables screenshots and movie recording

* --disable-avcodec
        
        Disables FFMPEG avcodec support

* --disable-fpu
        
        Disables the emulated FPU. Although the FPU emulation code isn't
        finished and isn't entirely accurate, it's advised to leave it on.

* --disable-fpu-x86
* --disable-fpu-x64
        
        Disables the assembly FPU core. Although relatively new, the x86/x64
        FPU core has more accuracy than the regular FPU core. 

* --disable-dynamic-x86
        
        Disables the dynamic x86/x64 specific CPU core. Although it might be
        a bit unstable, it can greatly improve the speed of dosbox-x on x86 
        and x64 hosts.
        Please note that this option on x86/x64 will result in a different
        dynamic/recompiling CPU core being compiled than the default.
        For more information see the option --disable-dynrec

* --disable-dynrec
        
        Disables the recompiling CPU core. Currently x86/x64 and arm only.
        You can activate this core on x86/x64 by disabling the dynamic-x86
        core.

* --disable-dynamic-core
        
        Disables all dynamic cores. (same effect as --disable-dynamic-x86
        or --disable-dynrec).

* --disable-opengl
        
        Disables OpenGL support (output mode that can be selected in the
        DOSBox-X configuration file).

* --disable-unaligned-memory
        
        Disables unaligned memory access.
