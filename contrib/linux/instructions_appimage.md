# DOSBox-X AppImage

This AppImage contains both the SDL1 and SDL2 versions of DOSBox-X.

## Requirements

A 64-bit Linux system is required.

The AppImage is built on Ubuntu 22.04. It is intended for use on modern 64-bit Linux distributions.

## Running DOSBox-X

Make the AppImage executable if necessary:

```bash
chmod +x dosbox-x_*nightly_amd64.AppImage
```

Run the SDL2 version:

```bash
./dosbox-x_*nightly_amd64.AppImage
```

The SDL2 version is used by default.

To run the SDL1 version:

```bash
./dosbox-x_*nightly_amd64.AppImage --sdl1
```

The `--sdl1` option is handled by the AppImage launcher and is not passed to DOSBox-X.

All other options are passed directly to the selected DOSBox-X executable. For example:

```bash
./dosbox-x_*nightly_amd64.AppImage -fullscreen
```

runs the SDL2 version with the `-fullscreen` option.

The following runs the SDL1 version with the same DOSBox-X option:

```bash
./dosbox-x_*nightly_amd64.AppImage --sdl1 -fullscreen
```

## AppImage-specific options

To display the AppImage launcher help:

```bash
./dosbox-x_*nightly_amd64.AppImage --appimage-help
```

This displays:

* `--sdl1` — Run the SDL1 version.
* `--appimage-help` — Display AppImage-specific help.

The normal DOSBox-X `--help` option is passed to DOSBox-X:

```bash
./dosbox-x_*nightly_amd64.AppImage --help
```

## DOSBox-X options

Except for the AppImage-specific `--sdl1` and `--appimage-help` options, command-line options are passed directly to DOSBox-X.

For more information about DOSBox-X options and configuration, see the included reference configuration files.
