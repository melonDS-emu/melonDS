cmake_minimum_required(VERSION 3.13)

cmake_policy(VERSION 3.13)
if (POLICY CMP0076)
    cmake_policy(SET CMP0076 NEW)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

project(melonDS)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(CheckSymbolExists)
function(detect_architecture symbol arch)
    if (NOT DEFINED ARCHITECTURE)
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        # The output variable needs to be unique across invocations otherwise
        # CMake's crazy scope rules will keep it defined
        if (ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
            set(ARCHITECTURE_${arch} 1 PARENT_SCOPE)
            add_definitions(-DARCHITECTURE_${arch}=1)
        endif()
    endif()
endfunction()

detect_architecture("__x86_64__" x86_64)
detect_architecture("__i386__" x86)
detect_architecture("__arm__" ARM)
detect_architecture("__aarch64__" ARM64)

if (ARCHITECTURE STREQUAL x86_64 OR ARCHITECTURE STREQUAL ARM64)
	option(ENABLE_JIT "Enable x64 JIT recompiler" ON)
endif()

if (ENABLE_JIT)
	add_definitions(-DJIT_ENABLED)
endif()

if (CMAKE_BUILD_TYPE STREQUAL Release)
	option(ENABLE_LTO "Enable link-time optimization" ON)
else()
	option(ENABLE_LTO "Enable link-time optimization" OFF)
endif()

option(ENABLE_OGLRENDERER "Enable OpenGL renderer" ON)

if (ENABLE_OGLRENDERER)
    add_definitions(-DOGLRENDERER_ENABLED)
endif()

if (CMAKE_BUILD_TYPE STREQUAL Debug)
	add_compile_options(-Og)
endif()

if (CMAKE_BUILD_TYPE STREQUAL Release)
	add_compile_options(-O3)
	add_link_options(-s)
endif()

add_compile_options(-fno-pic)
add_link_options(-no-pie)

option(BUILD_QT_SDL "Build Qt/SDL frontend" ON)

if (WIN32)
	option(BUILD_STATIC "Statically link dependencies" OFF)
endif()

add_subdirectory(src)

if (BUILD_QT_SDL)
	add_subdirectory(src/frontend/qt_sdl)
endif()
