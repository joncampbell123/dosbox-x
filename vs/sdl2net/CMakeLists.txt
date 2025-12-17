cmake_minimum_required(VERSION 3.16)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# See release_checklist.md
set(MAJOR_VERSION 2)
set(MINOR_VERSION 2)
set(MICRO_VERSION 0)
set(SDL_REQUIRED_VERSION 2.0.4)

set(DYLIB_COMPATIBILITY_VERSION "1")

include(PrivateSdlFunctions)
sdl_calculate_derived_version_variables()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the SDL_net source code and call cmake from there")
endif()

project(SDL2_net
    LANGUAGES C
    VERSION "${FULL_VERSION}"
)

message(STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION}")

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(SDL2NET_ROOTPROJECT ON)
else()
    set(SDL2NET_ROOTPROJECT OFF)
endif()

if(TARGET SDL2::SDL2test)
    set(SDL2test_FOUND ON)
else()
    find_package(SDL2test)
endif()

# Set defaults preventing destination file conflicts
set(SDL2NET_DEBUG_POSTFIX "d"
    CACHE STRING "Name suffix for debug builds")
mark_as_advanced(SDL2NET_DEBUG_POSTFIX)

include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(CMAKE_POSITION_INDEPENDENT_CODE "Build static libraries with -fPIC" ON)
option(BUILD_SHARED_LIBS "Build the library as a shared library" ON)

option(SDL2NET_INSTALL "Enable SDL2_net install target" ${SDL2NET_ROOTPROJECT})

cmake_dependent_option(SDL2NET_SAMPLES "Build SDL2_net samples" ${SDL2NET_ROOTPROJECT} "${SDL2test_FOUND}" OFF)

if(SDL2NET_SAMPLES)
    find_package(SDL2test REQUIRED)
endif()

