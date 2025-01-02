README for DOSBox-X macOS version
=================================

Welcome to DOSBox-X, a cross-platform DOS emulator based on the DOSBox project.

This is an official DOSBox-X macOS package, which should run natively on the recent versions of macOS such as macOS Monterey (12), Big Sur (11), and Catalina (10.15). Both SDL1 and SDL2 binaries (in .app format) are provided in the macOS package, in the directories named "dosbox-x" and "dosbox-x-sdl2" respectively inside the zip file. While the SDL1 version is the default version, the SDL2 version may be preferred over the SDL1 version for certain features (particularly related to input handling) such as touchscreen input support. You can select either SDL1 or SDL2 version according to your preference, or just run the SDL1 version if you are not sure.

DOSBox-X Quick start
====================

Using the Finder app, go to the folder where the macOS zip package is downloaded, and click the zip package. Then the package will be unzipped and you will see a folder with the same name as the package. Both SDL1 and SDL2 binaries (in .app format) are provided in the folders named dosbox-x and dosbox-x-sdl2 respectively inside the zip file.

There are three ways to run DOSBox-X in macOS, either from Launchpad, Finder, and Command-line (Terminal).

* Install and launch DOSBox-X from Launchpad
In either dosbox-x or dosbox-x-sdl2 folders, you can find an app named dosbox-x. Drag and drop the app to the Applications folder to install DOSBox-X to Launchpad.

* Launch DOSBox-X from Finder application
Instead of installing DOSBox-X to Launchpad as mentioned above, you can double-click the dosbox-x app to start DOSBox-X from the Finder application.

* Launch DOSBox-X from command-line (Terminal)
Starting from the unzipped folder mentioned above, use cd command to go to the directory where the DOSBox-X executable is located. For SDL1 build, type cd dosbox-x/dosbox-x.app/Contents/MacOS, and for SDL2 build, type cd dosbox-x-sdl2/dosbox-x.app/Contents/MacOS. Run DOSBox-X with ./dosbox-x and you will see the DOSBox-X window.

Troubleshooting
===============

- I get a dialogue stating “The app is not from the Mac App Store”

You need change your settings to allow launching apps from known developers.
Confirm "Privacy & Security" in the macOS Users Guide for details.

    Choose Apple menu > System Settings, then click Privacy & Security in the sidebar. (You may need to scroll down.)
    Go to Security, then click Open.
    Click the pop-up menu next to “Allow applications from”, then choose App Store & Known Developers.
    You should see a message mentioning launching "DOSBox-X" was blocked. Click Open Anyway.
    Enter your login password, then click OK.

- I get "dosbox-x" is damaged and can't be opened error

You should be able to solve the problem by running the following command once in the Terminal.

    Using the Terminal app, go to the unzipped folder of the macOS zip package. (You should find two folders dosbox-x and dosbox-x-sdl2)
    Run xattr -cr .

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

The DOSBox-X Discord channel for real-time communications about the project is also available from:
https://discord.gg/5cnTmcsTpG
