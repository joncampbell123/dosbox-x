# FIXME: this should be provided by SDL2

include(FindPackageHandleStandardArgs)
include("${CMAKE_CURRENT_LIST_DIR}/CommonFindSDL2.cmake")

find_library(SDL2_MAIN_LIBRARY
    NAMES SDL2main
    HINTS ${SDL2_DIR} ENV SDL2_DIR
    PATH_SUFFIXES ${_lib_suffixes}
)

find_package_handle_standard_args(SDL2main
    REQUIRED_VARS SDL2_MAIN_LIBRARY
)

if(SDL2main_FOUND)
    if(NOT TARGET SDL2::SDL2main)
        add_library(SDL2::SDL2main UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2main PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${SDL2_MAIN_LIBRARY}"
        )
    endif()
endif()
