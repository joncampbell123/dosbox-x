# This file is shared amongst SDL_image/SDL_mixer/SDL_net/SDL_ttf

macro(sdl_calculate_derived_version_variables)
    if (NOT DEFINED MAJOR_VERSION OR NOT DEFINED MINOR_VERSION OR NOT DEFINED MICRO_VERSION)
        message(FATAL_ERROR "MAJOR_VERSION, MINOR_VERSION and MICRO_VERSION need to be defined")
    endif()

    set(FULL_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}")

    # Calculate a libtool-like version number
    math(EXPR BINARY_AGE "${MINOR_VERSION} * 100 + ${MICRO_VERSION}")
    math(EXPR IS_DEVELOPMENT "${MINOR_VERSION} % 2")
    if (IS_DEVELOPMENT)
        # Development branch, 2.5.1 -> libSDL2_XXXXX-2.0.so.0.501.0
        set(INTERFACE_AGE 0)
    else()
        # Stable branch, 2.6.1 -> libSDL2_XXXXX-2.0.so.0.600.1
        set(INTERFACE_AGE ${MICRO_VERSION})
    endif()

    # Increment this if there is an incompatible change - but if that happens,
    # we should rename the library from SDL2 to SDL3, at which point this would
    # reset to 0 anyway.
    set(LT_MAJOR "0")

    math(EXPR LT_AGE "${BINARY_AGE} - ${INTERFACE_AGE}")
    math(EXPR LT_CURRENT "${LT_MAJOR} + ${LT_AGE}")
    set(LT_REVISION "${INTERFACE_AGE}")
    # For historical reasons, the library name redundantly includes the major
    # version twice: libSDL2_XXXXX-2.0.so.0.
    # TODO: in SDL 3, set the OUTPUT_NAME to plain SDL3_XXXXX, which will simplify
    # it to libSDL2_XXXXX.so.0
    set(LT_RELEASE "2.0")
    set(LT_VERSION "${LT_MAJOR}.${LT_AGE}.${LT_REVISION}")

    # The following should match the versions in the Xcode project file.
    # Each version is 1 higher than you might expect, for compatibility
    # with libtool: macOS ABI versioning is 1-based, unlike other platforms
    # which are normally 0-based.
    math(EXPR DYLIB_CURRENT_VERSION_MAJOR "${LT_MAJOR} + ${LT_AGE} + 1")
    math(EXPR DYLIB_CURRENT_VERSION_MINOR "${LT_REVISION}")
    math(EXPR DYLIB_COMPAT_VERSION_MAJOR "${LT_MAJOR} + 1")
    set(DYLIB_CURRENT_VERSION "${DYLIB_CURRENT_VERSION_MAJOR}.${DYLIB_CURRENT_VERSION_MINOR}.0")
endmacro()

macro(sdl_find_sdl2 TARGET VERSION)
    if(NOT TARGET ${TARGET})
        # FIXME: can't add REQUIRED since not all SDL2 installs ship SDL2ConfigVersion.cmake (or sdl2-config-version.cmake)
        find_package(SDL2 ${VERSION} QUIET)
    endif()
    if(NOT TARGET ${TARGET})
        # FIXME: can't add REQUIRED since not all SDL2 installs ship SDL2Config.cmake (or sdl2-config.cmake)
        find_package(SDL2 QUIET)
        if(SDL2_FOUND)
            message(WARNING "Could not verify SDL2 version. Assuming SDL2 has version of at least ${VERSION}.")
        endif()
    endif()

    # Use Private FindSDL2.cmake module to find SDL2 for installations where no SDL2Config.cmake is available,
    # or for those installations where no target is generated.
    if (NOT TARGET ${TARGET})
        message(STATUS "Using private SDL2 find module")
        find_package(PrivateSDL2 ${VERSION} REQUIRED)
        add_library(${TARGET} INTERFACE IMPORTED)
        set_target_properties(${TARGET} PROPERTIES
            INTERFACE_LINK_LIBRARIES "PrivateSDL2::PrivateSDL2"
        )
    endif()
