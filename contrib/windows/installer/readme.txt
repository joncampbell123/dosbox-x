README for DOSBox-X
===================

Welcome to DOSBox-X, a cross-platform DOS emulator with the goal of being a complete emulation package that covers all pre-2000 DOS and Windows 9x based hardware scenarios, and gives you all the options to configure the DOS virtual machine.

Like DOSBox, it emulates a PC necessary for running many MS-DOS games and applications that simply cannot be run on modern PCs and operating systems. However, while the main focus of DOSBox is for running DOS games, DOSBox-X goes much further than this. Started as a fork of the DOSBox project, it retains compatibility with the wide base of DOS games and DOS gaming DOSBox was designed for. But it is also a platform for running DOS applications, including emulating the environments to run Windows 3.x, 9x and ME and software written for those versions of Windows.

As a general-purpose DOS emulator, DOSBox-X has many useful and unique features that do not exist in other emulators like DOSBox, such as GUI menu and configuration tool, the ability to save and load state, support for more DOS commands and utilities, better compatibility with DOS applications, as well as Windows clipboard and long filename support for a tighter Windows integration. With DOSBox-X you can run most DOS applications and games reliably in a DOS virtual machine. When in the DOSBox-X command-line, you can type HELP for DOSBox-X command help. You can also open the file dosbox-x.conf for various optional settings in DOSBox-X.

DOSBox-X Quick Start
====================

Type HELP to see a list of DOSBox-X's shell commands, and INTRO for a quick tour to DOSBox-X. It is essential that you get familiar with the idea of mounting, since DOSBox-X does not automatically make any drive (or a part of it) accessible to the emulation, unless you turn on automatic mounting of available Windows drives at start by setting the option "automountall" to "true" (without quotes) in DOSBox-X's configuration.

At the beginning you have got a Z:\> instead of a C:\> at the DOSBox-X prompt. Since no drives are mounted yet, you need to make your directories available as drives in DOSBox-X. DOSBox-X features a drop-down menu where you can do many things without typing commands. Check out the "Drive" menu to mount drives from DOSBox-X's graphical interface. Select a drive, such as "C", and click "Mount folder as Hard Disk", then the Windows file browser will appear. Select a folder and click "OK", then this folder will be mounted as Drive C: in DOSBox-X. Alternatively, you can mount drives in DOSBox-X with the "mount" command. For example, the command line "mount C D:\GAMES" will give you a C drive in DOSBox-X which points to your Windows D:\GAMES directory (that was created before).

To change to the drive mounted like above, type "C:". If everything went fine, DOSBox-X will display the prompt "C:\>". You do not have to manually mount drives after DOSBox-X starts. Click the "Main" menu, and select "Configuration tool". Then select the "AUTOEXEC.BAT" setting group, where you can change its contents. The commands present here are run when DOSBox-X starts, so you can use this section for the mounting and other purposes, such as launching a specific program you want to use, or a game you want to play. You can also quickly launch a DOS program or game in DOSBox-X by clicking "Quick launch program..." under "DOS" menu.

Hint: DOSBox-X supports different video output systems for different purposes. By default it uses the Direct3D output, but if you desire the pixel-perfect scaling feature for improved image quality you may want to select the openglpp output ("OpenGL perfect"). Also, if you use text-mode DOS applications and/or the DOS shell frequently you probably want to select the TrueType font (TTF) output to make the text screen look much better by using scalable TrueType fonts.

Troubleshooting
===============

- Choosing the appropriate build for your Windows version
    Modern Windows users (7 and after) should use the standard (non-XP, non-lowend) builds but may try the non-standard builds if you prefer, although officially not supported.
    Windows 9x/NT4/2000 users should use the MinGW lowend 9x builds (32-bit SDL1 only).
    Windows XP users must use the XP compatible installer with "XP" in the file name, which includes Visual Studio XP builds and the 32-bit MinGW low-end builds. Note that not all features are available in the MinGW low-end builds, currently Slirp support is known to be missing. You also need to install the DirectX runtime or DOSBox-X will complain you're missing XInput9_1_0.dll. XP compatible builds works in ReactOS as well, but support is considered experimental.
    Windows Vista users can use the XP installer or standard (non-XP) Visual Studio builds, because standard (non-XP) installer doesn't work in Vista. MinGW dropped support for XP/Vista, so install the 32-bit low-end builds from the XP compatible installer if you prefer MinGW builds.

- Joystick support for SDL1 builds
    Regarding Joystick support, the SDL1 builds requires XInput compatible devices. If your joystick is not Xinput compatible, you may want to try the SDL2 builds or DirectInput to XInput wrappers such as XOutput or Xbox 360 controller emulator.
      XOutput: https://github.com/csutorasa/XOutput
      Xbox 360 controller emulator: https://www.x360ce.com/

Further Information
===================
Please visit the DOSBox-X homepage for the latest information about DOSBox-X:
https://dosbox-x.com/ or http://dosbox-x.software/

Also, some useful information such as install and build instructions can be found in the official README file
https://github.com/joncampbell123/dosbox-x?tab=readme-ov-file#

For a complete DOSBox-X user guide, including common ways to configure DOSBox-X and its usage tips, please visit the DOSBox-X Wiki:
https://dosbox-x.com/wiki

If you have any issues or want to request new features, please feel free to post them in the DOSBox-X issue tracker:
https://github.com/joncampbell123/dosbox-x/issues

The source code of DOSBox-X is also available from the DOSBox-X GitHub site:
https://github.com/joncampbell123/dosbox-x/
