---
title: About DOSBox-X
---

**DOSBox-X** is a DOS emulator derived from DOSBox that is designed to support **different types of DOS software and systems**, including serious DOS application usages (such as those requiring printing and SHARE functions) and East Asian systems (such as DOS/V and PC-98). It focuses on implementing **accurate emulations** (especially accurate reproduction of hardware behavior) and various ways to tweak and configure the DOS virtual machine, and also intends to be a great platform for emulating **DOS-based Windows** such as Windows 3.x and 9x/Me and software written for those versions of Windows. DOSBox-X implements various features needed by DOS programs and also to support the **[development of new DOS software](newdosdevelopment.html)** (in combination with [DOSLIB](https://github.com/joncampbell123/doslib)/[DOSLIB2](https://github.com/joncampbell123/doslib2) and [Hackipedia](http://hackipedia.org/)).

### Goals

* Implement DOS systems covering all pre-2000 DOS and Windows 9x based hardware scenarios, including all manner of hardware
* Implement configuration parameters to configure the DOS virtual machine, whatever take to get the DOS game or program to run
* Implement emulation accurate enough to help new DOS developments with confidence programs run properly on actual DOS systems
* Implement features needed by different types of DOS software and DOS-based Windows (Windows 3.x/9x) for correction operations
* Being a great platform for emulating Windows 3.x/9x/Me and software written for these Windows versions (with full acceleration)
* Improve user interface and try to make DOSBox-X work consistently on supported platforms (especially Windows, Linux, and macOS)
* Improve documentation and ensure stability of release versions with the help of CI builds, unit tests and other testing methods
* Along with DOSLIB, help with continued DOS developments by supporting and encouraging developers to write new DOS games/programs

### Non-goals

* Giving the best experience to DOS gamers who desire good performance, or being the fastest DOS emulator on x86 hardware
* Adopting changes that would clearly impair the emulation accuracy, or otherwise conflict with the project goals
* Implementing features not necessary by the emulator, if they cause major issues or code maintainability concerns
* Supporting Pentium 4 or higher CPU level emulation (unless desired by the DOSBox-X community in general)
* Emulation of PC hardware after 2000, when the updated "PC 2001" specifications were published in 2001
* Emulation of DOS systems other than IBM PC/XT/AT, AX, Amstrad, Tandy, PCjr, JEGA/AX, and PC-98 (with exceptions)
* Windows guest emulation, Windows Vista or later (DOSBox-X focuses mostly on DOS-based Windows like Win3.x/9x/Me)
* Acting as a standalone DOS operating system like FreeDOS, although you can run DOS images inside the emulator

### Other DOS emulation projects

We wholy recommend the following projects, as each has a different focus and features:

* [DOSBox](https://www.dosbox.com/) - The upstream project that DOSBox-X and other forks (such as DOSBox Staging) are based on.
* [DOSBox Staging](https://dosbox-staging.github.io) - Fork of the DOSBox project that focuses on ease of use, modern technology and best practices.
* [DOSBox Pure](https://github.com/schellingb/dosbox-pure) - Fork of the DOSBox project built for RetroArch/Libretro aiming for simplicity and ease of use.
* [PCem](https://pcem-emulator.co.uk/) - IBM PC emulator that specializes in running old operating systems and software that are designed for IBM PC compatibles.
* [dosemu2](https://github.com/dosemu2/dosemu2/) - Emulator for running DOS programs under Linux.
* [FreeDOS](https://www.freedos.org) - Open source DOS-compatible operating system for playing classic games, running legacy software and supporting embedded systems.
