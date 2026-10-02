# DOSBox-X Ubuntu Package Installation

1. Update the package information:

   sudo apt-get update

2. Install the DOSBox-X Debian package:

   sudo apt-get install ./dosbox-x_<version>nightly_amd64.deb

Replace `<version>` with the version of the downloaded package.

For example:

```
sudo apt-get install ./dosbox-x_20260910nightly_amd64.deb
```

APT will automatically install any required dependencies.

3. DOSBox-X provides two executables:

   dosbox-x-sdl1
   dosbox-x-sdl2

`dosbox-x-sdl2` is the SDL2 version and is the default version launched by the `dosbox-x` command.

`dosbox-x-sdl1` is the SDL1 version. It can be useful on older systems or in environments where SDL2 is not available or does not work properly.

4. Start the default SDL2 version:

   dosbox-x

5. To make `dosbox-x` launch the SDL1 version instead, replace the symbolic link:

   sudo ln -sf dosbox-x-sdl1 /usr/bin/dosbox-x

After this, running:

```
dosbox-x
```

will launch the SDL1 version.

6. To switch back to the SDL2 version:

   sudo ln -sf dosbox-x-sdl2 /usr/bin/dosbox-x

The installed executables can also be launched directly:

```
dosbox-x-sdl1
dosbox-x-sdl2
```
