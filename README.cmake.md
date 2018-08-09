The CMake is provided as an alternative to the manually-crafted Visual Studio solutions, but over time was expanded to support other platforms.

# Windows

It leverages vcpkg for its dependencies, as a consequence only SDL2 is supported.

Note: DOSBox configuration options defaults have been set for this environment.

To install the required dependencies: ```vcpkg install zlib sdl2 libpng```.

# Linux

Both SDL versions are supported.

TODO

Thanks to https://github.com/mkoloberdin for the Linux patches.