endmacro()

function(read_absolute_symlink DEST PATH)
    file(READ_SYMLINK "${PATH}" p)
    if (NOT IS_ABSOLUTE p)
        get_filename_component(pdir "${PATH}" DIRECTORY)
        set(p "${pdir}/${p}")
    endif()
    set("${DEST}" "${p}" PARENT_SCOPE)
endfunction()

function(win32_implib_identify_dll DEST IMPLIB)
    cmake_parse_arguments(ARGS "NOTFATAL" "" "" ${ARGN})
    if (CMAKE_DLLTOOL)
        execute_process(
            COMMAND "${CMAKE_DLLTOOL}" --identify "${IMPLIB}"
            RESULT_VARIABLE retcode
            OUTPUT_VARIABLE stdout
            ERROR_VARIABLE stderr)
        if (NOT retcode EQUAL 0)
            if (NOT ARGS_NOTFATAL)
                message(FATAL_ERROR "${CMAKE_DLLTOOL} failed.")
            else()
                set("${DEST}" "${DEST}-NOTFOUND" PARENT_SCOPE)
                return()
            endif()
        endif()
        string(STRIP "${stdout}" result)
        set(${DEST} "${result}" PARENT_SCOPE)
    elseif (MSVC)
        get_filename_component(CMAKE_C_COMPILER_DIRECTORY "${CMAKE_C_COMPILER}" DIRECTORY CACHE)
        find_program(CMAKE_DUMPBIN NAMES dumpbin PATHS "${CMAKE_C_COMPILER_DIRECTORY}")
        if (CMAKE_DUMPBIN)
            execute_process(
                COMMAND "${CMAKE_DUMPBIN}" "-headers" "${IMPLIB}"
                RESULT_VARIABLE retcode
                OUTPUT_VARIABLE stdout
                ERROR_VARIABLE stderr)
            if (NOT retcode EQUAL 0)
                if (NOT ARGS_NOTFATAL)
                    message(FATAL_ERROR "dumpbin failed.")
                else()
                    set(${DEST} "${DEST}-NOTFOUND" PARENT_SCOPE)
                    return()
                endif()
            endif()
            string(REGEX MATCH "DLL name[ ]+:[ ]+([^\n]+)\n" match "${stdout}")
            if (NOT match)
                if (NOT ARGS_NOTFATAL)
                    message(FATAL_ERROR "dumpbin did not find any associated dll for ${IMPLIB}.")
                else()
                    set(${DEST} "${DEST}-NOTFOUND" PARENT_SCOPE)
                    return()
                endif()
            endif()
            set(result "${CMAKE_MATCH_1}")
            set(${DEST} "${result}" PARENT_SCOPE)
        else()
            message(FATAL_ERROR "Cannot find dumpbin, please set CMAKE_DUMPBIN cmake variable")
        endif()
    else()
        if (NOT ARGS_NOTFATAL)
            message(FATAL_ERROR "Don't know how to identify dll from import library. Set CMAKE_DLLTOOL (for mingw) or CMAKE_DUMPBIN (for MSVC)")
        else()
            set(${DEST} "${DEST}-NOTFOUND")
        endif()
    endif()
endfunction()

