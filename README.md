DOSBox-X Manual (always use the latest version from [dosbox-x.com](http://dosbox-x.com))

DOSBox-X is a cross-platform DOS emulator based on the DOSBox project (www.dosbox.com)

For more information please read the user guide in the [DOSBox-X Wiki](https://github.com/joncampbell123/dosbox-x/wiki).

This project has a Code of Conduct in CODE_OF_CONDUCT.md, please read it.

I am rewriting this README, and new information will be added over time --J.C.



How to compile DOSBox-X in Ubuntu (kapper1224)
----------------------------------------------

    sudo apt install automake libncurses-dev nasm libsdl-net1.2-dev libpcap-dev libfluidsynth-devffmpeg libavdevice58 libavformat-* libswscale-* libavcodec-*
    ./build
    sudo make install



How to compile in general
-------------------------

General Linux compile (FFMPEG/libav support required)
./build-debug


General Linux compile if FFMPEG/libav not desired
./build-debug-no-avcodec


Mac OS X compile
./build-macosx


Mac OS X compile (SDL2)
./build-macosx-sdl2


MinGW compile (on Windows, using MinGW64) for 32-bit Windows XP
./build-mingw


MinGW compile (on Windows, using MinGW64) for 32-bit Windows XP, lower-end systems that lack MMX/SSE
./build-mingw-lowend


MinGW compile (on Windows, using MinGW, not MinGW64) to target MS-DOS and the HX DOS extender
./build-mingw-hx-dos

NOTICE: Use the 32-bit toolchain from the main MinGW project, not the MinGW64 project.
        Binaries compiled with MinGW64 have extra dependencies not provided by the HX DOS extender.


Mac OS X: If you want to make an .app bundle you can run from the Finder, compile the
program as instructed then run "make dosbox-x.app".


Visual Studio 2017 compile for 32/64-bit Windows Vista or higher
Use the ./vs2015/dosbox-x.sln "solution" file and compile.
You will need the DirectX 2010 SDK for Direct3D9 support.

As of 2018/06/06, VS2017 builds (32-bit and 64-bit) explicitly require
a processor that supports the SSE instruction set.

Visual Studio Code is supported, too.

Check the README.Windows file for more information about this platform.


XCode (on Mac OS X, from the Terminal) to target Mac OS X
./build-debug


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


Future development experiments
------------------------------

Scattered experiments and small projects are in experiments/ as
proving grounds for future revisions to DOSBox-X and it's codebase.

These experiments may or may not make it into future revisions
or the next version.

Comments are welcome on the experiments, to help improve the code
overall.


Warnings regarding C integer types
----------------------------------

Contrary to initial assumptions, never assume that int and long have specific
sizes. Even long long.

The general assumption is that int is 32 bit and long is 32 bit.
That is not always true, and that can get you in trouble when
working on this or other projects.

Another common problem is the use of integers for pointer manipulation.
Storing pointers or computing differences between pointers may happen
to work on 32-bit, where ints and pointers are the same size, but the
same code may break on 64-bit.

Therefore, for manipulating pointers, use uintptr_t instead of int or
long.

For quick reference, here is a breakdown of the development
targets and their sizes:

Windows (Microsoft C++) 32-bit:
  sizeof(int) == 32-bit
  sizeof(long) == 32-bit
  sizeof(long long) == 64-bit
  sizeof(uintptr_t) == 32-bit

Windows (Microsoft C++) 64-bit:
  sizeof(int) == 32-bit
  sizeof(long) == 32-bit
  sizeof(long long) == 64-bit
  sizeof(uintptr_t) == 64-bit

NOTE: If you ever intend to compile against older versions of Microsoft C++/Visual Studio,
      the "long long" type will need to be replaced by __int64.

Linux 32-bit:
  sizeof(int) == 32-bit
  sizeof(long) == 32-bit
  sizeof(long long) == 64-bit
  sizeof(uintptr_t) == 32-bit

Linux 64-bit:
  sizeof(int) == 32-bit
  sizeof(long) == 64-bit
  sizeof(long long) == 64-bit
  sizeof(uintptr_t) == 64-bit


This code is written to assume that sizeof(int) >= 32-bit.
However know that there are platforms where sizeof(int) is
even smaller. In real-mode MS-DOS and 16-bit Windows for
example, sizeof(int) == 16 bits (2 bytes). DOSBox-X will
not target 16-bit DOS and Windows, so this is not a problem
so far.

For obvious reasons, far pointers are not supported. The
memory map of the runtime environment is assumed to be
flat with possible virtual memory and paging.

When working on this code, please understand the limits of
the integer type in the code you are writing to avoid
problems. Pick a data type that is large enough for the
expected range of input.

It is suggested to use C header constants if possible
for min and max integer values, like UINT_MAX.

If the code needs to operate with specific widths of
integer, please use data types like uint16_t, uint32_t,
int16_t and int32_t provided by modern C libraries, and
do not assume the width of int and long.

If compiling with older versions of Visual Studio, you will
need to include a header file to provide the uintptr_t and
uint32_t datatypes to fill in what is lacking in the C library.

When multiplying integers, overflow cases can be avoided
with a * b by rejecting the operation if b > (UINT_MAX / a)
or by multiplying a * b with a and b typecast to the next
largest datatype.

Remember that signed and unsigned integers have the same
width but the MSB changes the interpretation. This code
is written for processors (such as x86) where signed integers
are 2's complement. It will not work correctly with any
other type of signed integer.

2's complement means that the MSB bit of an integer indicates
the number is negative. When it is negative, the value could
be thought of as N - (2^sizeof_in_bits(int)). For a 16-bit
signed integer:

   2^16 = 0x10000 = 65536

     hex         int    unsigned int        equiv
   0x7FFE       32766       32766           32766 - 0
   0x7FFF       32767       32767           32767 - 0
   0x8000      -32768       32768           32768 - 65536
   0x8001      -32767       32769           32769 - 65536
   ...
   0xFFFE      -2           65534           65534 - 65536
   0xFFFF      -1           65535           65535 - 65536
   (carry, overflow all 16-bits, roll back to 0)
   0x0000       0           0               0 - 0
   0x0001       1           1               1 - 0

Another possible problem may lie in using negation (-) or
inverting all bits (~) of an integer for masking. The
result may be treated by the compiler as an integer. Make
sure to typecast it to clarify.

Another possible incompatiblity lies with printf() and
long long integers.

Always typecast the printf() parameters to the data type
intended to avoid problems and warnings.

While Mac OS X and Linux have runtimes that can take %llu
or %llx, Microsoft's runtime in Windows cannot. Either
avoid printing long long integers or add conditional code
to use %llx or %llx on Linux and %I64u or %I64d on Windows.

Note that MinGW compilation on Windows suffers from the
same limitation due to use of Microsoft C runtime.

When dealing with sizes, including file I/O and byte counts,
use size_t (unsigned value) and ssize_t (signed value) instead.
This will help with using the C++ standard template library
and the C file I/O library. If compiling for a target where
read and write use int for a return value instead, then
use typecasting.

When handling file offsets, use off_t instead of long.
Modern C runtime versions of lseek and tell will use that
datatype. For older runtimes that use "long", make a typecast
in a header file for your target to declare off_t. Remember
that off_t is a signed value and that it can be negative.

Make sure to use the 64-bit version of lseek (often named
lseek64 or _lseeki64) in order to support files 4GB or
larger if allowed by the runtime environment.

On most modern runtimes, an alternate version of open()
may be required in order to open or create files larger
than 2GB. However the alternate open() reference can be
eliminated in certain cases.

On 32-bit Linux, direct calls to open64() can be avoided
if CFLAGS contains -D_FILE_OFFSET_BITS=64 or
#define _FILE_OFFSET_BITS=64 is added to the project.

Remember that lseek() can return -1 to indicate an error.
lseek() however will permit seeking past the end of a file.

writing at that point will extend the file to allow the
file write to occur at that offset. Depending on the platform,
that will either cause a sparse file (Linux + ext) or will
cause a loop within the filesystem driver to extend the file
and zero clusters to make it happen (Windows XP through 10).

Use of the FILE* file I/O layer is OK, but not recommended
unless there is a need to use text parsing with functions
like fgets() or fprintf(). For other uses, please use C
functions open, close, read, write, lseek and learn to use
file handles.

Understand that when fgets() returns with the buffer filled
with the line of text, the end of the string will always include
the newline (\n) that fgets() stopped reading at.

If fopen() was called with the "b" flag on DOS and Windows
formatted text files, the end of the string will probably
contain \r\n (CR LF). On platforms other than DOS and Windows,
\r\n will always appear if it is in the file.

C file handles are signed integers. They can be negative.
File handles returned by the C runtime however are never
negative except to indicate an error.

A good way to track whether an int holds an open file therefore,
is to initialize at startup that integer to -1, and then when
open succeeds, assign that value the file handle. When closing
the file, assign -1 to the integer to record that the handle was
closed.

Other parts of the code can also check if the file handle is
non-negative before operating on the file as a safety measure
against calling that function when the file was never opened.

On Windows, the HANDLE value at the Win32 API level can be obtained
from an integer file handle using _get_osfhandle() for use with
the Win32 API functions directly.

When using open(), make sure to use O_BINARY to avoid
CR/LF translation on DOS and Windows systems. Make sure
there is a header that defines O_BINARY as (0) if the
platform does not provide O_BINARY to avoid #ifdef's
around each open() call.

When using arithmetic with C pointers and integers,
understand that the pointer is adjusted by the value of the
integer times the size of the pointer type. If you intend
to adjust by bytes, then typecast the pointer to char* or
unsigned char* first, or typecast to uintptr_t to operate
on the pointer value as if an integer, then add the integer
value to the pointer.

At the lowest level, a pointer could be thought of as an
integer that is interepreted by the CPU as a memory address
to operate on, rather than an integer value directly.

Therefore, when adding an integer to a pointer value, the
result could be thought of as:

  (new pointer value in bytes) = (current pointer value in bytes) + integer value * sizeof(pointer data type)

If the pointer is char, then adding 4 will advance by 4 bytes.
If the pointer is int, then adding 4 will advance by 4 * sizeof(int) bytes, or, 4 memory locations of type int.

Keep this in mind when manipulating pointers while working
on this code.


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

DOSBox-X started as a fork of the main DOSBox project sometime
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

The main DOSBox project was not written by one programmer. It has
been under development since late 2000 with patches, fixes, and
improvements from members all over the Vogons forums. Despite
not having an official release since DOSBox 0.74 over 10 years
ago, the project is still in active development today. Some of
the changes themselves incorporated code from other projects,
which are also credited in the list below.

Some of the code in this source tree also came from another
branch of DOSBox known as DOSBox Daum (http://ykhwong.x-y.net)
which itself incorporated code from the main DOSBox project,
DOSBox-X, and many experimental patches. Although the Daum
branch seems to be dead, the code borrowed from it still
exists in DOSBox-X.

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


Tips for hacking and modifying the source code
----------------------------------------------

As a SDL (Simple Directmedia Layer) based application,
DOSBox-X starts execution from main(), which is either
the real main() function or a redefined main() function
called from SDLmain depending on the platform.

On Linux and Mac OS X, main() is the real main function.

On Windows, main() is SDLmain() and is called from the
WinMain function defined in the SDL library.

The entry point main() is in src/gui/sdlmain.cpp,
somewhere closer to the bottom.


Configuration and control state (from dosbox-x.conf and
the command line) are accessible through a globally
scoped pointer named "control".

In the original DOSBox SVN project, "control" is
most often used for accessing the sections and
settings of dosbox.conf.

In DOSBox-X, "control" also holds flags and variables
gathered from the command line (such as -conf).

Most (though not all) of the sections and settings
are defined in src/dosbox.cpp. There is one function
DOSBox_SetupConfigSections() that adds sections and
settings.

Each section has a list of settings by name. Each
setting can be defined as an int, hexadecimal,
string, double, and multivalue item. Read
include/setup.h and src/misc/setup.cpp for more
information.

There is one section (the autoexec section) that
is defined as lines of text.

In the original DOSBox SVN project, each section
also has an init and destructor function. The
codebase in SVN is heavily written around emulator
setup from each section, which is why the order
of the sections is important. DOSBox-X eliminated
these init and destructor functions and encourages
initial setup from functions called in main(),
and additional setup/teardown through VM event
callbacks (see include/setup.h). A callback
mechanism is provided however (at a section level)
when settings change.

Most of the code in this codebase assumes that
it can retrieve a section by name, and a setting
by name, without checking whether the returned
referce to a setting is NULL. Therefore, removing
a setting or referring to settings before the
creation of them can cause this code to crash
until that reference is removed.


Time and cycles in DOSBox-X
---------------------------

Time is handled as a macro unit of 1ms time called "ticks",
tied heavily to SDL_GetTicks() to track time.

Within each 1ms tick, a cycle count specified by the user
is executed as CPU time.

Setting cycles=3000 therefore, instructs DOSBox and DOSBox-X
to execute 3000 cpu cycles per millisecond. That generally
means (though not always) that 3000 instructions are executed
per millisecond.

Other parts of emulation may consume additional CPU cycles
to simulate I/O or video RAM delay.

Normal_Loop() in src/dosbox.cpp controls per-tick execution
as directed by PIC_RunQueue() whether or not the 1ms tick
has completed.

Generally the CPU core will execute instructions for the
entire 1ms tick, but the loop will cut short if events
are scheduled to execute sooner.

Events are scheduled in src/hardware/timer.cpp, using
PIC_AddEvent() given a callback and a delay in milliseconds.
Scheduling an event will cut the CPU cycle count back to
enable the event to execute on time.

PIC_AddEvent() events are scheduled once. Periodic events
should call PIC_AddEvent() again within the callback. For
precision reasons, PIC_AddEvent() can identify whether it
is being called from an event callback, and it will use
the delta time differently to help periodic events maintain
regular intervals.

Events can be removed using PIC_RemoveEvents().

Per-tick event handlers can be added using the
TIMER_AddTickHandler() function in src/hardware/pic.cpp.
The callback will be called at the completion of the
1ms tick.

Emulator code can query emulator time at any time
using the functions in include/pic.h.

PIC_TickIndex() returns the time within the 1ms
tick as a floating point value from 0.0 to 1.0.

PIC_TickIndexND() returns the same as cycle counts
within the 1ms tick.

PIC_FullIndex() returns absolute emulator time
by combining ticks and cycle count time.


How DOSBox and DOSBox-X mix x86 and native code
-----------------------------------------------

Much of the DOS and BIOS handling in DOSBox and DOSBox-X
is done through the use of the "callback" instruction
and a callback system in src/cpu/callback.cpp.

Each BIOS interrupt is a callback, as is the DOS kernel
interrupts. INT 21h is handled as a callback to src/dos/dos.cpp
function DOS21_Handler(), for example. That native code
function can then manipulate CPU registers and memory as
needed to emulate the DOS call.

Some callback functions will also modify the stack frame
to set or clear specific CPU flags on return, using
functions CALLBACK_SCF(), CALLBACK_SZF(), and CALLBACK_SIF().

The callback instruction is 0xFE 0x38 <uint16_t>. This
is an invalid opcode on actual x86 hardware, but it is
a call into a callback function within the DOSBox
emulation. The uint16_t value specifies which callback.

Callbacks are registered through CALLBACK_Allocate(),
which then returns an integer value that is an index into
the callback table. 0 is an invalid callback value that
indicates no callback was allocated, though at this time,
CALLBACK_Allocate() is written to E_Exit() and abort
emulation in the case that none are available, instead
of returning zero.

CALLBACK_DeAllocate() can be used with the index to
free that slot so that other code can use CALLBACK_Allocate()
to take that slot if needed, though it is rare to use
CALLBACK_DeAllocate() so far.

When allocated, the emulation code can then write x86
instructions where needed that include the callback
instruction in order to work from native code at that
point in execution. Generally, most of the x86 code
generation is done within the callback framework itself
using CALLBACK_SetupExtra to write common patterns of
x86 code depending on how the native code is meant to
execute or return to the caller.

When the CPU core encounters a callback instruction,
the index of the instruction (nonzero, remember) is
returned from the execution loop with the expectation
the caller will then index the callback array with it.

If the callback instruction is called from protected
mode, memory and I/O access may cause recursion of
the emulator. Memory access functions called by the
native code may trigger an I/O port or page fault
exception within the guest. DOSBox and DOSBox-X
resolve the fault by pushing an exception frame
onto the stack and then recursing into another
emulation loop which does not break until the fault
is resolved. While this is perfectly fine for
DOS and Windows 3.1 simple fault handling, this
may cause recursion issues with more advanced
task switching and fault handling in Windows 95
and later.

The most common reason a callback handler might
get caught with a page fault is the emulation
of DOS and BIOS interrupts while running within
the virtualization environment of Windows 3.0
through Windows ME.

Another possible source of page faults may occur
with DOS extenders that enable paging of memory
to disk.

Callback functions will typically return CBRET_NONE.


General description of source code
----------------------------------

src/shell/shell.cpp         SHELL init, SHELL run, fake COMMAND.COM setup,
                                startup messages and ANSI art, AUTOEXEC.BAT
                                emulation and setup, shell interface,
                                input, parsing, and execution

src/shell/shell_batch.cpp   Batch file (*.BAT) handling

src/shell/shell_cmds.cpp    Shell internal command handling, shell commands:
                                DIR     CHDIR   ATTRIB  CALL    CD      CHOICE
                                CLS     COPY    DATE    DEL     DELETE  ERASE
                                ECHO    EXIT    GOTO    HELP    IF      LOADHIGH
                                LH      MKDIR   MD      PATH    PAUSE   RMDIR
                                RD      REM     RENAME  REN     SET     SHIFT
                                SUBST   TIME    TYPE    VER     ADDKEY  VOL
                                PROMPT  LABEL   MORE    FOR     INT2FDBG
                                CTTY    DEBUGBOX

src/shell/shell_misc.cpp    PROMPT generator, command line input interface,
                            shell execution, and command location via PATH
                            interface.

src/gui/sdlmain.cpp         Entry point, emulator setup, runtime execution,
                            cleanup. Menu management, GFX start/end handling,
                            GFX mode setup and management. Menu handling.
                            Logging of GFX state. A lot of other misc code.

src/gui/sdlmain_linux.cpp   Linux-specific state tracking and handling.

src/gui/sdl_mapper.cpp      Mapper interface, mapper event handling and routing,
                            mapper file reading and writing. Keyboard, mouse,
                            joystick, and shortcut handling. In DOSBox-X,
                            also ties mapper shortcuts to the menu system.

src/gui/sdl_gui.cpp         Configuration GUI (using gui_tk), dialog boxes,
                            background "golden blur" behind dialog boxes,
                            input management and display of dialog boxes.

src/gui/menu.cpp            Menu handling and management, processing,
                            application of menu to host OS menu framework
                            if applicable. In DOSBox-X, contains the menu
                            C++ class and menu item object system which then
                            maps to Windows HMENU, Mac OS X NSMenu, or the
                            custom drawn SDL menus if neither are available.

                            Which menu framework is used depends on the
                            assignment of the DOSBOXMENU_* constant as defined
                            in include/menu.h. By default:

                                Windows (HMENU) is used if targeting Windows
                                and not HX DOS and not SDL2

                                Mac OS X (NSMENU) is used if targeting Apple
                                Mac OS X and not SDL2

                                SDL drawn menus are used in all other cases.

                                A define is available via configure.ac if
                                SDL drawn menus should be used regardless of
                                the host OS and environment.

                                A NULL menu define is provided if a build
                                with no visible menus is desired.

src/gui/render.cpp          RENDER_ and render scaler code. Also handles
                            color palette, aspect ratio, autofit options.
                            The selection of render scaler is defined and
                            chosen here.

src/gui/render_scalers.cpp  Render scaler definitions and code. Note that
                            scalers are defined using header files as
                            templates and #defines to support each color
                            format.

src/gui/midi.cpp            MIDI output framework. Header files include
                            additional platform-specific code.

src/gui/menu_osx.mm         Mac OS X Objective C++ code to bridge Objective C
                            and C++ so that the menu manipulation code can
                            work correctly.

src/gui/direct3d.cpp        Windows Direct3D9 support code. This code allows
                            output=direct3d to work properly. Uses DirectX 9
                            interfaces.

include/bitop.h             Header file to provide compile-time and runtime
                            inline functions for bit manipulation and masking.
                            Additional code is in src/gui/bitop.cpp

include/ptrop.h             Header file to provide compile-time and runtime
                            inline functions for pointer manipulation and
                            alignment. Additional code is in src/gui/ptrop.cpp.

src/aviwriter/*             AVI writer library, written by Jonathan Campbell
                            sometime around 2010, and incorporated into DOSBox-X.
                            Unlike the initial code from DOSBox SVN, this code
                            can support writing OpenDML AVI files that exceed
                            the 2GB file size limit.

                            All definitions, including Windows PCM formats and
                            GUIDs, are provided here.

src/misc/cross.cpp          Cross-platform utility functions.

src/misc/messages.cpp       Message translation table functions.

src/misc/setup.cpp          Configuration, section, and setting management.

src/misc/shiftjis.cpp       Shift-JIS utility functions.

src/misc/support.cpp        String support functions including case conversion.

src/builtin/*.cpp           Built-in executable binaries, defined as unsigned char[]
                            arrays and registered at runtime:

                                25.COM          28.COM          50.COM          APPEND.EXE
                                BUFFERS.COM     COPY.EXE        CWSDPMI.EXE     DEBUG.EXE
                                DEVICE.COM      DOS32A.EXE      DOS4GW.EXE      DOSIDLE.EXE
                                EDIT.COM        FCBS.COM        FIND.EXE        HEXMEM16.EXE
                                HEXMEM32.EXE    LASTDRIV.COM    MEM.COM         MOVE.EXE
                                TREE.EXE        XCOPY.EXE

src/cpu/paging.cpp          Paging and page handling code, TLB (translation lookaside buffer),
                            Page handlers

src/cpu/modrm.cpp           x86 mod/reg/rm effective address handling and lookup

src/cpu/mmx.cpp             Minimalist MMX register handling and effective address lookup

src/cpu/lazyflags.cpp       Lazy CPU flag evalulation. CPU flags are evaluated only if needed.

src/cpu/flags.cpp           CPU flag evaluation code.

src/cpu/cpu.cpp             NMI emulation, protected mode descriptors, stack push/pop,
                            Selector base/limit handling, CPL, flags, exception handling,
                            TSS (Task State Segment), task switching, I/O exception
                            handling, general exception handling, interrupt handling,
                            general flow control instruction handling, evaluation of
                            [cpu] section settings and application of settings and
                            changes to settings, I/O instruction stubs, model-specific
                            register emulation, CMPXCHG8B.

src/cpu/core_simple.cpp     Simple CPU core (core=simple). Uses normal core header files.
                            Core cannot be used if paging is enabled or when executing
                            from memory outside the valid range of system memory.

src/cpu/core_prefetch.cpp   Prefetch CPU core (cputype=*_prefetch). Uses normal core header files.
                            This core should be used for any application that is dependent
                            on CPU prefetch including anti-debugger, copy protection, or
                            self modifying code.

src/cpu/core_normal.cpp     Normal CPU core.

src/cpu/core_normal_286.cpp Normal CPU core, 286 emulation.

src/cpu/core_normal_8086.cpp Normal CPU core, 8086 emulation.

src/cpu/core_full.cpp       Full CPU core (core=full). Appears to have been borrowed from
                            Bochs.

src/cpu/core_dyn_x86.cpp    Dynamic CPU core (core=dynamic). On 32-bit x86 builds, this code
                            interprets the guest executable code and produces executable
                            code for the host process. This core is faster than the other
                            cores however it may have problems with paging and it does not
                            emulate CPU cycle counts accurately.

src/cpu/callback.cpp        DOSBox/DOSBox-X callback instruction and callback handling system.

src/debug/debug.cpp         Debugger, breakpoint handling and enforcement, debugger commands,
                            debugger interface, debug runtime loop (when broken into the
                            debugger)

src/debug/debug_gui.cpp     Debugger interface windowing system, GUI drawing, logging system
                            and LOG() C++ class, LOG_MSG() function, log file writing

src/debug/debug_disasm.cpp  16/32-bit i486 instruction disassembler, used in the debugger
                            to show instructions in the code window. Apparently taken from
                            the GNU debugger.

src/debug/debug_win32.cpp   Win32 console handling code, including resizing.

src/hardware/iohandler.cpp  I/O port handling code and registration system.

src/hardware/memory.cpp     Memory mapping, handling code, registration,
                            system RAM allocation, A20 gate control,
                            CPU reset vector handling, A20 config setting.

src/hardware/mixer.cpp      Audio mixer, audio system. All audio is mixed
                            in 1ms frames from all mixer channels. Other parts of
                            the emulator register mixer callbacks, where they are
                            called on to render up to 1ms of audio. All audio is
                            processed and rendered as 16-bit stereo PCM even if
                            the audio source provides 8/16-bit mono/stereo. See
                            mixer framework section for more information. This
                            also provides MIXER.COM on drive Z:, volume control
                            mapper shortcuts, menu controls "mute" and "swap stereo".

src/hardware/adlib.cpp      Adlib OPL2 and OPL3 emulation. Also provides the NukedOPL
                            emulation. Note that this is accomplished by including
                            nukedopl.h, and including opl.cpp twice inline. Once
                            for OPL2, and once for OPL3.

src/hardware/opl.cpp        This is the OPL2/OPL3 implementation, except for NukedOPL.

src/hardware/nukedopl.cpp   NukedOPL FM emulation.

src/hardware/sblaster.cpp   Sound Blaster emulation, overall. The same codebase
                            emulates Sound Blaster 1.0 through Sound Blaster 16
                            as well as ESS688 and Reveal SC400.

src/hardware/pci_bus.cpp    PCI bus emulation and framework.

src/hardware/vga.cpp        VGA emulation, modeset, resize event, lookup tables,
                            config parsing.

src/hardware/vga_attr.cpp   VGA attribute controller emulation

src/hardware/vga_crtc.cpp   VGA CRTC emulation

src/hardware/vga_dac.cpp    VGA DAC (palette) emulation

src/hardware/vga_draw.cpp   Code to draw pixels in each VGA mode, including PC-98

src/hardware/vga_gfx.cpp    VGA GFX (0x3CE-0x3CF) emulation

src/hardware/vga_memory.cpp VGA RAM and RAM access emulation, video RAM allocation

src/hardware/vga_misc.cpp   Misc VGA ports, including port 3DAh, 3C2h, 3CCh, 3CAh, 3C8h

src/hardware/vga_other.cpp  Other emulation, including CGA functions

src/hardware/vga_paradise.cpp Paradise SVGA emulation

src/hardware/vga_s3.cpp     S3 SVGA emulation

src/hardware/vga_seq.cpp    VGA sequencer emulation

src/hardware/vga_tseng.cpp  Tseng ET3000/ET4000 emulation

src/hardware/vga_xga.cpp    VGA XGA emulation

src/hardware/vga_pc98_cg.cpp PC-98 CG (character generator) emulation

src/hardware/vga_pc98_crtc.cpp PC-98 CRTC emulation

src/hardware/vga_pc98_dac.cpp PC-98 DAC (palette) emulation

src/hardware/vga_pc98_egc.cpp PC-98 EGC (extended graphics charger) emulation

src/hardware/vga_pc98_gdc.cpp PC-98 GDC (graphics display controller) emulation

src/hardware/voodoo.cpp     3Dfx Voodoo emulation

src/hardware/voodoo_emu.cpp 3Dfx Voodoo emulation

src/hardware/voodoo_interface.cpp 3Dfx Voodoo emulation

src/hardware/voodoo_opengl.cpp 3Dfx Voodoo emulation

src/hardware/voodoo_vogl.cpp 3Dfx Voodoo emulation

src/hardware/pc98.cpp       PC98UTIL.COM utility built-in command

src/hardware/pc98_fm.cpp    PC-98 FM board emulation (ties DOSBox-X to emulation
                            code borrowed from Neko Project II)

src/hardware/snd_pc98/*     PC-98 FM board emulation (code borrowed from
                            Neko Project II)

DOSBox-X menu framework
-----------------------

Instead of using a specific menu system directly, DOSBox-X uses a menu
framework as defined in include/menu.h and src/gui/menu.cpp.

This menu framework allows using the same menu item and menu layout
on all supported targets.

Prior to the framework, DOSBox-X menus were exclusively for Windows only
and defined in an *.rc file.

The design of the system is that all components of the emulator define
and register their menu items during init by a specific name. Mapper
shortcuts automatically register a menu item named "mapper_" + mapper
shortcut name.

Popup menus are also menu items by name, controlled by src/gui/menu.cpp.

The final layout is controlled by src/gui/menu.cpp which refers to
menu items by name and the order that they are arranged in.

The final layout can be seen through the display list in the menu object
and the display list in each menu item that was created as a submenu.
The display list contains the exact order that menu items are arranged.

In the SDL drawn menus, each menu item also contains the screen and
relative coordinates that were decided on when the menu object was last
called to rebuild or arrange the menus.

The SDL drawn menus are the only type that requires the main DOSBox-X
event loop to process menu events on their behalf including drawing
and reacting to mouse/touchscreen input. Windows and Mac OS X menus do
not require the main event loop's attention except when the user selects
an item.

Access to the menu items is by name, as well. get_item() returns a menu
item by reference, which itself contains methods to control the state
of the menu item and to reflect the changes to the menu framework.

Menu item methods return a reference to themselves to permit chaining
the calls on one line to keep visual clutter to a minimum.

The menu framework will call E_Exit() if the menu item by name does
not exist. Another method exists in the menu object to test if an
item exists by name.

It is expected that references returned from get_item() are used
short-term and never held onto for longer than needed. References
point directly to a vector within the menu object that can become
invalid if anything is done to cause the vector to resize. Always
call get_item() for a menu item to operate on it, never cache or
store the return value. Never add items while holding a reference.


Debugger interface
------------------

In builds where it is enabled, DOSBox-X supports breaking into the debugger
interface, which is shown on the console.

A mapper shortcut is provided to break into the debugger on demand. Normally
this shortcut is set to Alt+Pause.

In Windows, DOSBox-X can create a console and show the debugger interface
on it.

On other systems including Linux and Mac OS X, DOSBox-X must be started
from a terminal in order to enable the debugger.

The debugger interface should scale and respond to resizing of the
terminal window.

WARNING: Fitting to the window was added in DOSBox-X. The debugger
         interface in DOSBox SVN requires a minimum terminal window
         size to function, and may segfault if the terminal is too
         small.

The debugger interface is written against the "ncurses" library.

The window regions of the debugger interface are:
 - Register Overview
 - Data view
 - Code Overview
 - Variable (not shown by default)
 - Output

The register window will show at all times the contents of the CPU
registers and segment registers as well as other important CPU
state.

Data view allows viewing the contents of memory while debugging.
The location shown is controlled by a segment:offset pair.

In DOSBox-X, the Data view also permits viewing data as a linear
(pageable) offset and as a physical memory view (outside the
CPU's paging control).

The code overview/disassembly window shows the contents of a
memory location as disassembled x86 instructions. Normally, this
is set to the instruction pointer, but it can be set anywhere.
Decoding is based on the CPU mode.

The variable list is used when the debugger is given variables
to debug by.

The output window allows you to scroll through the last 1000
or so log messages written from within the codebase by LOG()
or LOG_MSG(). If the window is scrolled to the bottom, new
messages will appear by default.

The lowest row of the terminal is reserved for a line where
the user can enter debugger commands. An underscore shows
where the cursor is positioned.

The code and data views have been fixed in DOSBox-X to indicate
when data is not available to view for a specific segment:offset
or linear address.

If the CPU is in protected mode, and the segment portion refers
to a segment that does not exist, or the offset extends past
the limit of that segment, the code or data view will show
'na' instead of a byte value.

If 80386 paging is enabled, and the segment:offset or linear
address refers to a page that is not present, then the data
view will show 'pf' to indicate this.

  na = segment does not exist, or offset exceeds segment limit

  pf = segment:offset or linear address is paged out or
       not present according to page tables.


Debugger keyboard shortcuts
---------------------------

Tab                 Switch between windows
F5                  Resume emulation
F9                  Set/clear breakpoint
F10                 Single step (over)
F11                 Single step (into)
Up arrow            Scroll up one line (if applicable)
Down arrow          Scroll down one line (if applicable)
Page Up             Scroll up by window height (if applicable)
Page Down           Scroll down by window height (if applicable)


Debugger commands
-----------------

MOVEWINDN                                   Move current window down
MOVEWINDU                                   Move current window up
SHOWWIN <winname>                           Show window (by name)
HIDEWIN <winname>                           Hide window (by name)
MEMDUMP <seg> <off> <bytecount>             Dump memory to file (MEMDUMP.TXT)
MEMDUMPBIN <seg> <off> <bytecount>          Dump memory to file (MEMDUMP.BIN)
IV <seg> <off> <name>                       Insert variable
SV <name>                                   Save variables to <name>
LV <name>                                   Load variables from <name>
SR <reg> <value>                            Set register <reg> value to <value>
SM <seg> <off> [bytes in hex]               Set memory at <seg>:<off> to byte values given
BP <seg> <off>                              Add breakpoint (real mode)
BPM <seg> <off>                             Add breakpoint (protected mode)
BPLM <offset>                               Add breakpoint (linear/virtual address)
BPINT <intnum>                              Add breakpoint on interrupt
BPINT <intnum> <value>                      Add breakpoint on interrupt and AH=<value>
BPLIST                                      List breakpoints
BPDEL <breakpoint number>                   Delete breakpoint
RUN                                         Resume emulation
RUNWATCH                                    Resume emulation, but show state while running
A20                                         Show A20 gate state
A20 ON                                      Turn on A20 gate
A20 OFF                                     Turn off A20 gate
PIC                                         Show interrupt controller state
PIC MASKIRQ <irq>                           Mask IRQ at interrupt controller
PIC UNMASKIRQ <irq>                         Unmask IRQ at interrupt controller
PIC ACKIRQ <irq>                            Acknowledge IRQ at interrupt controller
PIC LOWERIRQ <irq>                          Manually lower interrupt signal
PIC RAISEIRQ <irq>                          Manually raise interrupt signal
C <seg> <off>                               Set code view to address
D <seg> <off>                               Set data view to address (segment:offset)
DV <offset>                                 Set data view to address (linear/virtual address)
DP <offset>                                 Set data view to address (physical)
LOG <hexadecimal count>                     Log CPU state, for the specified number of instructions, to LOGCPU.TXT
LOGS <hexadecimal count>                    Log CPU state, short log, to LOGCPU.TXT
LOGL <hexadecimal count>                    Log CPU state, long log, to LOGCPU.TXT
INTT <intnum>                               Trace interrupt
INT <intnum>                                Start interrupt
SELINFO <n>                                 Show selector information
DOS MCBS                                    Dump DOS kernel MCB chain (conventional memory allocation chain)
DOS KERN                                    Dump DOS kernel memory allocation list
DOS XMS                                     Dump XMS (extended memory) allocation list
DOS EMS                                     Dump EMS (expanded memory) allocation list
BIOS MEM                                    Dump BIOS allocation and layout list
GDT                                         Dump GDT (global descriptor table)
LDT                                         Dump LDT (local descriptor table)
IDT                                         Dump IDT (interrupt descriptor table)
PAGING                                      Dump page table information
CPU                                         Dump additional CPU information
INTVEC <file>                               Dump interrupt vector table to <file>
INTHAND <intnum>                            Set code view to start of interrupt handler
EXTEND                                      Toggle additional information
TIMERIRQ                                    Start timer IRQ
HEAVYLOG                                    Toggle heavy CPU logging
ZEROPROTECT                                 Toggle zero protection
HELP                                        Show some debugger commands for reference


Mixer audio framework
---------------------

Audio is rendered from all sources once a millisecond (once per tick).

Audio is rendered to 16-bit stereo at the sample rate of the user's
choice (in dosbox-x.conf).

Audio may be rendered within the 1ms tick at any point if code calls
the MIXER_FillUp() function or FillUp() member of a mixer channel.
Typically that is done when a significant state change is made to
an audio source in order to render accurately while not rendering
once per sample in an inefficient manner.

It is important to note that when a significant state change happens,
the device calls FillUp() first to render audio UP TO THAT POINT,
then applies the state change.

When the 1ms tick is completed, the audio is filled out to 1ms
and then sent off to a circular buffer where it can be picked up
and sent to the sound card when the Simple Directmedia Library
calls to pick it up.

In DOSBox-X, the mixer is written to render exactly 1ms at the
sample rate per 1ms of emulator time. Fractional integer math
is carried out in src/hardware/mixer.cpp to ensure the exact
number of samples is rendered.

Audio is rendered down to a common mixer buffer that is at least
16384 samples large.

The mixer channel specifies the sample rate of the source, so that
the mixer can upsample properly. The source format is determined
at the time of writing to the mixer channel. The source is free
to change from 8/16-bit PCM mono/stereo at any time.

In DOSBox-X, there is additional framework provided to emulate
analog properties and DAC characteristics through lowpass filters
and rate vs slew rate interpolation.

Normally, the source does not specify a lowpass filter nor does
it provide a slew rate. In that case, normal linear interpolation
is applied on upsample.

If the source provides a slew rate, the slew rate is used for
linear interpolation. If the slew rate is higher than the sample
rate, then the interpolation within the sample completes faster.
If the slew rate is lower than the sample rate, the interpolation
will be done too slow to complete fully before the next sample.

The reason for slew rate rendering is simple. DACs without filters
change instantaneously between samples. This is what gives older
sound cards (including the older Sound Blaster cards) their grungy
metallic characteristic. Sound cards since then filter the audio
after the DAC (or filter as part of DAC output) to smooth
transitions between samples to improve sound quality for low sample
rates.

However, as anyone knows in the analog domain, transistors do not
actually change instantaneously. There is a transition period from
ON to OFF, and OFF to ON, however fast it is. The slew rate parameter
specifies the "sample rate" that defines the transition period.
The higher the slew rate, the faster the transition.

The lowpass filter is there to simulate the analog filtering post
DAC. In most cases, a sound card could be thought of as a DAC with
or without DAC interpolation, put through an audio amplifier
circuit that can only amplify and pass up to about 20KHz.

Sound Blaster 1.0/2.0 emulation in DOSBox-X for example is written
to simulate a DAC with a slew rate of 16-20KHz and a lowpass filter
of 20KHz, to simulate the grungy metallic flavor of the sound.

Sound Blaster Pro adds to the setup by using the lowpass parameter
according to the "filter bit" in the mixer registers.

Sound Blaster 16 and ESS emulation simulates the newer DACs using
normal linear interpolation and the lowpass filter according to
the source rate.

Within the mixer framework, audio routing is provided to render
to a WAV file if instructed by the user, and to the audio track
of an AVI file if also instructed by the user.

In DOSBox-X, individual audio channels from each source are also
recorded to an AVI file that has one audio track per channel, to
allow recording each channel individually. The intent of this
setup is to enable editing audio and video from a DOS game with
the ability to selectively disable unrelated audio or extract
game music without the sound effects in ways appropriate for
video production.

In DOSBox SVN, audio rendering is driven by the SDL audio device,
which may (usually) or may not drift slightly from emulation.

In DOSBox-X, audio rendering is tied to emulation time, and
audio/video sync will never drift in a captured AVI file.

In DOSBox-X, if compiled against FFMPEG, the captured audio may
be sent instead to the AAC codec and muxed into an MPEG transport
stream as part of video capture.

A device creates a mixer channel by calling MIXER_AddChannel()
to create another audio channel. The function will return a pointer
to a mixer channel object which can then be directed to start,
stop, and render audio. The function will also call the callback
handler function given at creation time when audio rendering is
needed.

When a device is finished with the channel, it should call
MIXER_DelChannel() to destroy the channel.

Mixer channel enumeration is possible with MIXER_FirstChannel()
or MIXER_FindChannel(). Mixer channels are linked together in
a singly linked list when active.

src/hardware/mixer.cpp also registers a .COM program on
drive Z: that can be used to list mixer channels and control
mixer volume.


Sound Blaster emulation
-----------------------

src/hardware/sblaster.cpp emulates all models of Sound Blaster
from Sound Blaster 1.0 through Sound Blaster 16. In DOSBox-X,
additional code was added to emulate the ESS688 and SC400
cards as well.

The code is written to be as accurate as possible about
the state and function of Sound Blaster cards, including many
undocumented quirks.

Additional hacks were added for additional tricks that some
old DOS games and demos use. One such hack is "goldplay" mode,
referring to an old music tracker playback library that
supported playing MOD files to LPT DAC, PC speaker, and
Sound Blaster. The reason a hack was added is due to the
way this library renders single-sample output via DMA.
Instead of normal DMA, the library allocates a 1-sample
buffer and instructs the DMA controller to loop over the
single sample. The timer interrupt then overwrites the
1-sample buffer at the sample rate it believes is the
best to render at.

In DOSBox SVN, sample rates are not capped.

In DOSBox-X, sample rates are capped according to the
behavior of the actual hardware. That includes the 23KHz
cap for non-highspeed and 45KHz cap for highspeed DSP
playback.

The emulation is written in a fairly straightforward way
that should be easy to modify if needed. DSP commands are
collected into a buffer according to a table that indicates
how long each command is from the first byte.

A buffer is used to return DSP bytes read back from the
sound card.

Unless otherwise asked, the Sound Blaster code will also
register I/O handlers for and initialize OPL2/OPL3 FM
emulation at port 0x388. The code may initialize Game
Blaster compatible CMS emulation as well.

On DOS kernel initialization, the Sound Blaster emulation
will also automatically create the BLASTER environment
variable. This environment variable is used by many DOS
games to find the sound card. Some games require it,
while others will probe manually for the sound card.


VGA emulation
-------------

The VGA emulation written in DOSBox-X is written in two
parts.

The first part concerns IBM PC/XT/AT emulation and VGA
emulation, with adjustments for some SVGA chipsets and
for MDA/Hercules/Tandy/EGA as well.

The second part concerns NEC PC-98 and emulation of it's
subsystem.

The two parts work somewhat independently. Which one
becomes active depends on the machine= setting, whether
it enables IBM PC or NEC PC-98 emulation.

All VGA emulation is tied to a VGA state structure that
is globally visible in the code. The state stores in the
structure exactly or closely mirrors the values written
to the registers. Extended state is either carried in
the same registers or stored in other values. Extended
state is generally stored and represented as if emulating
the S3 chipset. Some fields are either extended or
stored separately when holding Tandy/PCjr state.

All PC-98 emulation is tied to the GDC controller emulation
state of both GDCs (master and slave). VGA state is not
used much in PC-98 mode.

Support is not implemented for oddball PC-98 GDC state such
as programming the master and slave GDCs to run out of
sync with each other. Custom modes are supported however,
such as the custom video timing used in Ishtar.

VGA state is determined by the register contents (low level)
instead of INT 10h mode (high level). The register contents
are used to select a mode enumeration which affects the
way video memory is rendered. The modes are the M_*
enumeration constants defined in include/vga.h.

    M_CGA2          CGA 1bpp 2-color mode (e.g. 640x200)
    M_CGA4          CGA 2bpp 4-color mode (e.g. 320x200)
    M_EGA           EGA/VGA 4bpp 16-color planar modes
    M_VGA           VGA 8bpp 256-color modes, usually chained planar, or highcolor DAC output
    M_LIN4          SVGA 4bpp 16-color planar modes
    M_LIN8          SVGA 8bpp 256-color modes (linear)
    M_LIN15         SVGA 16bpp highcolor modes (15bpp 5:5:5 RGB = XRRRRRGGGGGBBBBB)
    M_LIN16         SVGA 16bpp highcolor modes (16bpp 5:6:5 RGB = RRRRRGGGGGGBBBBB)
    M_LIN24         SVGA 24bpp truecolor modes (24bpp 8:8:8 RGB)
    M_LIN32         SVGA 32bpp truecolor modes (32bpp 8:8:8:8 ARGB)
    M_TEXT          Alphanumeric text modes (CGA/EGA/VGA/SVGA/Tandy/PCjr)
    M_HERC_GFX      Hercules 1bpp 2-color mode (usually 720x348)
    M_HERC_TEXT     MDA/Hercules alphanumeric text mode
    M_CGA16         CGA composite video emulation
    M_TANDY2        Tandy/PCjr 1bpp 2-color mode
    M_TANDY4        Tandy/PCjr 2bpp 4-color mode
    M_TANDY16       Tandy/PCjr 4bpp 16-color mode
    M_TANDY_TEXT    Tandy/PCjr text mode
    M_AMSTRAD       Amstrad 4bpp 16-color mode
    M_PC98          NEC PC-98 text/graphics output (combined)
    M_FM_TOWNS      Stub for FM Towns

    NOTICE: A string array is also defined for private use of the
            VGA_SetupDrawing() function for each M_ constant.
            If you add a new constant, you must add a string
            for that constant in src/hardware/vga_draw.cpp,
            or else DOSBox-X may segfault when announcing the
            video mode on mode change.

            The order of the enumeration MUST match the strings.

            The string array is defined (at the time of writing
            this documentation) at line 2755 of
            src/hardware/vga_draw.cpp.

VGA drawing setup is initialized using the VGA_SetupDrawing()
function in src/hardware/vga_draw.cpp. The function uses
machine type, VGA mode, and register state to determine the
active display area (used to size DOSBox's window) and
refresh rate.

DOSBox SVN will generally render the VGA display in quarters
of the screen, except when machine=vgaonly, where it will
render one line at a time.

DOSBox-X will always render one line at a time, in all video
modes.

Rendering one line at a time may be required if the DOS
game in question uses raster or palette effects that require
scanline precision. "Copper" effects in the demoscene, such
as the act of changing color palette entries per scanline to
produce moving bars of color, require per-line rendering to
appear correctly.

NOTE: To better understand the term "copper effects", read
      the following links describing the original
      Commodore Amiga video hardware:

      http://eab.abime.net/showthread.php?t=21866
      https://en.wikipedia.org/wiki/Original_Chip_Set#Copper

PIC events are used in the VGA code to trigger rendering
a scanline at the interval determined by the horizontal
sync rate. Some additional events are used for horizontal
blank, sync, and return to active display. The VGA system
is set up so that these events set up continuous rasterization
of the display.

If for any reason, these events should stop, the display
in the emulator window will stop updating.

Rendering of VGA memory per scanline is carried out using
a function pointer to a function assigned by the last
call to VGA_SetupDrawing(). At call time, the function
is expected to render to a buffer and return the pixel
data as the return value. In most cases, that is done
by rendering to a specific temporary buffer set aside
for rasterizing, and then returning any number of bytes
(0 to 31) from the start of the buffer. VGA text rendering
may return 0 to 8 pixels from the start depending on the
horizontal panning (hpel) register state for example.

Per-scanline rendering is handled by PIC event
VGA_DrawSingleLine() which calls the render function
and manages VGA state as it advances through the
scan lines and addresses in memory.

Vertical retrace is handled by PIC event
VGA_VerticalTimer().

Emulation of a vertical retrace interrupt is handled by
PIC event VGA_VertInterrupt. This event has code to
emulate the IRQ 2/9 interrupt of EGA and the IRQ 2
vsync interrupt in PC-98 mode.

It is known that some double-buffered VGA registers
take effect some time between blanking at the end of
the active display, and active display at the start.
Whenever that happens the PIC event function
VGA_DisplayStartLatch() is set up to emulate the
transfer of those registers.

DOSBox SVN will generally render per scanline in
video memory, using the scaler to double the scanline
if appropriate. machine=vgaonly however may override
that.

DOSBox-X will generally render all scanlines that
would be sent to a CRT, meaning that 200-line modes
are doubled, by default for accuracy. This can be
disabled by setting doublescan=false to get DOSBox
SVN behavior.

The reason it matters is that the doublescan
behavior prevents the advanced scalers (such as 2xsai)
from working. To enable these scalers, turn off
the doublescan mode.

VGA memory size and allocation is handled in
src/hardware/vga_memory.cpp. VGA memory is mapped
to the appropriate memory ranges depending on
register state, machine type, and video mode in
VGA_SetupHandlers(). Register-level emulation of
certain registers will call VGA_SetupHandlers()
if it might or will affect how VGA memory is
mapped.

VGA_SetupHandlers() also determines the callback
handler used to respond to video memory access
from the CPU. Register state and hardware state
determine how the video hardware handles read
and write operations, this is where it's handled.

In the simplest case, VGA_SetupHandlers() will
emulate straightforward memory access for
MDA/CGA/Hercules text and graphics modes. No
advanced logic is involved, video RAM behaves
like normal RAM.

In the more complex cases, especially EGA/VGA,
a handler is set up to accept reads and writes
and route it through the read/write modes that
determine how it's handled across the planar
memory of the EGA/VGA hardware.

In PC-98 mode, the memory handler accepts
read/write to text and character RAM,
non-volatile RAM, and maps read/write operations
to graphics RAM through the state of the EGC
hardware.

There is separate code for S3 emulation to map
a linear framebuffer to an extended memory
address (currently 0xE0000000) defined in
src/hardware/memory.cpp. VESA BIOS emulation
involving linear framebuffer modes rely on
S3 emulation to provide them. There are registers
emulated by src/hardware/vga_s3.cpp that
indirectly control the linear framebuffer.

In DOSBox SVN, the linear framebuffer is
handled directly by the MEM_GetPageHandler()
function.

In DOSBox-X, the linear framebuffer is given
using the memory callout system when reads
and writes are issued to that range for the
first time.

Note that the linear framebuffer handler also
includes the memory-mapped I/O registers also
provided by S3 chipsets.

VGA memory handlers, just like any other
memory handler, must translate the given address
to the physical address before handling.

For some strange reason, DOSBox SVN page
mapping calls the memory handler with the CPU's
virtual memory address. This behavior was
inherited by DOSBox-X when forked from DOSBox SVN.

Convert the virtual address to physical using
the PAGING_GetPhysicalAddress() function before
using it in video RAM emulation.

The memory handler C++ base class is written
so that there are methods for getting a host
pointer or handling memory read/write as
a byte, word, or dword.

A memory handler object can simplify code by
implementing only the byte handler, and letting
the C++ base class break word and dword I/O
down to byte access. Memory handlers can
implement their own word/dword handlers if
the device requires different handling for
larger than byte sized I/O, or for performance
reasons.

The base C++ class of a memory handler has a
flags member that describes how to handle
memory I/O. The constructor can call down
to the base constructor with the flag value
to initialize by.

DOSBox-X includes code to consume CPU cycles
on memory I/O to simulate the fact that, at
least on older hardware, video memory is slower
than system memory. There are older DOS games
that rely on slow system memory, and they will
run too fast without it.

EGA/VGA planar write modes are handled
in src/hardware/vga_memory.cpp function
ModeOperation().

EGA/VGA planar memory is emulated by treating
the allocated RAM as if an array of 32-bit
unsigned integers (uint32_t). Each bitplane
occupies 8 bits within that 32-bit unsigned
int. Keeping the planes together enables
faster more efficient emulation of VGA
bit planar operations including copying
with write mode 2 and raster operations,
as well as Mode X tricks used by older
DOS games.

Due to the general way that video modes are
handled on EGA/VGA/SVGA hardware, all modes
including text mode must operate within the
constraints of bytes through the planar
layout of VGA video RAM.

One good example of that requirement is the
Windows 95 "boot logo", which relies on
IO.SYS setting 320x400 256-color Mode X, then
resetting BIOS and VGA hardware state so that
DOS and INT 10h still think the display is in
text mode, and the VGA hardware continues to
accept writes to B8000 as if running in text
mode. In this way, the text console can
continue to show console output underneath
the Windows 95 logo unscathed. When Windows
95 switches the VGA hardware back to text
mode, whatever was written to the console
is revealed.

Some exceptions are made for SVGA chipsets
known to function differently, such as the
Tseng ET3000/ET4000 chipsets known to operate
VGA Mode X differently from standard VGA.

VGA planar memory is handled in the code by
typecasting video RAM as a 32-bit unsigned int
(uint32_t) determined by an index directly
computed from the planar byte offset.

All EGA/VGA modes are mapped on top of this
planar memory structure, in the same way that
real hardware maps it. This includes text
mode (planes 0 & 1 for char/attribute),
CGA modes (planes 0 & 1 for even/odd bytes),
and 256-color mode (every 4 pixels is one
planar byte, and the pixel in that group
is mapped to a plane).

The code as written depends on a host CPU
that is little endian, including bit masks
and shift operations. Bitmask computation
will need to be altered to work correctly
on big endian systems in order to work
correctly through the uint32_t typecast.

To aid with planar memory, a VGA_Latch
union is defined that allows the uint32_t
to be accessed as one whole unit or
individual bytes of video memory from the
base of the bitplane up.

The uint32_t masks and shifting should be
maintained so that b[0] to b[3] refer to
the same bytes of video memory. Only uint32_t
should be handled differently to accomodate
host byte order.

For more information, see vga/hardware/vga_memory.cpp
and vga/hardware/vga.cpp where these bitmasks
are computed and used.


Foreign keyboard layouts
------------------------

DOSBox-X was developed around the US keyboard layout,
with only a few additional layouts natively supported.

To add additional layouts, see file "README.keyboard-layout-handling"
on how to do so as a developer.

