# FIXME: this should be provided by SDL2

include(FindPackageHandleStandardArgs)
include("${CMAKE_CURRENT_LIST_DIR}/CommonFindSDL2.cmake")

find_library(SDL2_TEST_LIBRARY
    NAMES SDL2test SDL2_test
    HINTS ${SDL2_DIR} ENV SDL2_DIR
    PATH_SUFFIXES ${_lib_suffixes}
)

find_package_handle_standard_args(SDL2test
    REQUIRED_VARS SDL2_TEST_LIBRARY
)

if(SDL2test_FOUND)
    if(NOT TARGET SDL2::SDL2test)
        add_library(SDL2::SDL2test UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2test PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "C"
            IMPORTED_LOCATION "${SDL2_TEST_LIBRARY}"
        )
        if(MSVC AND ((SDL2_VERSION AND SDL2_VERSION VERSION_LESS "2.0.20") OR NOT SDL2_VERSION))
            # FIXME: remove once minimum required SDL library is >=2.0.20
            # Until 2.0.18, SDL2test.lib used `printf` in SDL_test_common.c. instead of `SDL_log`. (fixed in 2.0.20)
            set_target_properties(SDL2::SDL2test PROPERTIES
                INTERFACE_LINK_LIBRARIES "legacy_stdio_definitions.lib"
            )
        endif()
    endif()
endif()
