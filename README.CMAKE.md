# CMake for dosbox-x

This is a preliminary CMake approach for dosbox-x, the first step in allowing us to zap tons of redundancy in the repository and make the overall developing experience on Windows better.

## Features
 - the entire directory structure has been replicated using filters in the solution
  - project structure looks the same as real organization of files
  - since components are separated, it is much easier to understand what does what and so on
  - it will be easier to zap components from the solution, e.g `core dynamic`
 - SDL, libpng and zlib are no more baked in the solution, rather they are 'just' external dependencies
 - leveraging of existing infrastructure, e.g. `config.h.in`
 - **no more need to deal and juggle with multiple VS solutions**  

## Known issues / Limitations / Notes
 - currently this had been done under Windows 10 with compiler version 10.0.16299.0 (pre-built package is below)
 - for the time being, linking is dynamic
 - not all dosbox-x components are enabled, yet
 - `Debug` configuration might blow, more CMake stuff is needed for that

## Getting started

This is a quick'n'dirty tutorial on how to get dosbox-x dependencies up for consumption.

- do not assume it is definite, this section *will* change
- it is solely provided as a reference for collaborators
- there is a pre-built 'package' of dependencies ready to use below
- consider it as not *production-ready* yet, rather as a proof of concept

### SDL

First, clone the SDL repository [https://github.com/aybe/SDL](https://github.com/aybe/SDL).

#### SDL 1

1. checkout `sdl-1.2` branch
2. open `SDL_VS2010.sln`, retarget solution
3. copy `include\SDL_config.h.default` to `include\SDL_config.h`
4. set `Release` or `Release_NoSTDIO` configuration, build solution
5. replicate an output similar to SDL 2 `INSTALL` output
	- create an `SDL1/x86` directory, add `bin`, `include/SDL1` and `lib` subdirs
	- copy `SDL\VisualC\SDL\Win32\Release\SDL.lib`, `SDL\VisualC\SDLmain\Win32\Release\SDLmain.lib` to `lib`
	- copy `SDL\VisualC\SDL\Win32\Release\SDL.dll` to `bin`
	- copy `SDL\include\*.h` to `include`
6. repeat step 5 for x64 

#### SDL 2

1. checkout `master` branch
2. generate solution with CMake, set install directory first (`CMAKE_INSTALL_PREFIX`)
3. open solution, set `Release` configuration, build `INSTALL`
4. repeat from step 2 for x64

### zlib

1. clone [https://github.com/aybe/zlib](https://github.com/aybe/zlib)
2. generate solution with CMake, set install directory first (`CMAKE_INSTALL_PREFIX`, `INSTALL_BIN_DIR`, `INSTALL_INC_DIR`, `INSTALL_LIB_DIR`, `INSTALL_MAN_DIR`, `INSTALL_PKGCONFIG_DIR`)
3. open solution, set `Release` configuration, build `INSTALL`
4. repeat from step 2 for x64

### libpng

1. clone [https://github.com/aybe/libpng](https://github.com/aybe/libpng)
2. generate solution with CMake, set install directory first (`CMAKE_INSTALL_PREFIX`)
3. click `Advanced`, point `ZLIB_INCLUDE_DIR`, `ZLIB_LIBRARY_DEBUG`, `ZLIB_LIBRARY_RELEASE` to the output of zlib `INSTALL`
  - for the former two you can just point to the same lib file
4. open solution, set `Release` configuration, build `INSTALL`
5. repeat from step 2 for x64

### dosbox-x

coming soon

## Downloads

coming soon