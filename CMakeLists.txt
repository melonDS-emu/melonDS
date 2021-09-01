cmake_minimum_required(VERSION 3.13)

include(CheckSymbolExists)
include(CheckLibraryExists)

cmake_policy(VERSION 3.13)
if (POLICY CMP0076)
    cmake_policy(SET CMP0076 NEW)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum OS X deployment version")

project(melonDS CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(MELONDS_VERSION "0.9.3")
add_compile_definitions(MELONDS_VERSION="${MELONDS_VERSION}")
string(REPLACE "." ";" VERSION_LIST ${MELONDS_VERSION})
# For the melon.rc file used on Windows
list(GET VERSION_LIST 0 MELONDS_VERSION_MAJOR)
list(GET VERSION_LIST 1 MELONDS_VERSION_MINOR)
# Check if melonDS version is three digits or two digits
list(LENGTH VERSION_LIST MELONDS_VER_LEN)
if (${MELONDS_VER_LEN} GREATER 2) 
    list(GET VERSION_LIST 2 MELONDS_VERSION_PATCH)
else()
    set(MELONDS_VERSION_PATCH 0)
endif()


check_library_exists(m pow "" LIBM)
if(LIBM)
    link_libraries(m)
endif()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

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

	option(ENABLE_JIT_PROFILING "Enable JIT profiling with VTune" OFF)

	if (ENABLE_JIT_PROFILING)
		include(cmake/FindVTune.cmake)
		add_definitions(-DJIT_PROFILING_ENABLED)
	endif()
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
	if (NOT APPLE)
		add_link_options(-s)
	endif()
endif()

if (WIN32)
	option(BUILD_STATIC "Statically link dependencies" OFF)
endif()

if (BUILD_STATIC AND WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()

if (ENABLE_LTO)
	if (WIN32 OR APPLE)
		add_compile_options(-flto)
		add_link_options(-flto)
	else()
		add_compile_options(-flto -fPIC)
		add_link_options(-flto -fuse-linker-plugin -pie)
	endif()
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(CMAKE_AR "gcc-ar")
		set(CMAKE_RANLIB "gcc-ranlib")
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		find_program(LLD NAMES ld.lld ld64.lld lld-link)
		if (NOT LLD STREQUAL "LLD-NOTFOUND")
			add_link_options(-fuse-ld=lld)
		endif()
		set(CMAKE_AR "llvm-ar")
		set(CMAKE_RANLIB "llvm-ranlib")
	endif()
endif()

find_program(CCACHE "ccache")
if (CCACHE)
    message(STATUS "Using CCache to speed up compilation")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
endif()

option(BUILD_QT_SDL "Build Qt/SDL frontend" ON)

add_subdirectory(src)

if (BUILD_QT_SDL)
	add_subdirectory(src/frontend/qt_sdl)
endif()
