README for DOSBox-X macOS version
=================================

Welcome to DOSBox-X, a cross-platform DOS emulator based on the DOSBox project.

This is an official DOSBox-X macOS package, which should run natively on the recent versions of macOS such as macOS Monterey (12), Big Sur (11.0), and Catalina (10.15). Both SDL1 and SDL2 binaries (in .app format) are provided in the macOS package, in the directories named "dosbox-x" and "dosbox-x-sdl2" respectively inside the zip file. While the SDL1 version is the default version, the SDL2 version may be preferred over the SDL1 version for certain features (particularly related to input handling) such as touchscreen input support. You can select either SDL1 or SDL2 version according to your preference, or just run the SDL1 version if you are not sure.

There are two ways to run DOSBox-X in macOS, either from the Finder or from the command-line (Terminal):

* From the Finder, go to the directory where the macOS zip package is downloaded, you will see a folder name which is the same as the file name of the downloaded zip package. Inside this folder you will see "dosbox-x" (SDL1) and "dosbox-x-sdl2" (SDL2). Go to either one and click the program "dosbox-x" to start DOSBox-X. If you see a dialog asking you to select a folder, please select one which will then become your DOSBox-X working directory. You can choose to save this folder after you select one so that the folder selection dialog will no show up again next time, or let DOSBox-X show the folder selection dialog every time you run it from the Finder.

* From the Terminal, go to the directory where the macOS zip package is downloaded, you will see a folder name which is the same as the file name of the downloaded zip package. Starting from this folder, use "cd" command to go to the directory where the DOSBox-X executable is located. For SDL1 build, type "cd dosbox-x/dosbox-x.app/Contents/MacOS", and for SDL2 build, type "cd dosbox-x-sdl2/dosbox-x.app/Contents/MacOS". Run DOSBox-X with "./dosbox-x" and you will see the DOSBox-X window.

In case the DOSBox-X binary does not run on your macOS system, you can try execute the following commands from the Terminal *once* in the directory that the zip package is extracted (where you see two directories named "dosbox-x" and "dosbox-x-sdl2"):

xattr -cr dosbox-x/dosbox-x.app dosbox-x-sdl2/dosbox-x.app
chmod +x dosbox-x/dosbox-x.app/Contents/MacOS/dosbox-x dosbox-x-sdl2/dosbox-x.app/Contents/MacOS/dosbox-x

Further Information
===================
Please visit the DOSBox-X homepage for the latest information about DOSBox-X:
https://dosbox-x.com/ or http://dosbox-x.software/

For a complete DOSBox-X user guide, including common ways to configure DOSBox-X and its usage tips, please visit the DOSBox-X Wiki:
https://dosbox-x.com/wiki

If you have any issues or want to request new features, please feel free to post them in the DOSBox-X issue tracker:
https://github.com/joncampbell123/dosbox-x/issues

The DOSBox-X Discord channel for real-time communications about the project is also available from:
https://discord.gg/5cnTmcsTpG
