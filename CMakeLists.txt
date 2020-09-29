cmake_minimum_required(VERSION 3.10)
project(dosbox-x C CXX) # C for CheckIncludeFiles
include(CheckTypeSize)
include(CheckIncludeFiles)
include(CheckFunctionExists) # for HAVE_MPROTECT, HAVE_SET_PRIORITY

### options with safe-defaults (laid out like config.h.in)
# Define if building universal (internal helper macro)
# #undef AC_APPLE_UNIVERSAL_BUILD

# Compiling on BSD
# #undef BSD

if(MSVC)
	set(C_ATTRIBUTE_ALWAYS_INLINE 0)
elseif(GNU)
	set(C_ATTRIBUTE_ALWAYS_INLINE 1)
else()
	message(FATAL_ERROR "Option not implemented")
endif()

if(MSVC OR GNU)
	set(C_ATTRIBUTE_FASTCALL 1)
else()
	message(FATAL_ERROR "Option not implemented")
endif()

option(C_AVCODEC "Define to 1 to use FFMPEG libavcodec for video capture" OFF)
if(C_AVCODEC)
	set(C_AVCODEC 1)
endif()

option(C_CORE_INLINE "Define to 1 to use inlined memory functions in cpu core" ON)
if(C_CORE_INLINE)
	set(C_CORE_INLINE 1)
endif()

option(C_DEBUG "Define to 1 to enable internal debugger, requires libcurses" OFF)
if(C_DEBUG)
	set(C_DEBUG 1)
endif()

option(C_DIRECTLPT "Define to 1 if you want parallel passthrough support (Win32, Linux)" OFF)
if(C_DIRECTLPT)
	set(C_DIRECTLPT 1)
endif()

option(C_DIRECTSERIAL "Define to 1 if you want serial passthrough support (Win32, Posix and OS/2)" OFF)
if(C_DIRECTSERIAL)
	set(C_DIRECTSERIAL 1)
endif()

option(C_DYNAMIC_X86 "Define to 1 to use x86 dynamic cpu core" ON)
if(C_DYNAMIC_X86)
	set(C_DYNAMIC_X86 1)
endif()

option(C_FLUIDSYNTH "Define to 1 to enable fluidsynth MIDI synthesis" OFF)
if(C_FLUIDSYNTH)
	set(C_FLUIDSYNTH 1)
endif()

option(C_FPU "Define to 1 to enable floating point emulation" ON)
if(C_FPU)
	set(C_FPU 1)
endif()

if(MSVC)
	set(HAVE__ATTRIBUTE__ 0)
elseif(GNU)
	set(HAVE__ATTRIBUTE__ 1)
else()
	message(FATAL_ERROR "Option not implemented")
endif()

if(MSVC)
	set(C_HAS_BUILTIN_EXPECT 0)
elseif(GNU)
	set(C_HAS_BUILTIN_EXPECT 1)
else()
	message(FATAL_ERROR "Option not implemented")
endif()

check_function_exists(mprotect HAVE_MPROTECT)
if(HAVE_MPROTECT)
  set(HAVE_MPROTECT 1)
endif()

option(C_HEAVY_DEBUG "Define to 1 to enable heavy debugging, also have to enable C_DEBUG" OFF)
if(C_HEAVY_DEBUG)
	set(C_HEAVY_DEBUG 1)
endif()

option(C_IPX "Define to 1 to enable IPX over Internet networking, requires SDL_net" OFF)
if(C_IPX)
	set(C_IPX 1)
endif()

option(C_LIBPNG "Define to 1 if you have libpng" ON)
if(C_LIBPNG)
	set(C_LIBPNG 1)
endif()

option(C_MODEM "Define to 1 to enable internal modem support, requires SDL_net" OFF)
if(C_MODEM)
	set(C_MODEM 1)
endif()

option(C_MT32 "Define to 1 to enable MT32 emulation (x86/x86_64 only)" OFF)
if(C_MT32)
	set(C_MT32 1)
endif()

option(C_NE2000 "Define to 1 to enable NE2000 ethernet passthrough, requires libpcap" OFF)
if(C_NE2000)
	set(C_NE2000 1)
endif()

option(C_OPENGL "Define to 1 to use opengl display output support" OFF)
if(C_OPENGL)
	set(C_OPENGL 1)
endif()

option(C_SDL1 "Set to 1 to enable SDL 1.x support" OFF)
if(C_SDL1)
	set(C_SDL1 1)
endif()

option(C_SDL2 "Set to 1 to enable SDL 2.x support" ON)
if(C_SDL2)
	set(C_SDL2 1)
endif()

# TODO C_SDL_NET

