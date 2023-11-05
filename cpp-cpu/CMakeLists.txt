cmake_minimum_required(VERSION 3.14)
project(Mandelbrot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Fix compile_commands.json under NixOS
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Debug")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mavx -O0 -Wall -Wextra -fdiagnostics-color=always")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx -O3")

add_executable(Mandelbrot ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

target_link_libraries(Mandelbrot minifb)

include(FetchContent)

find_library(LIBRARY_minifb minifb)
if (NOT LIBRARY_minifb)
    set(USE_WAYLAND_API OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        minifb
        GIT_REPOSITORY https://github.com/emoon/minifb
        GIT_TAG        master
    )

FetchContent_MakeAvailable(minifb)
endif()
