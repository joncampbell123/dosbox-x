DOSBox-X Installation and Released Packages 
===========================================

DOSBox-X is a cross-platform DOS emulator based on DOSBox, with the eventual goal of being a complete DOS emulation package. New versions of DOSBox-X are released periodically, typically on the last day of a month or the first day of the next month. Since DOSBox-X is cross-platform, all major host operating systems are officially supported including Windows (XP or later), Linux (with X11), macOS (10.12 or later) and DOS operating systems.

The current version of DOSBox-X at this time is DOSBox-X 0.83.5, which was released on September 1, 2020. Pre-compiled Windows binaries (both 32-bit and 64-bit), Linux RPM packages (64-bit), macOS packages (64-bit) and the special HX-DOS packages (for real DOS environments) are officially available, as well as the source code packages. You will find DOSBox-X versions that have been released so far (ZIP or RPM packages) and change logs for these versions in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page. The Windows installers are also available (see the next section for details).

See also: [DOSBox-X 0.83.5 Release Notes](http://dosbox-x.com/release-0.83.5.html)

Once you get DOSBox-X installed and running, you probably want to look at the DOSBox-X user guide in the [DOSBox-X Wiki](https://github.com/joncampbell123/dosbox-x/wiki) for usage information.

Windows Packages (Installer or Portable)
----------------------------------------

You probably want to use the all-in-one Windows installation packages for the ease of installation, which are especially recommended for new and non-expert users. With the installer the installation process will be automated while allowing you to change the install folder and the default build to run if you prefer (and the option to install all builds to subdirectories), so that you will be able to start DOSBox-X as soon as the installation ends. A quick start guide is also included in the package, and shell context menus can be automatically added for a fast launch of DOSBox-X from the Windows Explorer. The Windows installer for the current DOSBox-X version 0.83.5 is available from:

* [dosbox-x-windows-0.83.5-setup.exe](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.5/dosbox-x-windows-0.83.5-setup.exe)

The Windows installers for recent DOSBox-X versions are also available from:

* [dosbox-x-windows-0.83.4-setup.exe](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.4/dosbox-x-windows-0.83.4-setup.exe)
* [dosbox-x-windows-0.83.3-setup.exe](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.3/dosbox-x-windows-0.83.3-setup.exe)

Apart from the Windows installers, you can usually find three zip packages for each DOSBox-X version for the Windows platform in the Releases page as an alternative way to install DOSBox-X. These zip files are portable packages containing binaries built with Visual Studio 2019, MinGW 32-bit and MinGW 64-bit (or mingw-w64). For the current DOSBox-X version 0.83.5, the portable MinGW builds are officially available:

* [dosbox-x-mingw-win32-20200901082112.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.5/dosbox-x-mingw-win32-20200901082112.zip)
* [dosbox-x-mingw-win64-20200901093925.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.5/dosbox-x-mingw-win64-20200901093925.zip)

The Visual Studio builds are available as zip packages for most versions; they are the default Windows builds to use, but they only run on Windows Vista and later (Windows 7, 8, and 10). The MinGW builds will be required if you are running Windows XP. You may also want to use one of the MinGW builds (plain, lowend, etc) if you encounter specific problem(s) with the Visual Studio builds. In addition, while the SDL1 version is the default version, the SDL2 version may be prefered over the SDL1 version for certain features (particularly related to input handling) such as touchscreen input support.

If you prefer to use one of the portable packages, please select the zip package you want to download for your platform and unzip, then you will find various folders or subdirectories, which are some supported targets. For Visual Studio builds, these correspond to Win32, x64, ARM and ARM64 (either SDL1 or SDL2 version), which are the build platforms. For MinGW builds, the targets are plain MinGW SDL1 build (mingw), MinGW build for lower-end systems (mingw-lowend), MinGW SDL2 build (mingw-sdl2) and MinGW build with custom drawn menu (mingw-sdldraw). Go to a target folder for your platform and run dosbox-x.exe inside it, then DOSBox-X will be launched and ready to be used. Unlike the Windows installer version however, there is no documentation included in these packages, and you may not see all such packages for some DOSBox-X versions.

Linux Packages (RPM)
--------------------

RPM packages are officially released for the Linux operating system (64-bit, with X11), specifically for CentOS 7 / RHEL 7 ("el7") and CentOS 8 / RHEL 8 ("el8") platforms. There are usually packages for both CentOS 7 and CentOS 8 platforms, but for the current DOSBox-X version 0.83.5 they are not officially available. For the previous version 0.83.4 the following CentOS 7 RPM packages are available (no CentOS 8 RPM packages):

* [dosbox-x-0.83.4-0.el7.x86_64.rpm](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.4/dosbox-x-0.83.4-0.el7.x86_64.rpm)
* [dosbox-x-debuginfo-0.83.4-0.el7.x86_64.rpm](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.4/dosbox-x-debuginfo-0.83.4-0.el7.x86_64.rpm)

The last DOSBox-X version that provided official packages for both CentOS 7 and CentOS 8 platforms was version 0.83.2, which included the following Linux RPM packages:

* [dosbox-x-0.83.2-0.el7.x86_64.rpm](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.2/dosbox-x-0.83.2-0.el7.x86_64.rpm)
* [dosbox-x-0.83.2-0.el8.x86_64.rpm](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.2/dosbox-x-0.83.2-0.el8.x86_64.rpm)
* [dosbox-x-debuginfo-0.83.2-0.el7.x86_64.rpm](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.2/dosbox-x-debuginfo-0.83.2-0.el7.x86_64.rpm)
* [dosbox-x-debuginfo-0.83.2-0.el8.x86_64.rpm](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.2/dosbox-x-debuginfo-0.83.2-0.el8.x86_64.rpm)
* [dosbox-x-debugsource-0.83.2-0.el8.x86_64.rpm](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.2/dosbox-x-debugsource-0.83.2-0.el8.x86_64.rpm)

Pick a RPM package of the version you want to use for your Linux platform and install. On CentOS, RHEL or Fedora platforms, you can install a RPM package with a command line like this:

``sudo rpm -i <filename>.rpm``

Where ``<filename>`` is the main file name of the RPM package you wish to install. You may want to use the debug builds (the last three packages in the above example) if you desire to do some debugging work when running DOSBox-X. If there are missing dependencies for the rpm command, such as libpng and fluid-soundfont, then you will need to install them first.

If you use a Linux platform that only supports DEB packages, such as Debian, Ubuntu, or Linux Mint, then you can convert the RPM package you want to install to DEB format by using the ```alien``` command-line tool, e.g. ```sudo alien <filename>.rpm```, which will generate the DEB package for use with your Linux platform from the RPM package. You will need to install the ```alien``` tool first if it is not yet installed on your Linux system.

macOS and DOS Packages (Portable)
---------------------------------

Besides Windows and Linux packages, there are also packages for the macOS (64-bit) and DOS platforms. For the current DOSBox-X version 0.83.5, the macOS package and the special HX-DOS package are the following zip packages respectively:

* [dosbox-x-macosx-x64-20200901011555.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.5/dosbox-x-macosx-x64-20200901011555.zip)
* [dosbox-x-mingw-hx-dos-20200901082024.zip](https://github.com/joncampbell123/dosbox-x/releases/download/dosbox-x-v0.83.5/dosbox-x-mingw-hx-dos-20200901082024.zip)

The macOS package requires macOS Sierra 10.12 or higher. Both SDL1 and SDL2 binaries (in .app format) are provided in the macOS package, in the directories named "dosbox-x" and "dosbox-x-sdl2" inside the zip file.

The HX-DOS package allows you to run DOSBox-X in a real DOS system (MS-DOS or compatible) too with the help of the freely-available [HX DOS Extender](https://github.com/Baron-von-Riedesel/HX), which is already included in the recent HX-DOS release packages. Once you unzip the package you can directly type "DOSBOX-X" to run in DOS. Note however that not all features of DOSBox-X can be supported in this environment. See the README.TXT file inside the HX-DOS package for more information.

Note: You may not see such packages for some DOSBox-X versions. For example, these two packages are not available for DOSBox-X version 0.83.1.

Source Code Packages (zip or tar.gz)
------------------------------------

Full source code packages of DOSBox-X are also available in both zip and tar.gz formats. Both contain the full source code, but you probably want to download the source code in zip format if you are using Windows, and the source code in tar.gz format if you are using Linux. For the current DOSBox-X version 0.83.5, the source code packages are:

* [dosbox-x-v0.83.5.zip](https://github.com/joncampbell123/dosbox-x/archive/dosbox-x-v0.83.5.zip)
* [dosbox-x-v0.83.5.tar.gz](https://github.com/joncampbell123/dosbox-x/archive/dosbox-x-v0.83.5.tar.gz)

If you prefer you can compile DOSBox-X from the source code by yourself. The source code packages as listed in the Releases page contain the source code for that released version, and in this example the current DOSBox-X 0.83.5 version. On the other hand, if you are looking for the latest source code of DOSBox-X (including the most recent development changes in the source code), you may want to use the source code in the repository instead, or you can browse the latest source code using [Doxygen](http://dosbox-x.com/doxygen/html/index.html).

You could use either the released source code package or the latest source code according to your needs, and the source code may be compiled to run on the above-mentioned platforms (Windows, Linux, macOS and DOS) and possibly other operating systems too. Please see the [DOSBox-X source code description](README.source-code-description) file for detailed instructions on building the DOSBox-X source code and further information of the source code.
