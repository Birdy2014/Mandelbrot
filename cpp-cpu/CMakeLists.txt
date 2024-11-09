cmake_minimum_required(VERSION 3.14)
project(Mandelbrot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mavx -O0 -Wall -Wextra -fdiagnostics-color=always")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx -O3")

add_executable(Mandelbrot ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/wayland.cpp)

find_library(LIBRARY_wayland wayland-client REQUIRED)
target_link_libraries(Mandelbrot ${LIBRARY_wayland})

find_library(LIBRARY_wayland-cursor wayland-cursor REQUIRED)
target_link_libraries(Mandelbrot ${LIBRARY_wayland-cursor})

find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_get_variable(WAYLAND_PROTOCOLS wayland-protocols pkgdatadir)

if (WAYLAND_PROTOCOLS STREQUAL "")
  message(FATAL_ERROR "wayland-protocols not found")
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
foreach(protocol "stable/xdg-shell/xdg-shell.xml" "unstable/xdg-decoration/xdg-decoration-unstable-v1.xml")
  get_filename_component(protocol_filename ${protocol} NAME_WLE)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/${protocol_filename}.h"
    COMMAND ${WAYLAND_SCANNER} client-header < "${WAYLAND_PROTOCOLS}/${protocol}" > "${CMAKE_CURRENT_BINARY_DIR}/generated/${protocol_filename}.h"
    VERBATIM
  )
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/${protocol_filename}.c"
    COMMAND ${WAYLAND_SCANNER} private-code < "${WAYLAND_PROTOCOLS}/${protocol}" > "${CMAKE_CURRENT_BINARY_DIR}/generated/${protocol_filename}.c"
    VERBATIM
  )
  target_sources(Mandelbrot PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/${protocol_filename}.h")
  target_sources(Mandelbrot PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated/${protocol_filename}.c")
endforeach()
target_include_directories(Mandelbrot SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

install(TARGETS Mandelbrot)
