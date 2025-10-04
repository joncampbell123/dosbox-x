cmake_minimum_required(VERSION 3.1)
project(opencow)

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "-Wall")
    set(CMAKE_CXX_FLAGS_DEBUG "-g3")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -s")
    set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "-shared -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup")
endif()

file(GLOB srcs ${CMAKE_SOURCE_DIR}/src/*)

add_library(opencow SHARED ${srcs})
target_include_directories(opencow PRIVATE ${CMAKE_SOURCE_DIR}/src)
set_target_properties(opencow PROPERTIES PREFIX "")
