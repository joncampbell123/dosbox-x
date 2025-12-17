SDL_net is an example portable network library for use with SDL.

The source code is available from: http://www.libsdl.org/projects/SDL_net

This library is distributed under the terms of the zlib license: http://www.zlib.net/zlib_license.html

This packages contains the SDL2_net.framework for OS X. Conforming with Apple guidelines, this framework contains both the SDL runtime component and development header files.

Requirements:
You must have the SDL2.framework installed.

To Install:
Copy the SDL2_net.framework to /Library/Frameworks

You may alternatively install it in <your home directory>/Library/Frameworks if your access privileges are not high enough. (Be aware that the Xcode templates we provide in the SDL Developer Extras package may require some adjustment for your system if you do this.)

Use in CMake projects:
SDL2_net.framework can be used in CMake projects using the following pattern:
```
find_package(SDL2_net REQUIRED)
add_executable(my_game ${MY_SOURCES})
target_link_libraries(my_game PRIVATE SDL2_net::SDL2_net)
```
If SDL2_net.framework is installed in a non-standard location,
please refer to the following link for ways to configure CMake:
https://cmake.org/cmake/help/latest/command/find_package.html#config-mode-search-procedure
