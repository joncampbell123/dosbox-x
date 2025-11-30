DOSBox-X Installation and Released Packages
===========================================

DOSBox-X is a cross-platform DOS emulator based on DOSBox, with the eventual goal of being a complete DOS emulation package. New versions of DOSBox-X are released periodically, typically on the last day of a month or the first day of the next month. Since DOSBox-X is cross-platform, all major host operating systems are officially supported including Windows (Win9x, NT4, XP or later), Linux (with X11), macOS (10.15 or later) and pure DOS.

Pre-compiled binaries for Windows, Linux, macOS, and DOS as well as the source code are officially available.
Links to the current version can be found in the [DOSBox-X homepage](https://dosbox-x.com/), and all released versions along with their change logs can be found in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page.

**Binaries are automatically built upon release by github system, however, sometimes fails to upload. If you find a file for a particular platform not available, try the [Development(Nightly) build](https://dosbox-x.com/devel-build.html) instead.** You need to sign in to github in order to download those builds.

Once you get DOSBox-X installed and running, you probably want to look at the DOSBox-X user guide in the [DOSBox-X Wiki](https://dosbox-x.com/wiki) for usage information.

## SDL1 and SDL2 builds

For most packages there are both SDL1 and SDL2 builds, and most features are the same for both builds. While SDL1 builds may be the default one to use, you may want to try SDL2 builds if you want certain features specific to SDL2 builds (such as the raw mouse input option, touchscreen support) or you encounter specific issue(s) with SDL1 builds (such as incorrect keys in some international keyboard layouts).

## Packages for Supported Platforms

- [Windows Packages (Installer or Portable)](#windows-packages-installer-or-portable)
- [Linux Packages (Flatpak and more)](#linux-packages-flatpak-and-more)
- [macOS Packages (Portable)](#macos-packages-portable)
- [DOS Packages (Portable)](#dos-packages-portable)
- [Source Code Packages (zip or tar.gz)](#source-code-packages-zip-or-targz)

## Windows Packages (Installer or Portable)

**It is recommended to use the Windows installer found in the [DOSBox-X homepage](https://dosbox-x.com/) for the ease and more complete installations, especially for new and non-expert users.**

The installer allows you to specify the install folder and the default build to run (VS/MinGW, 32/64-bit, SDL1/SDL2, and Intel/ARM). You can also choose to install all available builds. A quick start guide is also included in the package, and shell context menus can be added for a fast launch of DOSBox-X from the Windows Explorer.

The Visual Studio (VS) builds are the default Windows builds to use. On the other hand, MinGW builds support the Slirp backend for the NE2000 networking but won't run on Windows XP/Vista. You may also want to use one of the MinGW builds if you encounter specific problem(s) with the Visual Studio builds (such as floating point precision issues). 

If you see the message ```Windows Defender SmartScreen prevented an unrecognized app from starting``` when running an installer, you can solve it by clicking the link "More info" in the dialog and then "Run anyway".

You can also easily upgrade DOSBox-X to the new version with the Windows installer. The installer in fact offers an option to automatically upgrade the config file (dosbox-x.conf) to the new version format while keeping all the user-customized settings already made.

Apart from the Windows installers, you can also find portable zip files starting with `dosbox-x-vsbuild-` and `dosbox-x-mingw-win` in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page as an alternative way to install DOSBox-X. In the zip file you will find `Release` and `Release SDL2` folders, which correspond to SDL1 and SDL2 respectively.  

Regarding Joystick support, the *SDL1 builds requires XInput compatible devices*. If your joystick is not Xinput compatible, you may want to try the SDL2 builds or DirectInput to XInput wrappers such as [Xoutput](https://github.com/csutorasa/XOutput) or [Xbox 360 controller emulator](https://www.x360ce.com/).

* Modern Windows users (7 and after) should use the standard (non-XP, non-lowend) builds but may try the non-standard builds if you prefer, although officially not supported.
* Windows 9x/NT4/2000 users should use the MinGW lowend 9x builds (32-bit SDL1 only).
* Windows XP users must use the XP compatible installer with "XP" in the file name, which includes Visual Studio XP builds and the 32-bit MinGW low-end builds. Note that not all features are available in the MinGW low-end builds, currently Slirp support is known to be missing. You also need to install the [DirectX runtime](https://www.microsoft.com/en-us/download/details.aspx?id=8109) or DOSBox-X will complain you're missing `XInput9_1_0.dll`. XP compatible builds works in ReactOS as well, but support is considered experimental.
* Windows Vista users can use the XP installer or standard (non-XP) Visual Studio portable builds, because standard (non-XP) installer doesn't work in Vista. MinGW dropped support for XP/Vista, so install the 32-bit low-end builds from the XP compatible installer if you prefer MinGW builds.


## Linux Packages (Flatpak and more)
DOSBox-X is available packaged in the below formats. 
You can select the one that best matches your Linux system and your needs.

### Flatpak
Flatpak packages are officially released for the Linux operating system (with X11 or Xwayland).

The Linux Flatpak package has the advantage of being supported by most Linux distributions, but it will run in a sandbox on your Linux system so that you may not be able to access some system-wide resources.

For the current DOSBox-X version the Linux Flatpak is available from:

* [com.dosbox_x.DOSBox-X.flatpakref](https://dl.flathub.org/repo/appstream/com.dosbox_x.DOSBox-X.flatpakref)

You may need to install Flatpak support depending on your Linux distribution for the first time. Please see the [Quick Setup page](https://flatpak.org/setup/) for more information specific to your Linux platform. The DOSBox-X Flathub page is also available from [here](https://flathub.org/apps/details/com.dosbox_x.DOSBox-X).

Once Flatpak support is enabled on your Linux system you can install the DOSBox-X Flatpak with the following command:

``flatpak install flathub com.dosbox_x.DOSBox-X``

Alternatively, you can install the DOSBox-X Flatpak locally if you downloaded the ``com.dosbox_x.DOSBox-X.flatpakref`` file to your computer:

``flatpak install --from com.dosbox_x.DOSBox-X.flatpakref``

After it is installed, it can be run with:

``flatpak run com.dosbox_x.DOSBox-X``

By default some system-wide resources will not be accessible by any Flatpak package.
But you can give the DOSBox-X Flatpak package additional access using the --filesystem option.
For example, to give it access to the /mnt directory:

``flatpak run --filesystem=/mnt com.dosbox_x.DOSBox-X``

In addition, if an earlier DOSBox-X Flatpak is already installed in the system you can update it to the current version with the command:

``flatpak update com.dosbox_x.DOSBox-X``

Or just ``flatpak update`` to update all installed Flatpak packages.

### RPM Package
RPM is a packaging format used by a variety of Linux distributions.
The current DOSBox-X version is offered via Fedora Copr here:

* [Fedora Copr DOSBox-X page](https://copr.fedorainfracloud.org/coprs/rob72/DOSBox-X/)

This supports the following Linux distributions:
- Fedora Linux (current versions plus rawhide)
- Red Hat Enterprise Linux (RHEL) 8 with EPEL
- CentOS 8 with EPEL.

Unlike a traditional package download, copr allows for the package to be automatically updated when the next release is available.

In addition, but this is not recommended, some RPM packages for older DOSBox-X version are available for CentOS in the DOSBox-X Github under Releases.
Simply pick the RPM package(s) for the version you want to use for your Linux platform and install.
On CentOS, RHEL or Fedora platforms, you can install an RPM package with a command line like this:

``sudo rpm -i <filename>.rpm``

Where ``<filename>`` is the main file name of the RPM package you wish to install. You may want to use the debug build if you desire to do some debugging work when running DOSBox-X. If there are missing dependencies for the rpm command, such as libpng and fluid-soundfont, then you will need to install them first. However, RPM packages are not natively supported by Linux distributions such as Debian, Ubuntu, or Linux Mint (although the ``alien`` command may sometimes help). In such case you probably want to use the Flatpak package, which works independent of your Linux distribution.

### SnapCraft
You can find DOSBox-X on SnapCraft (https://snapcraft.io/dosbox-x), which maintains universal Linux packages for software including DOSBox-X.
But please note that these packages are NOT built or updated by the DOSBox-X team.

### AUR Package
DOSBox-X is packaged for Arch Linux, and available in the user contributed AUR Package Repository.
But please note that these packages are NOT built or updated by the DOSBox-X Team.

- SDL1 build: https://aur.archlinux.org/packages/dosbox-x/
- SDL2 build: https://aur.archlinux.org/packages/dosbox-x-sdl2/

### DEB Package
DOSBox-X is packaged for Debian, and available in the
[trixie](https://packages.debian.org/trixie/dosbox-x) and
[unstable](https://packages.debian.org/unstable/dosbox-x)
repositories.
But please note that these packages are NOT built or updated by the DOSBox-X Team,
and that it only contains a small subset of the DOS programs provided in the Z: drive.

## macOS Packages (Portable)

Portable packages for the macOS (64-bit) platform are also available from the [DOSBox-X homepage](https://dosbox-x.com/).

The macOS package requires recent 64-bit Intel or ARM-based macOS (Catalina (10.15) and later). 
Using the Finder app, go to the folder where the macOS zip package is downloaded, and click the zip package. Then the package will be unzipped and you will see a folder with the same name as the package. 
Both SDL1 and SDL2 binaries (in .app format) are provided in the folders named ```dosbox-x``` and ```dosbox-x-sdl2``` respectively inside the zip file.

You have a choice of launching DOSBox-X by either from Launchpad, Finder, and Command-line (Terminal).

### Install and launch DOSBox-X from Launchpad
In either ```dosbox-x``` or ```dosbox-x-sdl2``` folders, you can find an app named ```dosbox-x```. 
Drag and drop the app to the ```Applications``` folder to install DOSBox-X to Launchpad.

### Launch DOSBox-X from Finder application
Instead of installing DOSBox-X to Launchpad as mentioned above, you can double-click the ```dosbox-x``` app to start DOSBox-X from the Finder application. 

### Launch DOSBox-X from command-line (Terminal)
Starting from the unzipped folder mentioned above, use ```cd``` command to go to the directory where the DOSBox-X executable is located. For SDL1 build, type ```cd dosbox-x/dosbox-x.app/Contents/MacOS```, and for SDL2 build, type ```cd dosbox-x-sdl2/dosbox-x.app/Contents/MacOS```. Run DOSBox-X with ```./dosbox-x``` and you will see the DOSBox-X window.

### Initial setting of your working directory
If you see a dialog asking you to select a folder after you launch DOSBox-X, please select one which will then become your DOSBox-X working directory. You can choose to save this folder after you select one so that the folder selection dialog will no show up again next time, or let DOSBox-X show the folder selection dialog every time you run it.

### Troubleshooting
#### I get a dialogue stating “The app is not from the Mac App Store”

You need change your settings to allow launching apps from known developers.
1. Choose Apple menu  > System Settings, then click Privacy & Security  in the sidebar. (You may need to scroll down.)
2. Go to Security, then click Open.
3. Click the pop-up menu next to “Allow applications from”, then choose App Store & Known Developers.
5. You should see a message mentioning launching "DOSBox-X" was blocked. Click ```Open Anyway```.
6. Enter your login password, then click OK.

#### I get ```"dosbox-x" is damaged and can't be opened``` error
  
You should be able to solve the problem by running the following command once in the Terminal.
1. Using the Terminal app, go to the unzipped folder of the macOS zip package. (You should find two folders ```dosbox-x``` and ```dosbox-x-sdl2```)
2. Run ``xattr -cr .``

### macOS Packages (Homebrew)
Homebrew provides [packages](https://formulae.brew.sh/formula/dosbox-x) for macOS Ventura and after.
You can install the package by the following steps.
1. Install [Homebrew](https://brew.sh)
2. In macOS Terminal (Applications -> Utilities -> Terminal) run `brew install dosbox-x`
3. Launch DOSBox-X by running `dosbox-x` in macOS Terminal.

Please note that these packages are NOT built or updated by the DOSBox-X team.

### macOS Packages (MacPorts)
[MacPorts](https://www.macports.org/) provides [packages](https://ports.macports.org/port/dosbox-x/details/) for High Sierra (10.13) and after. 
You can install the package by the following steps.
1. [Install MacPorts](https://www.macports.org/install.php)
2. In macOS terminal (Applications -> Utilities -> Terminal) run `sudo port install dosbox-x`
3. Launch DOSBox-X by running `dosbox-x` in macOS Terminal.

Please note that these packages are NOT built or updated by the DOSBox-X team.

### DOSBox-X for older macOS versions
Official portable packages for macOS versions 10.14 (Mojave) and earlier are no longer provided. Low-end builds named `DOSBox-X-macos-(version)-high-sierra.zip` were available up to [2022.09.0 (0.84.3)](https://github.com/joncampbell123/dosbox-x/releases/tag/dosbox-x-v0.84.3) for 10.12 (Sierra) and after.
Binaries for macOS versions earlier than 10.12 (Sierra) have not been provided. You may try to build yourself with the help of [MacPorts](https://www.macports.org/). 

## DOS Packages (Portable)

Besides Windows, Linux and macOS packages, there are also packages released for the DOS operating system. Yes, DOSBox-X can officially run on DOS systems as well, as some DOS users seem to prefer to run DOS applications and games through a DOS emulator. With DOSBox-X running in DOS you are able to emulate another DOS system with a different PC configuration (such as different machine types, video and sound cards etc) that works better for the purpose of the users. But please note that due to the limitations of this environment not all features of DOSBox-X that are available in other platforms can be supported in the DOS version.

The HX-DOS package allows you to run DOSBox-X in a real DOS system (MS-DOS 5.0+ or compatible) with the help of the freely-available [HX DOS Extender](https://github.com/Baron-von-Riedesel/HX), which is already included in the recent DOS release packages. Once you unzip the package you can directly type ```DOSBOX-X``` to run in DOS. See the README.TXT file inside the DOS package for more information.

Alternatively, you can run some versions of DOSBox-X (latest is [0.83.25 2022-05-01](https://github.com/joncampbell123/dosbox-x/releases/tag/dosbox-x-v0.83.25)) from a DOS environment with the help of the free [LOADLIN](https://docstore.mik.ua/orelly/linux/lnut/ch04_03.htm) program. With the LOADLIN DOS package you can run DOSBox-X right from DOSBox-X's DOS shell. Start the outside DOSBox-X with the setting ```memsize=127``` and ```cputype=pentium``` (perhaps also ```fullscreen=true``` and/or ```autolock=true```). Go to the directory where the files are extracted and type ```DOSBOX-X```. Then just wait for DOSBox-X to be automatically loaded within DOSBox-X.

Both DOS packages for the latest DOSBox-X version can be found in the [DOSBox-X homepage](https://dosbox-x.com/).

## Source Code Packages (zip or tar.gz)

Full source code packages of DOSBox-X are also available in both zip and tar.gz formats. Both contain the full source code, but you probably want to download the source code in zip format if you are using Windows, and the source code in tar.gz format if you are using Linux. The source code packages for the latest DOSBox-X version are available from the [DOSBox-X homepage](https://dosbox-x.com/).

If you prefer you can compile DOSBox-X from the source code by yourself. The source code packages as listed in the [Releases](https://github.com/joncampbell123/dosbox-x/releases) page contain the source code for that released version. On the other hand, if you are looking for the latest source code of DOSBox-X (including the most recent development changes in the source code), you may want to use the source code in the repository instead, or you can browse the latest source code using [Doxygen](https://dosbox-x.com/doxygen/html/index.html).

You could use either the released source code package or the latest source code according to your needs, and the source code may be compiled to run on the above-mentioned platforms (Windows, Linux, macOS and DOS) and possibly other operating systems too. Please see the [BUILD](BUILD.md) page for detailed instructions on building the DOSBox-X source code. Further descriptions of the source code can be found in the [DOSBox-X source code description](README.source-code-description) file.

## Development nightly builds

The development (preview) builds intended for testing purposes for the supported platforms can be found [here](https://dosbox-x.com/devel-build.html).

