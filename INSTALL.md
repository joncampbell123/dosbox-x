DOSBox-X Installation and Released Packages 
===========================================

DOSBox-X is a cross-platform DOS emulator based on DOSBox, with the eventual goal of being a complete DOS emulation package. New versions of DOSBox-X are released periodically, typically on the last day of a month or the first day of the next month. Since DOSBox-X is cross-platform, all major host operating systems are officially supported including Windows (XP or later), Linux (with X11), macOS (10.12 or later) and DOS operating systems.

The current version of DOSBox-X at this time is DOSBox-X 0.83.12, which was released on April 1, 2021. Pre-compiled Windows binaries (both 32-bit and 64-bit), Linux RPM and Flatpak packages (64-bit), macOS packages (64-bit) and the special HX-DOS packages (for real DOS environments) are officially available, as well as the Windows installers and source code packages. You will find DOSBox-X versions that have been released so far (ZIP or RPM packages) and change logs for these versions in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page.

See also: [DOSBox-X 0.83.12 Release Notes](https://dosbox-x.com/release-0.83.12.html)

Once you get DOSBox-X installed and running, you probably want to look at the DOSBox-X user guide in the [DOSBox-X Wiki](https://dosbox-x.com/wiki) for usage information.

## Packages for Supported Platforms

- [Windows Packages (Installer or Portable)](#windows-packages-installer-or-portable)
- [Linux Packages (RPM or Flatpak)](#linux-packages-rpm-or-flatpak)
- [macOS Packages (Portable)](#macos-packages-portable)
- [DOS Packages (Portable)](#dos-packages-portable)
- [Source Code Packages (zip or tar.gz)](#source-code-packages-zip-or-targz)

## Windows Packages (Installer or Portable)

You probably want to use the all-in-one Windows installation packages for the ease of installation, which are especially recommended for new and non-expert users. With the installer the installation process will be automated while allowing you to change the install folder and the default build to run if you prefer (and the option to install all builds to subdirectories), so that you will be able to start DOSBox-X as soon as the installation ends. A quick start guide is also included in the package, and shell context menus can be automatically added for a fast launch of DOSBox-X from the Windows Explorer. The Windows installer for the current DOSBox-X version 0.83.12 is available from:

* [dosbox-x-windows-0.83.12-setup.exe](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.12/dosbox-x-windows-0.83.12-setup.exe)

Windows installers for the previous DOSBox-X versions are also available from:

* [dosbox-x-windows-0.83.11-setup.exe](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.11/dosbox-x-windows-0.83.11-setup.exe)
* [dosbox-x-windows-0.83.10-setup.exe](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.10/dosbox-x-windows-0.83.10-setup.exe)

You can easily upgrade from a previous version of DOSBox-X to the new version with the Windows installer. The Windows installer in fact offers an option to automatically upgrade the config file (dosbox-x.conf) to the new version format while keeping all the user-customized settings already made. When you select this (recommended), the config file will include all options of the latest DOSBox-X version and also will keep all the changes already done previously by the user.

Apart from the Windows installers, you can usually find three zip packages for each DOSBox-X version for the Windows platform in the Releases page as an alternative way to install DOSBox-X. These zip files are portable packages containing binaries built with Visual Studio 2019, MinGW 32-bit and MinGW 64-bit (or mingw-w64). For the current DOSBox-X version 0.83.12, these portable builds are separately available from:

* [dosbox-x-vsbuild-win-20210401015807.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.12/dosbox-x-vsbuild-win-20210401015807.zip)
* [dosbox-x-mingw-win32-20210401015947.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.12/dosbox-x-mingw-win32-20210401015947.zip)
* [dosbox-x-mingw-win64-20210401030652.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.12/dosbox-x-mingw-win64-20210401030652.zip)

The Visual Studio builds are the default Windows builds to use, but they only run on Windows Vista and later (Windows 7, 8, and 10). The MinGW builds will be required if you are running Windows XP. You may also want to use one of the MinGW builds (plain, lowend, etc) if you encounter specific problem(s) with the Visual Studio builds. In addition, while the SDL1 version is the default version, the SDL2 version may be prefered over the SDL1 version for certain features (particularly related to input handling) such as touchscreen input support.

If you prefer to use one of the portable packages, please select the zip package you want to download for your platform and unzip, then you will find various folders or subdirectories, which are some supported targets. For Visual Studio builds, these correspond to Win32, x64, ARM and ARM64 (either SDL1 or SDL2 version), which are the build platforms. For MinGW builds, the targets are plain MinGW SDL1 build (mingw), MinGW build for lower-end systems (mingw-lowend), MinGW SDL2 build (mingw-sdl2) and MinGW build with custom drawn menu (mingw-sdldraw). Go to a target folder for your platform and run dosbox-x.exe inside it, then DOSBox-X will be launched and ready to be used. Unlike the Windows installer version however, there is no documentation included in these packages, and you may not see all such packages for some DOSBox-X versions.

## Linux Packages (Flatpak or RPM)

Both Flatpak and RPM packages are officially released for the Linux operating system (with X11). You can select one of these packages depending on your Linux system and your needs. The Linux Fatpak package has the advantage of being supported by most or all Linux distributions, but it will run in a sandbox on your Linux system so that you may not able to access some system-wide resources.

Flatpak packages are standalone applications independent of Linux distributions. For the current DOSBox-X version 0.83.12 the Linux Flatpak is available from:

* [com.dosbox_x.DOSBox-X.flatpakref](https://dl.flathub.org/repo/appstream/com.dosbox_x.DOSBox-X.flatpakref)

You may need to install Flatpak support depending on your Linux distribution for the first time. Please see the [Quick Setup page](https://flatpak.org/setup/) for more information specific to your Linux platform. The DOSBox-X Flathub page is also available from [here](https://flathub.org/apps/details/com.dosbox_x.DOSBox-X).

Once Flatpak support is enabled in your Linux system you can install the DOSBox-X Flatpak with the following command:

``flatpak install flathub com.dosbox_x.DOSBox-X``

After it is installed, it can be run with:

``flatpak run com.dosbox_x.DOSBox-X``

By default some system-wide resources will not be accessible by any Flatpak package. But you can give the DOSBox-X Flatpak package additional access using the --filesystem option. For example, to give it access to the /mnt directory:

``flatpak run --filesystem=/mnt com.dosbox_x.DOSBox-X``

In addition, if an earlier DOSBox-X Flatpak is already installed in the system you can update it to the current version with the command:

``flatpak update com.dosbox_x.DOSBox-X``

Or just "flatpak update" to update all Flatpak packages.

RPM packages are not available for the current version 0.83.12; the previous version that they were available was version 0.83.9:

The standard RPM package is available for 64-bit Linux, specifically CentOS 7 / RHEL 7 ("el7") platforms:

* [dosbox-x-0.83.9-0.el7.x86_64.rpm](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.9/dosbox-x-0.83.9-0.el7.x86_64.rpm)

The RPM package with debugging information:

* [dosbox-x-debuginfo-0.83.9-0.el7.x86_64.rpm](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.9/dosbox-x-debuginfo-0.83.9-0.el7.x86_64.rpm)

Pick a RPM package of the version you want to use for your Linux platform and install. On CentOS, RHEL or Fedora platforms, you can install a RPM package with a command line like this:

``sudo rpm -i <filename>.rpm``

Where ``<filename>`` is the main file name of the RPM package you wish to install. You may want to use the debug build if you desire to do some debugging work when running DOSBox-X. If there are missing dependencies for the rpm command, such as libpng and fluid-soundfont, then you will need to install them first. However, RPM packages are not natively supported by Linux distributions such as Debian, Ubuntu, or Linux Mint (although the ``alien`` command may sometimes help). In such case you probably want to use the Flatpak package, which works independent of your Linux distribution.

Moreover, you can find DOSBox-X on SnapCraft (https://snapcraft.io/dosbox-x), which maintains universal Linux packages for software including DOSBox-X. Please note the DOSBox-X Linux packages on this website are built and updated by SnapCraft instead of the DOSBox-X Team.

## macOS Packages (Portable)

If you use macOS as your operating system, we also release portable packages for the macOS (64-bit) platform. For the current DOSBox-X version 0.83.12, the official macOS packages are available as zip packages:

For 64-bit Intel-based macOS:
* [dosbox-x-macosx-x86_64-20210401031910.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.12/dosbox-x-macosx-x86_64-20210401031910.zip)

For 64-bit ARM-based macOS:
* [dosbox-x-macosx-arm64-20210401114115.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.12/dosbox-x-macosx-arm64-20210401114115.zip)

The macOS package requires 64-bit Intel or ARM-based macOS operating system. It should run natively on the recent versions of macOS such as macOS Catalina (10.15) and Big Sur (11.0). Both SDL1 and SDL2 binaries (in .app format) are provided in the macOS package, in the directories named ```dosbox-x``` and ```dosbox-x-sdl2``` respectively inside the zip file. You can select either SDL1 or SDL2 version according to your preference.

If you have an older macOS version such as macOS High Sierra (10.13), you may need to run the DOSBox-X binary (SDL1 or SDL2) in the Intel-based macOS package from the command line rather than from the Finder. In such case go to the directory ```dosbox-x/dosbox-x.app/Contents/MacOS``` (SDL1) or ```dosbox-x-sdl2/dosbox-x.app/Contents/MacOS``` (SDL2) after the package is extracted and then run the DosBox binary directly.

For the previous version 0.83.10, you can download the signed macOS package for both 64-bit Intel-based and 64-bit ARM-based macOS:

* [dosbox-x-macosx-0.83.10-bin64.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.10/dosbox-x-macosx-0.83.10-bin64.zip)

macOS versions earlier than 10.12 (Sierra) are not officially supported. If you use an old version such as OS X Lion (10.7) or OS X Yosemite (10.10), then you may try to build and run DOSBox-X yourself with the help of [MacPorts](https://www.macports.org/). Furthermore, you may not see official macOS packages for some DOSBox-X versions. For example, no official macOS package is available for DOSBox-X version 0.83.1.

## DOS Packages (Portable)

Besides Windows, Linux and macOS packages, there are also packages released for the DOS operating system. Yes, DOSBox-X can officially run on DOS systems as well, as some DOS users seem to prefer to run DOS applications and games through a DOS emulator. With DOSBox-X running in DOS you are able to emulate another DOS system with a different PC configuration (such as different machine types, video and sound cards etc) that works better for the purpose of the users. But please note that due to the limitations of this environment not all features of DOSBox-X that are available in other platforms can be supported in the DOS version.

For the current DOSBox-X version 0.83.12, the official DOS version is available in the following zip package:

* [dosbox-x-mingw-hx-dos-20210401001301.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.12/dosbox-x-mingw-hx-dos-20210401001301.zip)

The HX-DOS package allows you to run DOSBox-X in a real DOS system (MS-DOS 5.0+ or compatible) with the help of the freely-available [HX DOS Extender](https://github.com/Baron-von-Riedesel/HX), which is already included in the recent DOS release packages. Once you unzip the package you can directly type ```DOSBOX-X``` to run in DOS. See the README.TXT file inside the DOS package for more information.

Alternatively, you can run DOSBox-X from a DOS environment with the help of the free [LOADLIN](https://docstore.mik.ua/orelly/linux/lnut/ch04_03.htm) program. The alternative DOS package for DOSBox-X 0.83.11 using this approach is available from:

* [dosbox-x-dos-0.83.11-loadlin.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.11/dosbox-x-dos-0.83.11-loadlin.zip)

With this LOADLIN DOS package you can even run DOSBox-X right from DOSBox-X's DOS shell. Start the outside DOSBox-X with the setting ```memsize=127``` and ```cputype=pentium``` (perhaps also ```fullscreen=true```). Go to the directory where the files are extracted and type ```DOSBOX-X```. Then just wait for DOSBox-X to be automatically loaded within DOSBox-X.

Note: You may not see DOS packages for some DOSBox-X versions. For example, the official DOS package is not available for DOSBox-X version 0.83.1.

## Source Code Packages (zip or tar.gz)

Full source code packages of DOSBox-X are also available in both zip and tar.gz formats. Both contain the full source code, but you probably want to download the source code in zip format if you are using Windows, and the source code in tar.gz format if you are using Linux. For the current DOSBox-X version 0.83.12, the source code packages are:

* [dosbox-x-v0.83.12.zip](https://github.com/joncampbell123/dosbox-x/archive/refs/tags/dosbox-x-v0.83.12.zip)
* [dosbox-x-v0.83.12.tar.gz](https://github.com/joncampbell123/dosbox-x/archive/refs/tags/dosbox-x-v0.83.12.tar.gz)

If you prefer you can compile DOSBox-X from the source code by yourself. The source code packages as listed in the Releases page contain the source code for that released version, and in this example the current DOSBox-X 0.83.12 version. On the other hand, if you are looking for the latest source code of DOSBox-X (including the most recent development changes in the source code), you may want to use the source code in the repository instead, or you can browse the latest source code using [Doxygen](https://dosbox-x.com/doxygen/html/index.html).

You could use either the released source code package or the latest source code according to your needs, and the source code may be compiled to run on the above-mentioned platforms (Windows, Linux, macOS and DOS) and possibly other operating systems too. Please see the [DOSBox-X source code description](README.source-code-description) file for detailed instructions on building the DOSBox-X source code and further information of the source code.