# Save BUILD_SHARED_LIBS variable
set(SDL2NET_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

if(SDL2NET_BUILD_SHARED_LIBS)
    set(sdl2_net_export_name SDL2_net)
    set(sdl2_net_install_name_infix shared)
    set(sdl2_target_name SDL2::SDL2)
else()
    set(sdl2_net_export_name SDL2_net-static)
    set(sdl2_net_install_name_infix static)
    set(sdl2_target_name SDL2::SDL2-static)
endif()

sdl_find_sdl2(${sdl2_target_name} ${SDL_REQUIRED_VERSION})

add_library(SDL2_net
    SDLnet.c
    SDLnetselect.c
    SDLnetTCP.c
    SDLnetUDP.c
)
add_library(SDL2_net::${sdl2_net_export_name} ALIAS SDL2_net)
target_include_directories(SDL2_net PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/SDL2>"
)
target_compile_definitions(SDL2_net PRIVATE
    BUILD_SDL
    SDL_BUILD_MAJOR_VERSION=${MAJOR_VERSION}
    SDL_BUILD_MINOR_VERSION=${MINOR_VERSION}
    SDL_BUILD_MICRO_VERSION=${MICRO_VERSION}
)
target_link_libraries(SDL2_net PRIVATE $<BUILD_INTERFACE:${sdl2_target_name}>)
if(WIN32)
    if (MSVC)
        target_compile_options(SDL2_net PRIVATE /W3 /wd4244)
    endif()
    target_compile_definitions(SDL2_net PRIVATE _WINSOCK_DEPRECATED_NO_WARNINGS)
    target_link_libraries(SDL2_net PRIVATE ws2_32 iphlpapi)
    if(SDL2NET_BUILD_SHARED_LIBS)
        target_sources(SDL2_net PRIVATE version.rc)
    endif()
endif()
set_target_properties(SDL2_net PROPERTIES
    DEFINE_SYMBOL DLL_EXPORT
    EXPORT_NAME ${sdl2_net_export_name}
    C_VISIBILITY_PRESET "hidden"
)
if(NOT ANDROID)
    set_target_properties(SDL2_net PROPERTIES
        DEBUG_POSTFIX "${SDL2NET_DEBUG_POSTFIX}"
    )
    if(APPLE)
        # the SOVERSION property corresponds to the compatibility version and VERSION corresponds to the current version
        # https://cmake.org/cmake/help/latest/prop_tgt/SOVERSION.html#mach-o-versions
        set_target_properties(SDL2_net PROPERTIES
            SOVERSION "${DYLIB_COMPATIBILITY_VERSION}"
            VERSION "${DYLIB_CURRENT_VERSION}"
        )
    else()
        set_target_properties(SDL2_net PROPERTIES
            SOVERSION "${LT_MAJOR}"
            VERSION "${LT_VERSION}"
        )
    endif()
endif()
if(SDL2NET_BUILD_SHARED_LIBS AND (APPLE OR (UNIX AND NOT ANDROID)))
    add_custom_command(TARGET SDL2_net POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E create_symlink "$<TARGET_SONAME_FILE_NAME:SDL2_net>" "libSDL2_net$<$<CONFIG:Debug>:${SDL2NET_DEBUG_POSTFIX}>$<TARGET_FILE_SUFFIX:SDL2_net>"
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
endif()
if(SDL2NET_BUILD_SHARED_LIBS)
    if(WIN32 OR OS2)
        set_target_properties(SDL2_net PROPERTIES
            PREFIX ""
        )
    endif()
    if(OS2)
        # FIXME: OS/2 Makefile has a different LIBNAME
        set_target_properties(SDL2_net PROPERTIES
            OUTPUT_NAME "SDL2net"
        )
    elseif(UNIX AND NOT ANDROID)
        set_target_properties(SDL2_net PROPERTIES
            OUTPUT_NAME "SDL2_net-${LT_RELEASE}"
        )
    endif()
else()
    if(MSVC OR (WATCOM AND (WIN32 OR OS2)))
        set_target_properties(SDL2_net PROPERTIES
            OUTPUT_NAME "SDL2_net-static"
        )
    endif()
endif()

# Use `Compatible Interface Properties` to ensure a shared SDL2_net is built with a shared SDL2
if(SDL2NET_BUILD_SHARED_LIBS)
    set_target_properties(SDL2_net PROPERTIES 
        INTERFACE_SDL2_SHARED ${SDL2NET_BUILD_SHARED_LIBS}
        COMPATIBLE_INTERFACE_BOOL SDL2_SHARED
    )
endif()

if(SDL2NET_BUILD_SHARED_LIBS)
    sdl_target_link_options_no_undefined(SDL2_net)
endif()

if(SDL2NET_INSTALL)
    install(
        TARGETS SDL2_net
        EXPORT SDL2NetExports
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT devel
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT library
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT library
    )
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL_net.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/SDL2" COMPONENT devel
    )

    if(WIN32 AND NOT MINGW)
        set(PKG_PREFIX "cmake")
    else()
        set(PKG_PREFIX "${CMAKE_INSTALL_LIBDIR}/cmake/SDL2_net")
    endif()

    configure_package_config_file(SDL2_netConfig.cmake.in SDL2_netConfig.cmake
        INSTALL_DESTINATION "${PKG_PREFIX}"
    )
    write_basic_package_version_file("${PROJECT_BINARY_DIR}/SDL2_netConfigVersion.cmake"
        VERSION ${FULL_VERSION}
        COMPATIBILITY AnyNewerVersion
    )
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/SDL2_netConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/SDL2_netConfigVersion.cmake"
        DESTINATION "${PKG_PREFIX}"
        COMPONENT devel
    )
    install(EXPORT SDL2NetExports
        FILE SDL2_net-${sdl2_net_install_name_infix}-targets.cmake
        NAMESPACE SDL2_net::
        DESTINATION "${PKG_PREFIX}"
        COMPONENT devel
    )

    if(SDL2NET_BUILD_SHARED_LIBS)
        # Only create a .pc file for a shared SDL2_net
        set(prefix "${CMAKE_INSTALL_PREFIX}")
        set(exec_prefix "\${prefix}")
        set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
        set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
        set(PACKAGE "${PROJECT_NAME}")
        set(VERSION "${FULL_VERSION}")
        set(SDL_VERSION "${SDL_REQUIRED_VERSION}")
        string(JOIN " " PC_REQUIRES ${PC_REQUIRES})
        string(JOIN " " PC_LIBS ${PC_LIBS})
        configure_file("${PROJECT_SOURCE_DIR}/SDL2_net.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net.pc.intermediate" @ONLY)
        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-$<CONFIG>.pc" INPUT "${CMAKE_CURRENT_BINARY_DIR}/SDL2_net.pc.intermediate")

        set(PC_DESTDIR)
        if(CMAKE_SYSTEM_NAME MATCHES FreeBSD)
            # FreeBSD uses ${PREFIX}/libdata/pkgconfig
            set(PC_DESTDIR "libdata/pkgconfig")
        else()
            set(PC_DESTDIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
        endif()
        # Only install a SDL2_net.pc file in Release mode
        install(CODE "
            if(CMAKE_INSTALL_CONFIG_NAME MATCHES \"Release\")
                # FIXME: use file(COPY_FILE) if minimum CMake version >= 3.21
                execute_process(COMMAND \"\${CMAKE_COMMAND}\" -E copy_if_different
                    \"${CMAKE_CURRENT_BINARY_DIR}/SDL2_net-$<CONFIG>.pc\"
                    \"${CMAKE_CURRENT_BINARY_DIR}/SDL2_net.pc\")
                file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${PC_DESTDIR}\"
                    TYPE FILE
                    FILES \"${CMAKE_CURRENT_BINARY_DIR}/SDL2_net.pc\")
            endif()" COMPONENT devel)
    endif()

    if(SDL2NET_BUILD_SHARED_LIBS AND (APPLE OR (UNIX AND NOT ANDROID)))
        install(
            FILES
                "${PROJECT_BINARY_DIR}/libSDL2_net$<$<CONFIG:Debug>:${SDL2NET_DEBUG_POSTFIX}>$<TARGET_FILE_SUFFIX:SDL2_net>"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            COMPONENT devel
        )
    endif()

    install(FILES "LICENSE.txt"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/${PROJECT_NAME}"
        COMPONENT library
    )
endif()

if(SDL2NET_SAMPLES)
    find_package(SDL2main)

    add_executable(showinterfaces showinterfaces.c)
    target_compile_definitions(showinterfaces PRIVATE SDL_MAIN_HANDLED)
    target_link_libraries(showinterfaces PRIVATE SDL2_net::${sdl2_net_export_name} ${sdl2_target_name})

    add_executable(chat chat.c chat.h)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(chat PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(chat PRIVATE SDL2_net::${sdl2_net_export_name} SDL2::SDL2test ${sdl2_target_name})

    add_executable(chatd chatd.c)
    target_compile_definitions(chatd PRIVATE SDL_MAIN_HANDLED)
    target_link_libraries(chatd PRIVATE SDL2_net::${sdl2_net_export_name} ${sdl2_target_name})
endif()