function(target_get_dynamic_library DEST TARGET)
    set(result)
    get_target_property(alias "${TARGET}" ALIASED_TARGET)
    while (alias)
        set(TARGET "${alias}")
        get_target_property(alias "${TARGET}" ALIASED_TARGET)
    endwhile()
    if (WIN32)
        # Use the target dll of the import library
        set(props_to_check IMPORTED_IMPLIB)
        if (CMAKE_BUILD_TYPE)
            list(APPEND props_to_check IMPORTED_IMPLIB_${CMAKE_BUILD_TYPE})
        endif()
        list(APPEND props_to_check IMPORTED_LOCATION)
        if (CMAKE_BUILD_TYPE)
            list(APPEND props_to_check IMPORTED_LOCATION_${CMAKE_BUILD_TYPE})
        endif()
        foreach (config_type ${CMAKE_CONFIGURATION_TYPES} RELEASE DEBUG RELWITHDEBINFO MINSIZEREL)
            list(APPEND props_to_check IMPORTED_IMPLIB_${config_type})
            list(APPEND props_to_check IMPORTED_LOCATION_${config_type})
        endforeach()

        foreach(prop_to_check ${props_to_check})
            if (NOT result)
                get_target_property(propvalue "${TARGET}" ${prop_to_check})
                if (propvalue AND EXISTS "${propvalue}")
                    win32_implib_identify_dll(result "${propvalue}" NOTFATAL)
                endif()
            endif()
        endforeach()
    else()
        # 1. find the target library a file might be symbolic linking to
        # 2. find all other files in the same folder that symolic link to it
        # 3. sort all these files, and select the 2nd item
        set(props_to_check IMPORTED_LOCATION)
        if (CMAKE_BUILD_TYPE)
            list(APPEND props_to_check IMPORTED_LOCATION_${CMAKE_BUILD_TYPE})
        endif()
        foreach (config_type ${CMAKE_CONFIGURATION_TYPES} RELEASE DEBUG RELWITHDEBINFO MINSIZEREL)
            list(APPEND props_to_check IMPORTED_LOCATION_${config_type})
        endforeach()
        foreach(prop_to_check ${props_to_check})
            if (NOT result)
                get_target_property(propvalue "${TARGET}" ${prop_to_check})
                if (EXISTS "${propvalue}")
                    while (IS_SYMLINK "${propvalue}")
                        read_absolute_symlink(propvalue "${propvalue}")
                    endwhile()
                    get_filename_component(libdir "${propvalue}" DIRECTORY)
                    file(GLOB subfiles "${libdir}/*")
                    set(similar_files "${propvalue}")
                    foreach(subfile ${subfiles})
                        if (IS_SYMLINK "${subfile}")
                            read_absolute_symlink(subfile_target "${subfile}")
                            if (subfile_target STREQUAL propvalue)
                                list(APPEND similar_files "${subfile}")
                            endif()
                        endif()
                    endforeach()
                    list(SORT similar_files)
                    list(LENGTH similar_files eq_length)
                    if (eq_length GREATER 1)
                        list(GET similar_files 1 item)
                    else()
                        list(GET similar_files 0 item)
                    endif()
                    get_filename_component(result "${item}" NAME)
                endif()
            endif()
        endforeach()
    endif()
    if (NOT result)
        set (result "$<TARGET_FILE_NAME:${TARGET}>")
    endif()
    set(${DEST} ${result} PARENT_SCOPE)
endfunction()

macro(sdl_check_project_in_subfolder relative_subfolder name vendored_option)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/${relative_subfolder}/CMakeLists.txt")
        message(FATAL_ERROR "No cmake project for ${name} found in ${relative_subfolder}.\n"
            "Run the download script in the external folder, or re-configure with -D${vendored_option}=OFF to use system packages.")
    endif()
endmacro()

macro(sdl_check_linker_flag flag var)
    # FIXME: Use CheckLinkerFlag module once cmake minimum version >= 3.18
    include(CMakePushCheckState)
    include(CheckCSourceCompiles)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_LINK_OPTIONS "-Wl,--no-undefined")
    check_c_source_compiles("int main() { return 0; }" ${var})
    cmake_pop_check_state()
endmacro()

function(sdl_target_link_options_no_undefined TARGET)
    if(NOT MSVC)
        if(CMAKE_C_COMPILER_ID MATCHES "AppleClang")
            target_link_options(${TARGET} PRIVATE "-Wl,-undefined,error")
        else()
            sdl_check_linker_flag("-Wl,--no-undefined" HAVE_WL_NO_UNDEFINED)
            if(HAVE_WL_NO_UNDEFINED)
                target_link_options(${TARGET} PRIVATE "-Wl,--no-undefined")
            endif()
        endif()
    endif()
endfunction()
