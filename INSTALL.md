Released Packages and Installation of DOSBox-X
==============================================

DOSBox-X is a cross-platform DOS emulator based on DOSBox. New versions of DOSBox-X are released periodically, typically on the last day of a month or the first day of the next month. Since DOSBox-X is cross-platform, all major host operating systems are officially supported including Windows (XP or later), Linux (with X11), macOS (10.12 or later) and DOS operating systems.

The current version of DOSBox-X at this time is DOSBox-X 0.83.3, which was released on June 30, 2020. Pre-compiled Windows binaries (both 32-bit and 64-bit), Linux RPM packages (64-bit), macOS packages (64-bit) and the special HX-DOS packages (for real DOS environments) can be found in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page. You will find DOSBox-X versions that have been released so far (ZIP or RPM packages) and change logs for the releases. The Windows installer is also available (see below for details).

Windows Packages
----------------

For each DOSBox-X version there are generally three zip packages for the Windows platform, built with Visual Studio, MinGW 32-bit and MinGW 64-bit respectively. For example, in the case of DOSBox-X version 0.83.2, they correspond to the following:

* dosbox-x-windows-20200531-220809-windows.zip
* dosbox-x-mingw-win32-20200531151931.zip
* dosbox-x-mingw-win64-20200531160500.zip

The Visual Studio builds are the default Windows builds to use, but they only run on Windows Vista and later (Windows 7, 8, and 10). You will need to use the MinGW builds if you are running Windows XP. You may also want to use one of the MinGW builds if you encounter specific problem(s) with the Visual Studio builds.

You may want to use the all-in-one Windows installer packages instead in order to ease the installation. The Windows installers are especially recommended for new users. With the installer the installation process will be automated while allowing you to change the install path and the default build to run if you prefer (and the option to install all builds to subdirectories), so that you will be able to start DOSBox-X as soon as the installation ends. A quick start guide is also included in the package. Windows installer for the latest official DOSBox-X version is available from: [DOSBox-X-Setup-Windows-latest.exe](https://github.com/Wengier/dosbox-x-wiki/raw/master/DOSBox-X-Setup-Windows-latest.exe)

If you prefer to use the zip packages mentioned above, please select one of the zip packages to download for your platform and unzip, then you will find various folders, which are some supported targets. For Visual Studio builds, these correspond to Win32, x64, ARM and ARM64 (either SDL1 or SDL2), which are the build platforms. For MinGW builds, the targets are plain MinGW (mingw), MinGW for lowend systems (mingw-lowend), MinGW SDL2 (mingw-sdl2) and MinGW with custom drawn menu (mingw-sdldraw). Go to a target folder for your platform and run dosbox-x.exe inside it, then DOSBox-X will be launched and ready to be used. Unlike the Windows installer version, there is no documentation included in these packages, and you may not see all such packages for some DOSBox-X versions.

Linux Packages
--------------

RPM packages are released for the Linux operating system (64-bit, with X11), specifically for Centos 7 ("el7") and Centos 8 ("el8") platforms. In the case of DOSBox-X version 0.83.2, they correspond to the following:

* dosbox-x-0.83.2-0.el7.x86_64.rpm
* dosbox-x-0.83.2-0.el8.x86_64.rpm
* dosbox-x-debuginfo-0.83.2-0.el7.x86_64.rpm
* dosbox-x-debuginfo-0.83.2-0.el8.x86_64.rpm
* dosbox-x-debugsource-0.83.2-0.el8.x86_64.rpm

Pick a RPM package for your Linux platform and install. You may want to use the debug builds (the last three packages mentioned above) if you desire to do some debugging work.

Note: You may not see all such packages for some DOSBox-X versions. For example, there are no Centos 8 builds for DOSBox-X 0.83.3 because libcap-devel and fluidsynth are not available.

macOS and DOS Packages
----------------------

Besides Windows and Linux packages, there are also packages for the macOS (64-bit) and DOS platforms. In the case of DOSBox-X version 0.83.2, the macOS package and the special HX-DOS package correspond to the following zip packages respectively:

* dosbox-x-macosx-x64-20200531151047.zip
* dosbox-x-mingw-hx-dos-20200531220949.zip

The macOS package requires macOS Sierra 10.12 or higher.

The HX-DOS package allows you to run DOSBox-X in a real DOS system (MS-DOS or compatible) too with the help of the freely-available [HX DOS Extender](https://github.com/Baron-von-Riedesel/HX), which is already included in the recent HX-DOS release packages. Note however that not all features of DOSBox-X can be supported in this environment. See the README.TXT file inside the package for more information.

Note: You may not see such packages for some DOSBox-X versions. For example, these two packages are not available for DOSBox-X version 0.83.1.

Source Code Packages
----------------------

Full source code packages of DOSBox-X are also available in both zip and tar.gz formats. They correspond the following files in the case of DOSBox-X version 0.83.2:

* dosbox-x-v0.83.2.zip
* dosbox-x-v0.83.2.tar.gz

If you prefer you can compile DOSBox-X from the source code by yourself. The source code may be compiled to run on the above-mentioned platforms and possibly other operating systems too. Please see the [DOSBox-X source code description](README.source-code-description) file for detailed information on building the DOSBox-X source code and further details of the source code.