check_function_exists(setpriority HAVE_SET_PRIORITY)
if(HAVE_SET_PRIORITY)
  set(C_SET_PRIORITY 1)
endif()

option(C_SSHOT "Define to 1 to enable screenshots, requires libpng" ON)
if(C_SSHOT)
	set(C_SSHOT 1)
endif()

option(C_UNALIGNED_MEMORY "Define to 1 to use a unaligned memory access" ON)
if(C_UNALIGNED_MEMORY)
	set(C_UNALIGNED_MEMORY 1)
endif()

set(VERSION "0.82.1" CACHE STRING "Version number of package")
set(PACKAGE_STRING "dosbox-x 0.821" CACHE STRING "Define to the full name and version of this package")
# set(PACKAGE "dosbox-x" CACHE STRING "Name of package")
# set(PACKAGE_BUGREPORT "https://github.com/joncampbell123/dosbox-x/issues" CACHE STRING "Define to the address where bug reports for this package should be sent")
# set(PACKAGE_NAME "dosbox-x" CACHE STRING "Define to the full name of this package")
# set(PACKAGE_TARNAME "dosbox-x" CACHE STRING "Define to the one symbol short name of this package")
# set(PACKAGE_URL "https://dosbox-x.com" CACHE STRING "Define to the home page for this package")
# set(PACKAGE_VERSION "0.821" CACHE STRING "Define to the version of this package")
# set(_FILE_OFFSET_BITS "64" CACHE STRING "Number of bits in a file offset, on hosts where this is settable")

set_property(GLOBAL PROPERTY USE_FOLDERS ON) # solution filters in VS

include(${CMAKE_CURRENT_SOURCE_DIR}/include/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/aviwriter/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/builtin/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/cpu/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/cpu/core_dyn_x86/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/cpu/core_dynrec/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/cpu/core_full/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/cpu/core_normal/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/debug/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/dos/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/fpu/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/gui/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/parport/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/reSID/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/serialport/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/snd_pc98/cbus/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/snd_pc98/common/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/snd_pc98/generic/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/snd_pc98/sound/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/snd_pc98/sound/getsnd/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/snd_pc98/x11/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/ints/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/libs/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/libs/gui_tk/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/libs/porttalk/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/libs/zmbv/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/libs/mt32/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/libs/mt32/freeverb/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/libs/mt32/sha1/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/misc/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/platform/CMakeLists.txt)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/shell/CMakeLists.txt)

set(ROOT
	# suppresses default VS filters, desired but why official way fails ?
	"${CMAKE_BINARY_DIR}/config.h"
)
source_group("\\" FILES ${ROOT})

add_executable(dosbox-x
	${ROOT}
	${INCLUDES}
	${SRC}
	${SRC_AVIWRITER}
	${SRC_BUILTIN}
	${SRC_CPU}
	${SRC_CPU_CORE_DYN_X86}
	${SRC_CPU_CORE_DYN_REC}
	${SRC_CPU_CORE_FULL}
	${SRC_CPU_CORE_NORMAL}
	${SRC_DEBUG}
	${SRC_DOS}
	${SRC_FPU}
	${SRC_GUI}
	${SRC_HARDWARE}
	${SRC_HARDWARE_PARPORT}
	${SRC_HARDWARE_RESID}
	${SRC_HARDWARE_SERIALPORT}
	${SRC_HARDWARE_SND_PC98}
	${SRC_HARDWARE_SND_PC98_CBUS}
	${SRC_HARDWARE_SND_PC98_COMMON}
	${SRC_HARDWARE_SND_PC98_GENERIC}
	${SRC_HARDWARE_SND_PC98_SOUND}
	${SRC_HARDWARE_SND_PC98_SOUND_GETSND}
	${SRC_HARDWARE_SND_PC98_X11}
	${SRC_INTS}
	${SRC_LIBS}
	${SRC_LIBS_GUI_TK}
	${SRC_LIBS_PORTTALK}
	${SRC_LIBS_ZMBV}
	${SRC_MISC}
	${SRC_MT32}
	${SRC_MT32_FREEVERB}
	${SRC_MT32_SHA1}
	${SRC_PLATFORM}
	${SRC_SHELL}
)

