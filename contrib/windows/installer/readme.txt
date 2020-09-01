README for DOSBox-X
===================

Welcome to DOSBox-X, a cross-platform DOS emulator based on the DOSBox project.

Like DOSBox, it emulates a PC necessary for running many MS-DOS games and applications that simply cannot be run on modern PCs and operating systems. However, while the main focus of DOSBox is for running DOS games, DOSBox-X goes much further than this. Started as a fork of the DOSBox project, it retains compatibility with the wide base of DOS games and DOS gaming DOSBox was designed for. But it is also a platform for running DOS applications, including emulating the environments to run Windows 3.x, 9x and ME and software written for those versions of Windows.

As a general-purpose DOS emulator, DOSBox-X has many useful and unique features that do not exist in other emulators like DOSBox, such as GUI menu and configuration tool, the ability to save and load state, support for more DOS commands and utilities, better compatibility with DOS applications, as well as Windows clipboard and long filename support for a tighter Windows integration. With DOSBox-X you can run most DOS applications and games reliably in a DOS virtual machine. When in the DOSBox-X command-line, you can type HELP for DOSBox-X command help. You can also open the file dosbox-x.conf for various optional settings in DOSBox-X.

DOSBox-X Quick Start
====================

Type INTRO in DOSBox-X for a quick tour. It is essential that you get familiar with the idea of mounting, since DOSBox-X does not automatically make any drive (or a part of it) accessible to the emulation, unless you turn on automatic mounting of available Windows drives at start by setting the option "automountall" to "true" (without quotes) in DOSBox-X's config file, which is dosbox-x.conf by default.

At the beginning you have got a Z:\> instead of a C:\> at the DOSBox-X prompt. Since no drives are mounted yet, you need to make your directories available as drives in DOSBox-X by using the "mount" command. For example, the command line "mount C D:\GAMES" will give you a C drive in DOSBox-X which points to your Windows D:\GAMES directory (that was created before). To change to the drive mounted like above, type "C:". If everything went fine, DOSBox-X will display the prompt "C:\>".

You do not have to always type these commands. Check out the "Drive" menu to mount drives from DOSBox-X's graphical interface. There is also an [autoexec] section in DOSBox-X's config file. The commands present there are run when DOSBox-X starts, so you can use this section for the mounting and other purposes, such as launching a specific program you want to use, or a game you want to play.

Further Information
===================
Please visit the DOSBox-X homepage for the latest information about DOSBox-X:
http://dosbox-x.com/

For a complete DOSBox-X user guide, including common ways to configure DOSBox-X and its usage tips, please visit the DOSBox-X Wiki:
https://github.com/joncampbell123/dosbox-x/wiki

If you have any issues or want to request new features, please feel free to post them in the DOSBox-X issue tracker:
https://github.com/joncampbell123/dosbox-x/issues

The source code of DOSBox-X is also available from the DOSBox-X GitHub site:
https://github.com/joncampbell123/dosbox-x/