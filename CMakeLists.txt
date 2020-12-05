cmake_minimum_required(VERSION 3.13)

include(CheckSymbolExists)
include(CheckLibraryExists)

cmake_policy(VERSION 3.13)
if (POLICY CMP0076)
    cmake_policy(SET CMP0076 NEW)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")

project(melonDS CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

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

if (WIN32)
	option(BUILD_STATIC "Statically link dependencies" OFF)
endif()

if (ENABLE_LTO)
	if (WIN32)
		add_compile_options(-flto)
		add_link_options(-flto)
	else()
		add_compile_options(-flto -fPIC)
		add_link_options(-flto -fuse-linker-plugin -pie)
	endif()
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

option(BUILD_QT_SDL "Build Qt/SDL frontend" ON)

add_subdirectory(src)

if (BUILD_QT_SDL)
	add_subdirectory(src/frontend/qt_sdl)
endif()