CHECK_INCLUDE_FILES(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILES(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILES(netinet/in.h HAVE_NETINET_IN_H)
CHECK_INCLUDE_FILES(pwd.h HAVE_PWD_H)
CHECK_INCLUDE_FILES(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILES(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILES(strings.h HAVE_STRINGS_H)
CHECK_INCLUDE_FILES(string.h HAVE_STRING_H)
CHECK_INCLUDE_FILES(sys/socket.h HAVE_SYS_SOCKET_H)
CHECK_INCLUDE_FILES(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILES(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILES(unistd.h HAVE_UNISTD_H)
CHECK_TYPE_SIZE("unsigned char" SIZEOF_UNSIGNED_CHAR)
CHECK_TYPE_SIZE("unsigned short" SIZEOF_UNSIGNED_SHORT)
CHECK_TYPE_SIZE("unsigned int" SIZEOF_UNSIGNED_INT)
CHECK_TYPE_SIZE("unsigned long" SIZEOF_UNSIGNED_LONG)
CHECK_TYPE_SIZE("unsigned long long" SIZEOF_UNSIGNED_LONG_LONG)
CHECK_TYPE_SIZE("int *" SIZEOF_INT_P)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/config.cmake.h
	${CMAKE_CURRENT_BINARY_DIR}/config.h
)

include_directories("include")
include_directories(${CMAKE_BINARY_DIR}) # unistd.h, config.h
include_directories(".") # TODO fix dumb bios.cpp include syntax
include_directories("src/aviwriter")
include_directories("src") # TODO lazyflags in fpu_instructions.h
include_directories("src/hardware/snd_pc98/cbus")
include_directories("src/hardware/snd_pc98/common")
include_directories("src/hardware/snd_pc98/generic")
include_directories("src/hardware/snd_pc98/sound")
include_directories("src/hardware/snd_pc98/sound/getsnd")
include_directories("src/hardware/snd_pc98/x11")
include_directories("${ZLIB_INCLUDE_DIR}/SDL2") # nasty hack (vcpkg-specific)

if (NOT MSVC)
	# x86 intrinsic include in src/libs/mt32/mmath.h fails on bare i686 target
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1")
endif ()

if (WIN32 AND (NOT MSVC))
	# Always build static binaries on MinGW
	SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
	SET(CMAKE_EXE_LINKER_FLAGS "-static")
endif ()

if (MSVC)
	# create fake unistd.h
	file(WRITE "${CMAKE_BINARY_DIR}/unistd.h" "") 
endif ()

if (WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D__WIN32__)
	add_definitions(-D CMAKE_BUILD) # shim for _nhandle
else ()
	set(SRC_LIBS_ZMBV "")
endif ()

if(C_SDL1)
	if (MSVC)
		find_path(SDL_INCLUDE_DIR .)
		find_path(LIBPNG_INCLUDE_DIR .)
		find_path(ZLIB_INCLUDE_DIR .)

		include_directories("${SDL_INCLUDE_DIR}")
		include_directories("${LIBPNG_INCLUDE_DIR}")
		include_directories("${ZLIB_INCLUDE_DIR}")

		find_library(SDL_LIBRARY sdl.lib)
		find_library(SDLMAIN_LIBRARY sdlmain.lib)
		find_library(LIBPNG_LIBRARY libpng16.lib)
		find_library(ZLIB_LIBRARY zlib.lib)
	else ()
		find_package(SDL REQUIRED)
		include_directories(${SDL_INCLUDE_DIR})
		find_package(PNG REQUIRED)
		include_directories(${PNG_INCLUDE_DIR})
		find_package(ZLIB REQUIRED)
		include_directories(${ZLIB_INCLUDE_DIR})
		find_package(OpenGL REQUIRED)
	endif ()

	if (WIN32)
		set(EXTRA_LIBS
				winmm
				imm32)
	endif ()

	if (MSVC)
		set(EXTRA_LIBS ${EXTRA_LIBS}
				opengl32.lib
				${SDL_LIBRARY}
				${SDLMAIN_LIBRARY}
				${LIBPNG_LIBRARY}
				${ZLIB_LIBRARY})
	else ()
		set(EXTRA_LIBS
				${SDL_LIBRARY}
				${OPENGL_LIBRARIES}
				${PNG_LIBRARIES}
				${ZLIB_LIBRARIES}
				${EXTRA_LIBS})
		if (WIN32)
			set(EXTRA_LIBS ${EXTRA_LIBS} dxguid)
		endif ()
	endif ()
elseif(C_SDL2)
	find_package(sdl2 REQUIRED)
	find_package(PNG REQUIRED)
	set(EXTRA_LIBS PNG::PNG)
	set(EXTRA_LIBS ${EXTRA_LIBS} SDL2::SDL2 SDL2::SDL2main)
else()
	message(FATAL_ERROR "No SDL version selected !")
endif()

if (C_SDL1 AND C_SDL2)
	message(FATAL_ERROR "Either SDL1 or SDL2 must be selected, not both !")
endif()

target_link_libraries(dosbox-x PRIVATE ${EXTRA_LIBS})

set_target_properties(dosbox-x PROPERTIES LINKER_LANGUAGE CXX)

# DEBUG print what's going on inside CMake
# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
	# message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()
