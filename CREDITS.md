Credits
=======

Jonathan Campbell, the maintainer of DOSBox-X does not claim to have written all of the code in this project.

The purpose of this document is to try and build a comprehensive list of source code in this repository that was borrowed from other projects.

The base code is from the [DOSBox](https://www.dosbox.com) project in which most of the SVN commits made since 2011 were incorporated into DOSBox-X. This code had since been heavily modified by the DOSBox-X project.

Some of the source code also came from other projects such as [DOSBox SVN Daum](http://ykhwong.x-y.net), [DOSBox ECE](https://dosboxece.yesterplay.net/), [DOSBox Staging](https://dosbox-staging.github.io/), [DOSVAX](http://radioc.web.fc2.com/soflib/dosvax/dosvax.htm)/[DOSVAXJ3](https://www.nanshiki.co.jp/software/dosvaxj3.html), and [vDosPlus](http://www.vdosplus.org/), with major works from other developers and contributors of DOSBox-X such as Wengier, aybe, Allofich, and rderooy.

A list of features ported from DOSBox SVN Daum (maintainer: ykhwong):

* GUI menu bar (heavily improved since then)
* Some commands (PROMPT, MOUSE, VOL, DEVICE, etc)
* Basic support for automatic drive mounting (Windows)
* Printer output (improved since then by Wengier)
* NE2000 Ethernet (improved since then by Wengier)
* MT-32 emulation (MUNT; updated to newer version since then by Wengier)
* Internal 3dfx Voodoo card emulation (improved since then by Wengier and joncampbell123 along with code ported from DOSBox ECE)
* Some support for FluidSynth MIDI synthesizer
* Improved PC Speaker emulation accuracy
* CGA with Monochrome Monitor Support
* Support for CPU types like Pentium Pro
* Features such as V-Sync, xBRZ scaler, overscan border and stereo-swap
* Various patches such as DBCS and font patch

A list of features ported from DOSBox ECE (maintainer: YesterPlay):

* Support for FLAC, MP3, WAV, OGG Vorbis and Opus CD-DA tracks (with some decoders ported and cleaned up from DOSBox Staging and DOSBox Optionals by Wengier)
* Support for mapping mouse buttons to keyboard
* Improved support for 3dfx emulation (Glide wrapper and improved internal card emulation; both improved since then by Wengier)
* Improved support for FluidSynth MIDI synthesizer (internal FluidSynth support added for Windows by Wengier)
* Updated Nuked OPL3 to 1.8

A list of features ported from DOSBox Staging (maintainers: dreamer and kcgen):

* AUTOTYPE command for scripted keyboard entry (improved since then by Wengier)
* LS command (heavily improved since then by Wengier)
* Basic support for VMware mouse protocol and CuteMouse wheel API
* Support for phonebook and ENET reliable UDP for modem emulation
* Support for changing key bindings in runtime (improved since then by Wengier)

A list of features ported from DOSVAX (maintainer: akm) and DOSVAXJ3 (maintainer: nanshiki):

* Support for JEGA/AX machine type (imported from DOSVAX and DOSVAXJ3, cleaned up with some improvements by Wengier)
* Support for DCGA and Toshiba J-3100 system type (imported by nanshiki and Wengier from DOSVAXJ3)
* Support for DOS/V service (imported from DOSVAXJ3 and then extended by Wengier, including support for non-Japanese DOS/V modes)
* Support for system input methods (IMEs) in Windows/Linux SDL1 builds (SDL-IM-plus by the DOSVAXJ3 maintainer, cleaned up and improved by Wengier)

A list of features ported from vDosPlus (maintainer: Wengier):

* Long filename support (improved for FAT drives since then by Wengier and joncampbell123)
* TrueType font (TTF) output support (originally by Jos Schaars and heavily improved since then by Wengier)
* Improved support for automatic drive mounting (Windows; improved since then by Wengier)
* Support for clipboard copy and paste (improved since then by Wengier)
* Several shell improvements (improved since then by Wengier)

Features such as save & load states and Pentium MMX instructions were also ported from community contributions and have since been heavily improved by the DOSBox-X project.

The [DOSBox-X website](https://dosbox-x.com) was designed and maintained by Wengier.

The [DOSBox-X Wiki](https://dosbox-x.com/wiki) pages were created and updated by Wengier and rderooy.

This is an attempt to properly credit the other code and its sources below. It is **not** a complete list yet, and please feel free to revise and correct this list if there are errors.

NE2000 network card emulation (Bochs; LGPLv2+) src/hardware/ne2000.cpp

MT32 synthesizer (MUNT; LGPLv2.1+) src/libs/mt32/.cpp src/libs/mt32/.h

FluidSynth synthesizer (Tom Moebert; GPLv2+) src/libs/fluidsynth/.c src/libs/fluidsynth/.h

Framework-agnostic GUI toolkit (Jorg Walter; GPLv3+) src/libs/gui_tk/.cpp src/libs/gui_tk/.h

Porttalk library, to read/write I/O ports directly (Unknown source) src/libs/porttalk/.cpp src/libs/porttalk/.h

FLAC, MP3, WAV, and Vorbis libraries (David Reid, Kevin Croft, et al; GPLv2+) src/libs/decoders/mp3*.cpp src/libs/decoders/.c src/libs/decoders/.h

FreeDOS utilities as binary blobs (FreeDOS; no license) src/builtin/*.cpp

NukedOPL OPL3 emulation (Alexey Khokholov; GPLv2+) src/hardware/nukedopl.cpp

OPL emulation based on Ken Silverman OPL2 emulation (LGPLv2.1+) src/hardware/opl.cpp

MOS6581 SID emulation (GPLv2+) src/hardware/reSID/.cpp src/hardware/reSID/.h

SN76496 emulation (MAME project; GPLv2+) src/hardware/sn76496.h src/hardware/tandy_sound.cpp

3dfx Voodoo Graphics SST-1/2 emulation (Aaron Giles; BSD 3-clause) src/hardware/voodoo_emu.cpp

HQ2X and HQ3X render scaler (ScummVM, Maxim Stepin; GPLv2+) src/gui/render_templates_hq2x.h

PC-98 FM board emulation (Neko Project II; BSD 3-clause) src/hardware/snd_pc98/*

PC-98 GDC and LIO drawing support (Neko Project II; BSD 3-clause) src/hardware/vga_pc98_gdc_draw.cpp src/ints/pc98_lio.cpp

QCOW image support (Michael Greger; GPLv2+) src/ints/qcow2_disk.cpp

JEGA and DOS/V support (nanshiki, Wengier; GPLv2+) include/jfont.h src/ints/int_dosv.cpp

PhysFS archive support (Ryan Gordon; zlib licence) src/libs/physfs/*

Tiny File Dialogs (vareille; zlib licence) src/libs/tinyfiledialogs/*

MAME CHD support (Romain Tisserand; BSD 3-clause) src/libs/libchdr/*

Game Link IPC protocol (David Walters, ported by Jörg Walter; GPLv2+) src/gamelink/* src/output/output_gamelink*
