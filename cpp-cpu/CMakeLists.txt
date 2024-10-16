cmake_minimum_required(VERSION 3.14)
project(Mandelbrot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Debug")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mavx -O0 -Wall -Wextra -fdiagnostics-color=always")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx -O3")

add_executable(Mandelbrot ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/wayland.cpp)

target_sources(Mandelbrot PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/xdg-shell.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/generated/xdg-decoration.c
)

# TODO: Generate wayland protocol files automatically

include(FetchContent)

find_library(LIBRARY_wayland wayland-client)
target_link_libraries(Mandelbrot ${LIBRARY_wayland})

find_library(LIBRARY_wayland-cursor wayland-cursor)
target_link_libraries(Mandelbrot ${LIBRARY_wayland-cursor})
