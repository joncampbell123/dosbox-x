
# Setup Windows 3.1 in DOSBox-X

(instructions tested against `dosbox-x-windows-20180901-124401-windows.zip`)

## Remarks

### Mouse

During the graphical portion of the installation, you will see that there are two mouse pointers visible on the screen but only one is moving and does nothing. This is because DOSBox-X mouse integration driver for Windows needs to be installed first. To use the mouse at this point, capture it with <key>Ctrl</key>+<key>F10</key>.

Or

If you don't want mouse integration from within Windows 3.1 (i.e. you prefer the old behavior), do not add `integration device=true` to the `[cpu]` section in `dosbox.conf` and skip the mouse integration driver installation section below.

### Video

This guide assumes you are using the default DOSBox-X machine `svga_s3` for which DOSBox-X sets video memory to `512Kb` by default. To use higher resolutions and color depths, adjust `vmemsize` in `[dosbox]` accordingly to your needs (see `dosbox.reference.conf`).

## Installation

### Windows 3.1

- download and extract [DOSBox-X](https://github.com/joncampbell123/dosbox-x/releases)
- next to `DOSBox-X.exe`, create a `VirtualHD` directory
- inside `VirtualHD` create a `WINSETUP` directory
- extract Windows 3.1 floppies into it ([7-Zip](https://www.7-zip.org/) can extract IMG files)
- next to `DOSBox-x.exe` create a `dosbox.conf` with the following content:
```
[cpu]
cycles=25000
integration device=true

[sblaster]
irq=5

[autoexec]
@ECHO OFF
PATH=%PATH%;C:\WINDOWS
MOUNT C .\VirtualHD
C:
```
- start DOSBox-X
- launch Windows installation `C:\WINSETUP\SETUP.EXE`
- install Windows using `Express Setup (Recommended)` option
- follow the installation instructions
- when asked, reboot, start Windows by typing `WIN`
  
### Mouse integration driver

- launch `Windows Setup` program
- open menu item `Options\Change System Settings`
- in `Mouse`, pick `Other mouse (Requires disk from OEM)...`
- type location to where you've extracted the driver and press `OK`
  - `DOSBox-X Mouse Pointer Integration` is selected by default
- press `OK`
- press `OK` again
- press `Restart Windows`

### S3 video driver

- launch `Windows Setup` program
- open menu item `Options\Change System Settings`
- in `Display`, pick `Other display (Requires disk from OEM)...`
- type location to where you've extracted the driver and press `OK`
  - `S3 Trio64V 1.70.04 640x480 256` is selected by default
- press `OK`
- press `OK` again
- when asked for the driver location again, type `C:\WINDOWS\SYSTEM`
- press `OK`
- press `Restart Windows`

### Sound Blaster 16 sound driver

- installation must be done from command-prompt, exit Windows if not already
- `cd` to where you've extract the driver and run `INSTALL.EXE`
- choose `Full Installation (Recommended)`
- follow installation instructions
- when asked to overwrite some files, choose `Backup`
- restart DOSBox-X
- start Windows
- the installer finishes with setting up the group of audio software for Windows

## Links

Here are some extra references to further tweak your installation:

[Windows 3.1x DOSBox Guide](https://www.vogons.org/viewtopic.php?t=9405)
[Windows 3x Downloads](http://www.sierrahelp.com/Utilities/Emulators/DOSBox/3x_Downloads.html)
[VOGONS Vintage Driver Library](http://vogonsdrivers.com/)

Tip : backup your working installation before attempting to install other drivers.
