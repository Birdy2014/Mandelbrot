cmake_minimum_required(VERSION 3.14)
project(Mandelbrot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Debug")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Wall -fdiagnostics-color=always -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

add_executable(Mandelbrot ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

if(WIN32)
    target_link_libraries(Mandelbrot glfw opengl32 assimp freetype)
else()
    target_link_libraries(Mandelbrot glfw GL X11 pthread Xrandr Xi dl assimp freetype)
endif()

target_sources(Mandelbrot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/src/glad.c
)

target_include_directories(Mandelbrot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/include
)

include(FetchContent)

find_path(LIBRARY_GLM glm)
if(LIBRARY_GLM STREQUAL "LIBRARY_GLM-NOTFOUND")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG        master
    )

    FetchContent_MakeAvailable(glm)
    target_include_directories(Mandelbrot PUBLIC ${glm_SOURCE_DIR})
else()
    target_include_directories(Mandelbrot PUBLIC ${LIBRARY_GLM})
endif()

find_library(LIBRARY_GLFW glfw)
if (NOT LIBRARY_GLFW)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG        3.3-stable
    )

    FetchContent_MakeAvailable(glfw)
endif()

