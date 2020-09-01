**README for DOSBox-X** (official website: [dosbox-x.com](http://dosbox-x.com))

Introduction to DOSBox-X
------------------------

DOSBox-X is a cross-platform DOS emulator based on the DOSBox project (www.dosbox.com).

Like DOSBox, it emulates a PC necessary for running many MS-DOS games and applications that simply cannot be run on modern PCs and operating systems. However, while the main focus of DOSBox is for running DOS games, DOSBox-X goes much further than this. Started as a fork of the DOSBox project, it retains compatibility with the wide base of DOS games and DOS gaming DOSBox was designed for. But it is also a platform for running DOS applications, including emulating the environments to run Windows 3.x, 9x and ME and software written for those versions of Windows.

Our goal is to eventually make DOSBox-X a complete DOS emulation package, both fully-featured and easy to use, and give the users all the options to configure the DOS virtual machine. We implement new features with each official release, and also try our best to deliver a consistent cross-platform experience for users instead of focusing on a particular platform. In order to help improve the general DOS emulation and also to aid retro-programmming, it is our desire to maintain and implement more accurate emulation, but at the same time we are also making efforts to improve emulation quality, speed, and usability for end users. Furthermore, we hope to improve the out-of-the-box experience for new users who want to run DOS programs or games.

Please see the [INSTALL](INSTALL.md) page for DOSBox-X installation instructions and latest packages, and the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page for all released DOSBox-X versions. For more information about DOSBox-X, such as setting up and running DOSBox-X including its usage tips, please read the user guide in the [DOSBox-X Wiki](https://github.com/joncampbell123/dosbox-x/wiki).

DOSBox-X is completely open-source and free of charge to use and distribute. It is released under the [GNU General Public License, version 2](COPYING).

This project has a [Code of Conduct](CODE_OF_CONDUCT.md), please read it for general information on contributing to or getting support from the project.

Brought to you by: joncampbell123 (Jonathan Campbell)

New information will be added to this README over time.


Notable features in DOSBox-X
----------------------------

Although based on the DOSBox project, DOSBox-X is now a separate project because both have their own separate schedules and development priorities. For example, the main focus of DOSBox is for running DOS games whereas DOSBox-X goes way beyond this. At this time DOSBox-X already has a great number of features that do not exist in DOSBox. Examples of such features include:

* GUI menu bar and built-in graphical configuration tool

* Save and load state support (with save slots)

* Japanese NEC PC-98 mode emulation

* Better compatibility with DOS applications

* Support for more DOS commands and built-in external tools

* Support for CPU types like Pentium Pro and MMX instructions

* Support for IDE interfaces and improved Windows 3.x/9x emulation

* Support for long filenames and FAT32 disk images (DOS 7+ features)

* Support for printer output, either a real or virtual printer

* Support for 3dfx Voodoo chip and Glide emulation

* Support for cue sheets with FLAC, MP3, WAV, and OGG Vorbis CD-DA tracks

* Support for FluidSynth MIDI synthesizer and Innovation SSI-2001 emulation

* Support for NE2000 Ethernet for networking and modem phone book mapping

* Support for features such as V-Sync, overscan border and stereo swapping

* Plus many more..

While the great majority of features in DOSBox-X are cross-platform, DOSBox-X does also have several notable platform-dependent features, such as support for automatic drive mounting, clipboard copy and paste, and starting programs to run on the host (-winrun) on the Windows platform. These features cannot be easily ported to other platforms. More information about DOSBox-X's features can be found in the [DOSBox-X Wiki](https://github.com/joncampbell123/dosbox-x/wiki).

DOSBox-X officially supports both SDL 1.2 and SDL 2.0; both 32-bit and 64-bit builds are also supported.


DOSBox-X's supported platforms and releases
-------------------------------------------

DOSBox-X is a cross-platform DOS emulator, so all major host operating systems are officially supported, including:

1. Windows XP or higher, 32-bit and 64-bit

2. Linux (with X11), 32-bit and 64-bit

3. macOS (Mac OS X) Sierra 10.12 or higher 64-bit

4. MS-DOS or compatible (special HX-DOS versions)

Windows binaries (both 32-bit and 64-bit), Linux RPM packages (64-bit), macOS packages (64-bit) and DOS versions are officially released periodically, typically on the last day of a month or the first day of the next month. Please see the [INSTALL](INSTALL.md) page for the latest DOSBox-X packages on these platforms and further installation instructions. You can also find ZIP (or Linux RPM) packages for all released versions and their change history in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page. 

The latest version of DOSBox-X was released on August 2, 2020. If you use Windows, please note that the default Windows releases built with Visual Studio only support Windows Vista and later (Windows 7, 8, and 10); for Windows XP, the MinGW builds are required. Auto-installable Windows packages for DOSBox-X are also available to ease the installation process, which are especially recommended for new and non-expert users. If you use the installers you will be able to start DOSBox-X as soon as the installation ends. The all-in-one Windows installer for the latest official version of DOSBox-X can be downloaded from: [DOSBox-X-Setup-Windows-latest.exe](https://github.com/Wengier/dosbox-x-wiki/raw/master/DOSBox-X-Setup-Windows-latest.exe)

For running DOSBox-X in a real DOS system (MS-DOS or compatible), please use the special HX-DOS builds. It is achieved with the help of the freely-available [HX DOS Extender](https://github.com/Baron-von-Riedesel/HX), which is already included in the recent HX-DOS release packages. However, not all features of DOSBox-X can be supported in this environment. Moreover, while the HX-DOS builds may sometimes happen to also run on Windows, they are made for the HX DOS Extender environment so it is strongly recommended to use the Visual Studio or MinGW builds (both included in the Windows installer) for the Windows platform instead.

The full source code is officially provided with each DOSBox-X release, which may be compiled to run on the above and other operating systems too. You can also get the latest development source code from the repository directly. See also the [DOSBox-X source code description](README.source-code-description) page for information on compiling the source code.


Compatibility with DOS programs and games
-----------------------------------------

With the eventual goal of being a complete emulation package that covers all pre-2000 DOS and Windows 3.x/9x based hardware scenarios, we are making efforts to ensure that the vast majority of DOS games and applications will run in DOSBox-X, and these include both text-mode and graphical-mode DOS programs. Microsoft Windows versions that are largely DOS-based (such as Windows 3.x and 9x) are officially supported by DOSBox-X as well. Note that certain config settings may need to be changed from the default ones for some of these programs to work smoothly. Take a look at the [DOSBox-X Wiki](https://github.com/joncampbell123/dosbox-x/wiki) for more information.

Efforts are also made to aid retro DOS developments by attempting to accurately emulate the hardware, which is why DOSBox-X used to focus on the demoscene software (especially anything prior to 1996) because that era of the MS-DOS scene tends to have all manner of weird hardware tricks, bugs, and speed-sensitive issues that make them the perfect kind of stuff to test emulation accuracy against, even more so than old DOS games. But without a doubt we are also making a lot of efforts to test DOSBox-X against other DOS games and applications, as well as PC-98 programs (most of them are games).

We add new features and make other improvements in every new DOSBox-X version, so its compatibility with DOS programs and games are also improving over time. If you have some issue with a specific DOS program or game, please feel free to post it in the [issue tracker](https://github.com/joncampbell123/dosbox-x/issues).


Contributing to DOSBox-X
------------------------

We encourage new contributors by removing barriers to entry.
Ideas and patches are always welcome, though not necessarily accepted.

If you really need that feature or change, and your changes are not
accepted into this main project (or you just want to mess around with
the code), feel free to fork this project and make your changes in
your fork.

As joncampbell123 only has limited time to work on DOSBox-X, help is
greatly appreciated:

  - Testing
    - Features of DOSBox-X, such as its commands and functions 
    - The normal operation of DOS games and applications
    - Software or hardware emulation accuracy, helped by for example demoscene software
    - Windows 1.0/2.x/3.x & Windows 9x/ME guest system support
    - Retro development
  - Bug fixes, patches, improvements, refinements
  - Suggestions, ideas, general conversation
  - Platform support (primarily Linux and Windows, but others are welcome)
  - Documentation, language file translation
  - Notes regarding games, applications, hacks, weird MS-DOS tricks, etc.

See the [CONTRIBUTING](CONTRIBUTING.md) page for more contribution guidelines.
If you want to tweak or write some code and you don't know what to work on,
feel free to visit the [issue tracker](https://github.com/joncampbell123/dosbox-x/issues) to get some ideas.

For more information about the source code, please take a look at the
[DOSBox-X source code description](README.source-code-description) page.

Information about the debugger is also available in the
[DOSBox-X Debugger](README.debugger) page.

See also the [CREDITS](CREDITS.md) page for crediting information.


DOSBox-X’s development/release pattern
--------------------------------------

In order to make DOSBox-X's development process more smooth, we have implemented a general development/release pattern for DOSBox-X. The current release pattern for DOSBox-X is as follows:

New DOSBox-X versions are made public at the start (typically on the first day) of each month, including the source code and binary releases. Then the DOSBox-X developments will be re-opened for new features, pull requests, etc. There will be no new features added 6 days before the end of the month, but only bug fixes. The last day of the month is DOSBox-X’s build day to compile for binary releases the first of the next month, so there will be no source code changes on this day including pull requests or bug fixes.

For example, suppose August is the current month - August 25th will be the day pull requests will be ignored unless only bug fixes. August 31st (the last day of August) will be DOSBox-X build day.

This is DOSBox-X’s official release pattern, although it may change later.


Future development experiments
------------------------------

Scattered experiments and small projects are in experiments/ as proving grounds for future revisions to DOSBox-X and its codebase.

These experiments may or may not make it into future revisions or the next version.

Comments are welcome on the experiments, to help improve the code overall.

There are also patches in patch-integration/ for possible feature integations in the future. We have already integrated many community-developed patches into DOSBox-X in the past.

See also [General TODO.txt](PLANS/General%20TODO.txt) for some plans of future DOSBox-X developments.


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


Features that DOSBox-X is unlikely to support at this time
----------------------------------------------------------

DOSBox-X aims to be a fully-featured DOS emulation package, but there are
some things the design as implemented now cannot accomodate.

* Pentium II or higher CPU level emulation.

  DOSBox-X contains code only to emulate the 8088 through the Pentium Pro.
  Real DOS systems (MS-DOS and compatibles) also work best with these CPUs.

  If Pentium II or higher emulation is desired, consider using Bochs
  or QEMU instead. DOSBox-X may eventually develop Pentium II emulation,
  if wanted by the DOSBox-X community in general.

* Emulation of PC hardware 2001 or later.

  The official cutoff for DOSBox-X is 2001, when updated "PC 2001"
  specifications from Microsoft mandated the removal of the ISA slots
  from motherboards. The focus is on implementing hardware emulation
  for hardware made before that point.

  Contributers are free to focus on emulating hardware within the
  timeframe between 1980 and 2000/2001 of their choice.

* Windows guest emulation, Windows XP or later.

  DOSBox-X emulation, in terms of running Windows in DOSBox-X, will
  focus primarily on Windows 1.0 through Windows Millenium Edition,
  and then on Windows NT through Windows 2000. Windows XP and later
  versions are not a priority and will not be considered at this time.

  If you need to run Windows XP and later, please consider using
  QEMU, Bochs, VirtualBox, or VMware.

* Any MS-DOS system other than IBM PC/XT/AT, Tandy, PCjr, and NEC PC-98.

  Only the above listed systems will be considered for development
  in DOSBox-X. This restriction prevents stretching of the codebase
  to an unmanageable level and helps keep the code base organized.

  It would be easier on myself and the open source community if
  developers could focus on emulating their platform of interest in
  parallel instead of putting everything into one project that,
  most likely, will do a worse job overall emulating all platforms.
  However, if adding emulation of the system requires only small
  minimal changes, then the new system in question may be considered.

  You are strongly encouraged to fork this project and implement
  your own variation if you need to develop MS-DOS emulation for
  any other system or console. In doing that, you gain the complete
  freedom to focus on implementing the particular MS-DOS based
  system of interest, and if desired, the ability to strip away
  conflicting IBM PC/XT/AT emulation and unnecessary code to keep
  your branch's code manageable and maintainable.

  If you are starting a fork, feel free to let me know where your
  fork is and what system it is emulating, so I can list it in
  this README file for others seeking emulation of that system. To
  help, I have added machine and video mode enumerations as "stubs"
  to provide a starting point for your branch's implementation of
  the platform. A stub implemented so far is "FM Towns emulation"
  (machine=fm_towns).

* Cycle-accurate timing of x86 instructions and execution.

  Instructions generally run one per cycle in DOSBox-X, except for I/O
  and memory access.

  If accurate emulation of cycles per instruction is needed, please
  consider using PCem, 86Box, or VARCem instead.

* Full precision floating point emulation.

  Unless using the dynamic core, DOSBox and DOSBox-X emulate the FPU
  registers using the "double" 64-bit floating point data type.

  The Intel FPU registers are 80-bit "extended precision" floating point
  values, not 64-bit double precision, so this is effectively 12 bits of
  precision loss and 5 bits of range loss (64 to 53 mantissa bits and 16
  to 11 exponent bits). This slight loss of precision is perfectly fine
  considering DOSBox's original goal in supporting DOS games, but may
  cause problems in other cases that need the full precision.

  It is known at this time that this lack of precision is enough to
  cause otherwise straightforward comparisons against integers to
  fail in DOS applications originally written in QBasic or Turbo Basic.
  There are such DOS games written that check their file size using
  a floating point compare that will fail in this manner. To run these
  games, you will need to disable FPU emulation (fpu=false) to force
  the QBasic/TurboBasic runtime to use software emulation instead.


Origin and history of the DOSBox-X project
------------------------------------------

DOSBox-X started as a fork of the original DOSBox project sometime
in mid-2011. It was started out of a desire to improve the emulator
without having to fight with or worry about submitting patches
upstream.

As its developers have made it clear, DOSBox's main focus is on
DOS games. This is evident by the fact that much of the code is
somewhat accurate code with kludges to make DOS games run,
instead of focusing on what hardware actually does.

Many of the changes I wanted to make were non-game related, and
therefore were unlikely to be accepted by the DOSBox developers.

Since then, I have been modifying the source code over time to
improve emulation, fix bugs, and resolve incompatibilities with
Windows 95 through ME. I have added options so that DOSBox-X
by default can emulate a wider variety of configurations more
accurately, while allowing the user to enable hacks if needed
to run their favorite DOS game. I have also been cleaning up
and organizing the code to improve stability and portability
where possible.

The original DOSBox project was not written by one programmer. It
has been under development since late 2000 with patches, fixes,
and improvements from members all over the Vogons forums. Despite
not having a major official release since DOSBox 0.74 over 10
years ago, the project is still in active development today.
Meanwhile, some of the changes themselves incorporated code from
other projects.

Some features and improvments in DOSBox-X also came from another
branch of DOSBox known as [DOSBox SVN Daum](http://ykhwong.x-y.net)
which itself incorporated features from the original DOSBox
project, DOSBox-X, and many experimental patches. Although the
Daum branch seems to be dead, the features borrowed from it still
exists in DOSBox-X.

Later on, DOSBox-X also incorporated several features and improvements
from other projects such as [DOSBox ECE](https://dosboxece.yesterplay.net/), [dosbox-staging](https://dosbox-staging.github.io/) and
[vDosPlus](http://www.vdosplus.org/), with major improvements and works from its contributors
such as Wengier, Allofich, and rderooy.

See also the [CREDITS](CREDITS.md) page for crediting of the source code.


Known DOSBox-X forks
--------------------

DOSBox-X Emscripten port (runnable in a web browser) by Yksoft1.
Significant changes are made in order to run efficiently within the web browser when compiled using LLVM/Emscripten.
These significant changes require dropping some useful features (including the menus) but are required for performance.

URL: https://github.com/yksoft1/dosbox-x-vanilla-sdl/tree/emscripten (look for clone URL and use the emscripten branch)


Support for international language translations and keyboard layouts
--------------------------------------------------------------------

DOSBox-X displays English as the default language, and uses the U.S. code page (437) by default, just like DOSBox.

All messages displayed by DOSBox-X are in English with the default setting. DOSBox-X does support the feature to
change the display messages with the use of language files. The language files control all visible output of the
internal commands and the internal DOS. If you are a speaker of a non-English language, you can create additional
language files for use with DOSBox-X by translating messages in DOSBox-X to your language. Other DOSBox-X users may
also use these language files for DOSBox-X to display messages in such language if they wish.

The fact that DOSBox-X was developed around the U.S. keyboard layout is primarily due to limitations around the SDL1
library which provides input handling. As such when using the SDL1 version and a non-US keyboard, DOSBox-X automatically
uses scancodes with the default setting to work around keyboard layout issues. Scancodes are not needed when using
non-US keyboard layouts in the SDL2 version. If you find that a keyboard layout is not yet supported by DOSBox-X,
in order to add additional layouts for use with DOSBox-X, please see file [README.keyboard-layout-handling](README.keyboard-layout-handling)
on how to do so as a developer.

For further information on international support and regional settings of DOSBox-X, such as steps to create DOSBox-X
language files or use external keyboard files in DOSBox-X, as well as support for the Euro symbol and country-specific date and time formats, please look at the user guide in the [DOSBox-X Wiki](https://github.com/joncampbell123/dosbox-x/wiki).
