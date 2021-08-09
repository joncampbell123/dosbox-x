DOSBox-X Installation and Released Packages
===========================================

DOSBox-X is a cross-platform DOS emulator based on DOSBox, with the eventual goal of being a complete DOS emulation package. New versions of DOSBox-X are released periodically, typically on the last day of a month or the first day of the next month. Since DOSBox-X is cross-platform, all major host operating systems are officially supported including Windows (XP or later), Linux (with X11), macOS (10.12 or later) and DOS operating systems.

The current version of DOSBox-X at this time is DOSBox-X 0.83.16, which was released on August 1, 2021. Pre-compiled Windows binaries (both 32-bit and 64-bit), Linux Flatpak and RPM packages (64-bit), macOS packages (64-bit) and the special HX-DOS packages (for real DOS environments) are officially available, as well as the Windows installers and source code packages. You will find DOSBox-X versions that have been released so far (ZIP or RPM packages) and change logs for these versions in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page.

See also: [DOSBox-X 0.83.16 Release Notes](https://dosbox-x.com/release-0.83.16.html)

For most packages there are both SDL1 and SDL2 builds of DOSBox-X, and most features are the same for both builds. While SDL1 builds may be the default one to use, you may want to try SDL2 builds if you want certain features specific to SDL2 builds (such as the raw mouse input option) or you encounter specific issue(s) with SDL1 builds (such as incorrect keys in some international keyboard layouts).

Once you get DOSBox-X installed and running, you probably want to look at the DOSBox-X user guide in the [DOSBox-X Wiki](https://dosbox-x.com/wiki) for usage information.

## Packages for Supported Platforms

- [Windows Packages (Installer or Portable)](#windows-packages-installer-or-portable)
- [Linux Packages (Flatpak or Fedora package)](#linux-packages-flatpak-or-fedora-package)
- [macOS Packages (Portable)](#macos-packages-portable)
- [DOS Packages (Portable)](#dos-packages-portable)
- [Source Code Packages (zip or tar.gz)](#source-code-packages-zip-or-targz)

## Windows Packages (Installer or Portable)

You probably want to use the all-in-one Windows installation packages for the ease of installation, which are especially recommended for new and non-expert users. With the installer the installation process will be automated while allowing you to change the install folder and the default build to run if you prefer (and the option to install all builds to subdirectories), so that you will be able to start DOSBox-X as soon as the installation ends. A quick start guide is also included in the package, and shell context menus can be automatically added for a fast launch of DOSBox-X from the Windows Explorer. The Windows installers for the current DOSBox-X version 0.83.16 are available from:

* [dosbox-x-win32-0.83.16-setup.exe](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-win32-0.83.16-setup.exe) (32-bit Windows builds; Windows XP or later)
* [dosbox-x-win64-0.83.16-setup.exe](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-win64-0.83.16-setup.exe) (64-bit Windows builds; Windows Vista or later)

Windows installers for the previous DOSBox-X versions are also available from:

* [dosbox-x-windows-0.83.15-setup.exe](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.15/dosbox-x-windows-0.83.15-setup.exe) (version 0.83.15)
* [dosbox-x-windows-0.83.14-setup.exe](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.14/dosbox-x-windows-0.83.14-setup.exe) (version 0.83.14)

If you see the message ```Windows Defender SmartScreen prevented an unrecognized app from starting``` when running an installer, you can solve it by clicking the link "More info" in the dialog and then "Run anyway".

You can easily upgrade from a previous version of DOSBox-X to the new version with the Windows installer. The Windows installer in fact offers an option to automatically upgrade the config file (dosbox-x.conf) to the new version format while keeping all the user-customized settings already made. When you select this (recommended), the config file will include all options of the latest DOSBox-X version and also will keep all the changes already done previously by the user.

Apart from the Windows installers, you can find six zip packages (three before 0.83.13) for each DOSBox-X version for the Windows platform in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page as an alternative way to install DOSBox-X. These zip files are portable packages containing binaries built with Visual Studio 2019 (Win32, Win64, ARM32, ARM64 respectively), MinGW (Win32 and Win64 respectively). For the current DOSBox-X version 0.83.16, these portable builds are separately available from:

* [dosbox-x-vsbuild-win32-20210801012924.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-vsbuild-win32-20210801012924.zip) (Visual Studio Win32 builds)
* [dosbox-x-vsbuild-win64-20210801012924.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-vsbuild-win64-20210801012924.zip) (Visual Studio Win64 builds)
* [dosbox-x-vsbuild-arm32-20210801012924.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-vsbuild-arm32-20210801012924.zip) (Visual Studio ARM32 builds)
* [dosbox-x-vsbuild-arm64-20210801012924.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-vsbuild-arm64-20210801012924.zip) (Visual Studio ARM64 builds)
* [dosbox-x-mingw-win32-20210801203638.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-mingw-win32-20210801203638.zip) (MinGW Win32 builds)
* [dosbox-x-mingw-win64-20210801221927.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-mingw-win64-20210801221927.zip) (MinGW Win64 builds)

The Visual Studio builds are the default Windows builds to use, but they may require Windows Vista and later (Windows 7, 8, and 10). On the other hand, the MinGW builds may run in Windows XP but will not have the debugger. You may also want to use one of the MinGW builds (plain, lowend, etc) if you encounter specific problem(s) with the Visual Studio builds, or you want the Slirp backend support for the NE2000 networking. In addition, while the SDL1 version is the default version, the SDL2 version may be prefered over the SDL1 version for certain features (particularly related to input handling) such as better international keyboard support.

If you prefer to use one of the portable packages, please select the zip package you want to download for your platform and unzip, then you will find various folders or subdirectories, which are some supported targets. For Visual Studio builds, these correspond to Win32 (x86), Win64 (x64), ARM32 and ARM64 (either SDL1 or SDL2 version), which are the build platforms. For MinGW builds, the targets are plain MinGW SDL1 build (mingw), MinGW SDL1 build for lower-end systems (mingw-lowend), MinGW SDL1 build with custom drawn menu (mingw-sdldraw), and MinGW SDL2 build (mingw-sdl2). Go to a target folder for your platform and run dosbox-x.exe inside it, then DOSBox-X will be launched and ready to be used. Unlike the Windows installer version however, there is no documentation included in these packages, and you may not see all such packages for some DOSBox-X versions.

## Linux Packages
DOSBox-X is available packaged in the following formats:

### Flatpak
Flatpak packages are officially released for the Linux operating system (with X11).
You can select one of these packages depending on your Linux system and your needs.
The Linux Fatpak package has the advantage of being supported by most or all Linux distributions, but it will run in a sandbox on your Linux system so that you may not able to access some system-wide resources.

For the current DOSBox-X version the Linux Flatpak is available from:

* [com.dosbox_x.DOSBox-X.flatpakref](https://dl.flathub.org/repo/appstream/com.dosbox_x.DOSBox-X.flatpakref)

You may need to install Flatpak support depending on your Linux distribution for the first time. Please see the [Quick Setup page](https://flatpak.org/setup/) for more information specific to your Linux platform. The DOSBox-X Flathub page is also available from [here](https://flathub.org/apps/details/com.dosbox_x.DOSBox-X).

Once Flatpak support is enabled on your Linux system you can install the DOSBox-X Flatpak with the following command:

``flatpak install flathub com.dosbox_x.DOSBox-X``

After it is installed, it can be run with:

``flatpak run com.dosbox_x.DOSBox-X``

By default some system-wide resources will not be accessible by any Flatpak package. But you can give the DOSBox-X Flatpak package additional access using the --filesystem option. For example, to give it access to the /mnt directory:

``flatpak run --filesystem=/mnt com.dosbox_x.DOSBox-X``

In addition, if an earlier DOSBox-X Flatpak is already installed in the system you can update it to the current version with the command:

``flatpak update com.dosbox_x.DOSBox-X``

Or just "flatpak update" to update all Flatpak packages.

### SnapCraft
You can find DOSBox-X on SnapCraft (https://snapcraft.io/dosbox-x), which maintains universal Linux packages for software including DOSBox-X.
But please note the DOSBox-X Linux packages on this website are built and updated by SnapCraft instead of the DOSBox-X Team.

### RPM Package
RPM is a packaging format used by a variety of Linux distributions.
The current DOSBox-X version is offered via Fedora Copr here:

* [Fedora Copr DOSBox-X page](https://copr.fedorainfracloud.org/coprs/rob72/DOSBox-X/)

This supports the current Fedora Linux versions, Red Hat Enterprise Linux (RHEL) 8 with EPEL and CentOS 8 with EPEL.

In addition, but this is not recommended, some RPM packages for older DOSBox-X version are available for CentOS in the DOSBox-X Github under Releases.
Simply pick the RPM package(s) for the version you want to use for your Linux platform and install.
On CentOS, RHEL or Fedora platforms, you can install an RPM package with a command line like this:

``sudo rpm -i <filename>.rpm``

Where ``<filename>`` is the main file name of the RPM package you wish to install. You may want to use the debug build if you desire to do some debugging work when running DOSBox-X. If there are missing dependencies for the rpm command, such as libpng and fluid-soundfont, then you will need to install them first. However, RPM packages are not natively supported by Linux distributions such as Debian, Ubuntu, or Linux Mint (although the ``alien`` command may sometimes help). In such case you probably want to use the Flatpak package, which works independent of your Linux distribution.

### AUR Package
DOSBox-X is packaged for archlinux (https://aur.archlinux.org/packages/dosbox-x/), and available in the standard arch repo.
But please note that this package is not built or updated by the DOSBox-X team.

## macOS Packages (Portable)

If you use macOS as your operating system, we also release portable packages for the macOS (64-bit) platform. For the current DOSBox-X version 0.83.16, the official macOS packages are available as zip packages:

For 64-bit Intel-based macOS:
* [dosbox-x-macosx-x86_64-20210801002330.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-macosx-x86_64-20210801002330.zip)

For 64-bit ARM-based macOS:
* [dosbox-x-macosx-arm64-20210801002330.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-macosx-arm64-20210801002330.zip)

The macOS package requires 64-bit Intel or ARM-based macOS operating system. It should run natively on the recent versions of macOS such as macOS Catalina (10.15) and Big Sur (11.0). Both SDL1 and SDL2 binaries (in .app format) are provided in the macOS package, in the directories named ```dosbox-x``` and ```dosbox-x-sdl2``` respectively inside the zip file. While the SDL1 version is the default version, the SDL2 version may be preferred over the SDL1 version for certain features (particularly related to input handling) such as touchscreen input support. You can select either SDL1 or SDL2 version according to your preference, or just run the SDL1 version if you are not sure.

There are two ways to run DOSBox-X in macOS, either from the Finder or from the command-line (Terminal):

* From the Finder, go to the directory where the macOS zip package is downloaded, you will see a folder name which is the same as the file name of the downloaded zip package. Inside this folder you will see ```dosbox-x``` (SDL1) and ```dosbox-x-sdl2``` (SDL2). Go to either one and click the program "dosbox-x" to start DOSBox-X. If you see a dialog asking you to select a folder, please select one which will then become your DOSBox-X working directory. You can choose to save this folder after you select one so that the folder selection dialog will no show up again next time, or let DOSBox-X show the folder selection dialog every time you run it from the Finder.

* From the Terminal, go to the directory where the macOS zip package is downloaded, you will see a folder name which is the same as the file name of the downloaded zip package. Starting from this folder, use ```cd``` command to go to the directory where the DOSBox-X executable is located. For SDL1 build, type ```cd dosbox-x/dosbox-x.app/Contents/MacOS```, and for SDL2 build, type ```cd dosbox-x-sdl2/dosbox-x.app/Contents/MacOS```. Run DOSBox-X with ```./DosBox``` and you will see the DOSBox-X window.

If you see the message ```"dosbox-x" is damaged and can't be opened``` when trying to run DOSBox-X, you should be able to solve the problem by running the following command once in the Terminal and you are in the directory in which the macOS zip package is extracted (where you can find two folders including ```dosbox-x``` and ```dosbox-x-sdl2```):

``xattr -cr dosbox-x/dosbox-x.app dosbox-x-sdl2/dosbox-x.app``

As of DOSBox-X version 0.83.16, the regular macOS builds listed above will run on macOS Catalina (10.15) or later; they include features such as Slirp, FluidSynth and FFmpeg by default and bundle dylib libraries needed for such features.

If you have an older macOS version such as macOS High Sierra (10.13) and Mojave (10.14), or you use a newer macOS version but do not need any additional features provided by external libraries such as libslirp or libfluidsynth, you can use the alternative macOS builds below instead, which include both SDL1 and SDL2 versions of the DOSBox-X (named ``dosbox-x-sdl1`` and ``dosbox-x-sdl2`` respectively in the package). These are intended to be builds for "low-end" systems with no need for external libraries to run them in your macOS:

* [dosbox-x-macosx-0.83.16-high-sierra.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-macosx-0.83.16-high-sierra.zip)

If you see the message ```"dosbox-x" is damaged and can't be opened``` when trying to run the above builds, you should be able to solve the problem by executing the following command (once) in the Terminal and you are in the directory in which the zip package is extracted (where you can find two files including ```dosbox-x-sdl1``` and ```dosbox-x-sdl2```):

``xattr -cr .``

Then you should be able to run the binary ``dosbox-x-sdl1`` or ``dosbox-x-sdl2`` normally.

For the previous version 0.83.10, you can download the signed macOS package for both 64-bit Intel-based and 64-bit ARM-based macOS:

* [dosbox-x-macosx-0.83.10-bin64.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.10/dosbox-x-macosx-0.83.10-bin64.zip)

macOS versions earlier than 10.12 (Sierra) are not officially supported. If you use an old version such as OS X Lion (10.7) or OS X Yosemite (10.10), then you may try to build and run DOSBox-X yourself with the help of [MacPorts](https://www.macports.org/). Furthermore, you may not see official macOS packages for some DOSBox-X versions. For example, no official macOS package is available for DOSBox-X version 0.83.1.

## DOS Packages (Portable)

Besides Windows, Linux and macOS packages, there are also packages released for the DOS operating system. Yes, DOSBox-X can officially run on DOS systems as well, as some DOS users seem to prefer to run DOS applications and games through a DOS emulator. With DOSBox-X running in DOS you are able to emulate another DOS system with a different PC configuration (such as different machine types, video and sound cards etc) that works better for the purpose of the users. But please note that due to the limitations of this environment not all features of DOSBox-X that are available in other platforms can be supported in the DOS version.

For the current DOSBox-X version 0.83.16, the official DOS version is available in the following zip package:

* [dosbox-x-mingw-hx-dos-20210801212637.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-mingw-hx-dos-20210801212637.zip)

The HX-DOS package allows you to run DOSBox-X in a real DOS system (MS-DOS 5.0+ or compatible) with the help of the freely-available [HX DOS Extender](https://github.com/Baron-von-Riedesel/HX), which is already included in the recent DOS release packages. Once you unzip the package you can directly type ```DOSBOX-X``` to run in DOS. See the README.TXT file inside the DOS package for more information.

Alternatively, you can run DOSBox-X from a DOS environment with the help of the free [LOADLIN](https://docstore.mik.ua/orelly/linux/lnut/ch04_03.htm) program. The alternative DOS package for DOSBox-X 0.83.16 using this approach is available from:

* [dosbox-x-dos-0.83.16-loadlin.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.16/dosbox-x-dos-0.83.16-loadlin.zip)

With this LOADLIN DOS package you can run DOSBox-X right from DOSBox-X's DOS shell. Start the outside DOSBox-X with the setting ```memsize=127``` and ```cputype=pentium``` (perhaps also ```fullscreen=true``` and/or ```autolock=true```). Go to the directory where the files are extracted and type ```DOSBOX-X```. Then just wait for DOSBox-X to be automatically loaded within DOSBox-X.

Note: You may not see DOS packages for some DOSBox-X versions. For example, the official DOS package is not available for DOSBox-X version 0.83.1.

## Source Code Packages (zip or tar.gz)

Full source code packages of DOSBox-X are also available in both zip and tar.gz formats. Both contain the full source code, but you probably want to download the source code in zip format if you are using Windows, and the source code in tar.gz format if you are using Linux. For the current DOSBox-X version 0.83.16, the source code packages are:

* [dosbox-x-v0.83.16.zip](https://github.com/joncampbell123/dosbox-x/archive/refs/tags/dosbox-x-v0.83.16.zip)
* [dosbox-x-v0.83.16.tar.gz](https://github.com/joncampbell123/dosbox-x/archive/refs/tags/dosbox-x-v0.83.16.tar.gz)

If you prefer you can compile DOSBox-X from the source code by yourself. The source code packages as listed in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page contain the source code for that released version, and in this example the current DOSBox-X 0.83.16 version. On the other hand, if you are looking for the latest source code of DOSBox-X (including the most recent development changes in the source code), you may want to use the source code in the repository instead, or you can browse the latest source code using [Doxygen](https://dosbox-x.com/doxygen/html/index.html).

You could use either the released source code package or the latest source code according to your needs, and the source code may be compiled to run on the above-mentioned platforms (Windows, Linux, macOS and DOS) and possibly other operating systems too. Please see the [BUILD](BUILD.md) page for detailed instructions on building the DOSBox-X source code. Further descriptions of the source code can be found in the [DOSBox-X source code description](README.source-code-description) file.
