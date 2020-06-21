Credits
=======

Jonathan Campbell, the maintainer of DOSBox-X does not claim to have written all of the code in this project.

Some of the code is DOSBox SVN code in which some of the SVN commits made since 2011 were incorporated into DOSBox-X.

Some of the source code also came from DOSBox forks such as DOSBox Daum, DOSBox ECE, DOSBox-staging as well as
vDosPlus, with major works from contributors like Wengier and rderooy.

A list of features ported from DOSBox Daum:

* GUI menu bar (heavily improved since then)
* Some commands (PROMPT, MOUSE, VOL, DEVICE, etc)
* Basic support for automatic drive mount (Windows)
* Printer output
* NE2000 Ethernet
* MT-32 emulation (MUNT)
* Internal 3dfx voodoo chip emulation
* Some support for FluidSynth MIDI synthesizer
* improved PC Speaker emulation accuracy
* CGA with Monochrome Monitor Support
* Support for CPU types like Pentium MMX
* Features such as V-Sync, xBRZ scaler, overscan border and stereo-swap
* Various patches such as DBCS and font patch

A list of features ported from DOSBox ECE:

* Support for mapping mouse buttons to keyboard
* Improved support for FluidSynth MIDI synthesizer
* Updated Nuked OPL3 to 1.8

A list of features ported from DOSBox-staging:

* Support for FLAC, Opus, Vorbis, and MP3 CD-DA tracks
* AUTOTYPE command for scripted keyboard entry
* LS command (heavily improved since then by Wengier)
* Modem phonebook support
* Support for changing key bindings in runtime

A list of features ported from vDosPlus by Wengier:

* Long filename support (improved for FAT drives since then by Wengier and joncampbell123)
* Improved support for automatic drive mount (Windows)
* Support for clipboard copy and paste (Windows)
* Several shell improvements

Features such as save and load states are also recently ported from community contributions and have since been improved by the DOSBox-X project.

This is an attempt to properly credit the other code and its sources below. It is not yet complete, and feel free to revise and correct this list if there are errors.

NE2000 network card emulation (Bochs; LGPLv2+) src/hardware/ne2000.cpp

MT32 synthesizer (MUNT; LGPLv2.1+) src/mt32/.cpp src/mt32/.h

Framework-agnostic GUI toolkit (Jorg Walter; GPLv3+) src/libs/gui_tk/.cpp src/libs/gui_tk/.h

Porttalk library, to read/write I/O ports directly (Unknown source) src/libs/porttalk/.cpp src/libs/porttalk/.h

FreeDOS utilities as binary blobs (FreeDOS; no license) src/builtin/*.cpp

NukedOPL OPL3 emulation (Alexey Khokholov; GPLv2+) src/hardware/nukedopl.cpp

OPL emulation based on Ken Silverman OPL2 emulation (LGPLv2.1+) src/hardware/opl.cpp

MOS6581 SID emulation (GPLv2+) src/hardware/reSID/.cpp src/hardware/reSID/.h

SN76496 emulation (MAME project; GPLv2+) src/hardware/sn76496.h src/hardware/tandy_sound.cpp

3dfx Voodoo Graphics SST-1/2 emulation (Aaron Giles; BSD 3-clause) src/hardware/voodoo_emu.cpp

PC-98 FM board emulation (Neko Project II; BSD 3-clause) src/hardware/snd_pc98/*

QCOW image support (Michael Greger; GPLv2+) src/ints/qcow2_disk.cpp

HQ2X and HQ3X render scaler (ScummVM, Maxim Stepin; GPLv2+) src/gui/render_templates_hq2x.h
