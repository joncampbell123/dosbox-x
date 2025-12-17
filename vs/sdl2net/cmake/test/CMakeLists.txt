# This cmake build script is meant for verifying the various CMake configuration script.

cmake_minimum_required(VERSION 3.12)
project(sdl_test LANGUAGES C)

cmake_policy(SET CMP0074 NEW)

# Override CMAKE_FIND_ROOT_PATH_MODE to allow search for SDL2 outside of sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)

include(FeatureSummary)

option(TEST_SHARED "Test linking to shared SDL2_net library" ON)
add_feature_info("TEST_SHARED" TEST_SHARED "Test linking with shared library")

option(TEST_STATIC "Test linking to static SDL2_net libary" ON)
add_feature_info("TEST_STATIC" TEST_STATIC "Test linking with static library")

if(TEST_SHARED)
    # FIXME: in the distant future, must become REQUIRED
    find_package(SDL2 CONFIG COMPONENTS SDL2)
    # FIXME: and the following should be removed
    if(NOT TARGET SDL2::SDL2)
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/..")
        include(PrivateSdlFunctions)
        sdl_find_sdl2(SDL2::SDL2 2.0)
    endif()
    find_package(SDL2_net REQUIRED CONFIG)
    add_executable(main_shared main.c)
    target_link_libraries(main_shared PRIVATE SDL2::SDL2 SDL2_net::SDL2_net)
endif()

if(TEST_STATIC)
    # FIXME: in the distant future, must become REQUIRED
    find_package(SDL2 CONFIG COMPONENTS SDL2-static)
    # FIXME: and the following should be removed
    if(NOT TARGET SDL2::SDL2-static)
        list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/..")
        include(PrivateSdlFunctions)
        sdl_find_sdl2(SDL2::SDL2-static 2.0)
    endif()
    find_package(SDL2_net REQUIRED CONFIG)
    add_executable(main_static main.c)
    target_link_libraries(main_static PRIVATE SDL2::SDL2-static SDL2_net::SDL2_net-static)
endif()

feature_summary(WHAT ALL)
