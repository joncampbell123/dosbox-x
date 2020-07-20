DOSBox-X Installation and Released Packages 
===========================================

DOSBox-X is a cross-platform DOS emulator based on DOSBox, with the eventual goal of being a complete DOS emulation package. New versions of DOSBox-X are released periodically, typically on the last day of a month or the first day of the next month. Since DOSBox-X is cross-platform, all major host operating systems are officially supported including Windows (XP or later), Linux (with X11), macOS (10.12 or later) and DOS operating systems.

The current version of DOSBox-X at this time is DOSBox-X 0.83.3, which was released on June 30, 2020. Pre-compiled Windows binaries (both 32-bit and 64-bit), Linux RPM packages (64-bit), macOS packages (64-bit) and the special HX-DOS packages (for real DOS environments) can be found in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page. You will find DOSBox-X versions that have been released so far (ZIP or RPM packages) and change logs for the releases. The Windows installer is also available (see below for details).

Once you get DOSBox-X installed and running, you probably want to look at the DOSBox-X user guide in the [DOSBox-X Wiki](https://github.com/joncampbell123/dosbox-x/wiki) for usage information.

Windows Packages (Portable or Installer)
----------------------------------------

For each DOSBox-X version there are usually three zip packages for the Windows platform, which contain the portable binaries built with Visual Studio, MinGW 32-bit and MinGW 64-bit. For example, in the case of DOSBox-X version 0.83.3, they correspond to the following respectively:

* dosbox-x-windows-20200630-163819-windows.zip
* dosbox-x-mingw-win32-20200630164800.zip
* dosbox-x-mingw-win64-20200630174234.zip

The Visual Studio builds are the default Windows builds to use, but they only run on Windows Vista and later (Windows 7, 8, and 10). You will need to use the MinGW builds if you are running Windows XP. You may also want to use one of the MinGW builds if you encounter specific problem(s) with the Visual Studio builds. In addition, while the SDL1 version is the default version, the SDL2 version may be prefered over the SDL1 version for certain features such as touchscreen input support.

You may want to use the all-in-one Windows installer packages instead to ease the installation. The Windows installers are especially recommended for new users. With the installer the installation process will be automated while allowing you to change the install path and the default build to run if you prefer (and the option to install all builds to subdirectories), so that you will be able to start DOSBox-X as soon as the installation ends. A quick start guide is also included in the package. The Windows installer for the latest official DOSBox-X version is available from: [DOSBox-X-Setup-Windows-latest.exe](https://github.com/Wengier/dosbox-x-wiki/raw/master/DOSBox-X-Setup-Windows-latest.exe)

If you prefer to use the zip packages as previously mentioned, please select one of the zip packages to download for your platform and unzip, then you will find various folders or subdirectories, which are some supported targets. For Visual Studio builds, these correspond to Win32, x64, ARM and ARM64 (either SDL1 or SDL2 version), which are the build platforms. For MinGW builds, the targets are plain MinGW SDL1 build (mingw), MinGW build for lower-end systems (mingw-lowend), MinGW SDL2 build (mingw-sdl2) and MinGW build with custom drawn menu (mingw-sdldraw). Go to a target folder for your platform and run dosbox-x.exe inside it, then DOSBox-X will be launched and ready to be used. Unlike the Windows installer version however, there is no documentation included in these packages, and you may not see all such packages for some DOSBox-X versions.

Linux Packages (RPM)
--------------------

RPM packages are officially released for the Linux operating system (64-bit, with X11), specifically for CentOS 7 ("el7") and CentOS 8 ("el8") platforms. In the case of DOSBox-X version 0.83.2, they correspond to the following:

* dosbox-x-0.83.2-0.el7.x86_64.rpm
* dosbox-x-0.83.2-0.el8.x86_64.rpm
* dosbox-x-debuginfo-0.83.2-0.el7.x86_64.rpm
* dosbox-x-debuginfo-0.83.2-0.el8.x86_64.rpm
* dosbox-x-debugsource-0.83.2-0.el8.x86_64.rpm

Pick a RPM package for your Linux platform and install. You may want to use the debug builds (the last three packages mentioned above) if you desire to do some debugging work when running DOSBox-X.

Note: You may not see all such packages for some DOSBox-X versions. For example, there are no CentOS 8 builds for DOSBox-X 0.83.3 because libcap-devel and FluidSynth are not available.

macOS and DOS Packages (Portable)
---------------------------------

Besides Windows and Linux packages, there are also packages for the macOS (64-bit) and DOS platforms. In the case of DOSBox-X version 0.83.3, the macOS package and the special HX-DOS package correspond to the following zip packages respectively:

* dosbox-x-macosx-x64-20200630161244.zip
* dosbox-x-mingw-hx-dos-20200701193641.zip

The macOS package requires macOS Sierra 10.12 or higher. Both SDL1 and SDL2 binaries (in .app formats) are provided in the macOS package.

The HX-DOS package allows you to run DOSBox-X in a real DOS system (MS-DOS or compatible) too with the help of the freely-available [HX DOS Extender](https://github.com/Baron-von-Riedesel/HX), which is already included in the recent HX-DOS release packages. Note however that not all features of DOSBox-X can be supported in this environment. See the README.TXT file inside the HX-DOS package for more information.

Note: You may not see such packages for some DOSBox-X versions. For example, these two packages are not available for DOSBox-X version 0.83.1.

Source Code Packages (zip or tar.gz)
------------------------------------

Full source code packages of DOSBox-X are also available in both zip and tar.gz formats. Both contain the full source code, but you probably want to download the source code in zip format if you are using Windows, and the source code in tar.gz format if you are using Linux. They correspond the following files in the case of DOSBox-X version 0.83.3:

* dosbox-x-v0.83.3.zip
* dosbox-x-v0.83.3.tar.gz

If you prefer you can compile DOSBox-X from the source code by yourself. The source code packages as listed in the Releases page contain the source code for that released version, and in this example the DOSBox-X 0.83.3 version. On the other hand, if you are looking for the latest source code of DOSBox-X (including the most recent development changes in the source code), you may want to use the source code in the repository instead. You can use either of them according to your needs, and the source code may be compiled to run on the above-mentioned platforms (Windows, Linux, macOS and DOS) and possibly other operating systems too. Please see the [DOSBox-X source code description](README.source-code-description) file for detailed instructions on building the DOSBox-X source code and further information of the source code.
