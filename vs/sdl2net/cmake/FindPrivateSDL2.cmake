# This should be provided by recent SDL2 versions.
# But the earlier compatible versions may lack it

include(FindPackageHandleStandardArgs)
include("${CMAKE_CURRENT_LIST_DIR}/CommonFindSDL2.cmake")

find_library(SDL2_LIBRARY
    NAMES SDL2
    HINTS ${SDL2_DIR} ENV SDL2_DIR
    PATH_SUFFIXES ${_lib_suffixes}
)

find_path(SDL2_INCLUDE_DIR
    NAMES SDL_haptic.h
    PATH_SUFFIXES SDL2
    HINTS ${SDL2_DIR} ENV SDL2_DIR
    PATH_SUFFIXES ${_inc_suffixes}
)

set(SDL2_VERSION)
if(SDL2_INCLUDE_DIR)
    file(READ "${SDL2_INCLUDE_DIR}/SDL_version.h" _sdl_version_h)
    string(REGEX MATCH "#define[ \t]+SDL_MAJOR_VERSION[ \t]+([0-9]+)" _sdl2_major_re "${_sdl_version_h}")
    set(_sdl2_major "${CMAKE_MATCH_1}")
    string(REGEX MATCH "#define[ \t]+SDL_MINOR_VERSION[ \t]+([0-9]+)" _sdl2_minor_re "${_sdl_version_h}")
    set(_sdl2_minor "${CMAKE_MATCH_1}")
    string(REGEX MATCH "#define[ \t]+SDL_PATCHLEVEL[ \t]+([0-9]+)" _sdl2_patch_re "${_sdl_version_h}")
    set(_sdl2_patch "${CMAKE_MATCH_1}")
    if(_sdl2_major_re AND _sdl2_minor_re AND _sdl2_patch_re)
        set(SDL2_VERSION "${_sdl2_major}.${_sdl2_minor}.${_sdl2_patch}")
    endif()
endif()

find_package_handle_standard_args(PrivateSDL2
    REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR
    VERSION_VAR SDL2_VERSION
)

if(PrivateSDL2_FOUND)
    if(NOT TARGET PrivateSDL2::PrivateSDL2)
        add_library(PrivateSDL2::PrivateSDL2 UNKNOWN IMPORTED)
        set_target_properties(PrivateSDL2::PrivateSDL2 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${SDL2_LIBRARY}"
        )
    endif()
endif()
