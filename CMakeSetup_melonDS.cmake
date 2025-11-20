cmake_minimum_required(VERSION 3.16)

# -----------------------------
# Project Information
# -----------------------------
project(melonDS
    VERSION 1.1
    DESCRIPTION "A modernized build setup for the melonDS emulator"
    LANGUAGES C CXX
)

# -----------------------------
# Global Settings
# -----------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# -----------------------------
# Architecture Detection
# -----------------------------
function(detect_arch flag arch_name)
    check_symbol_exists("${flag}" "" FOUND_${arch_name})
    if (FOUND_${arch_name})
        set(ARCHITECTURE ${arch_name} CACHE STRING "Detected Architecture" FORCE)
        add_compile_definitions(ARCHITECTURE_${arch_name}=1)
    endif()
endfunction()

detect_arch("__x86_64__" X86_64)
detect_arch("__i386__"   X86)
detect_arch("__arm__"    ARM)
detect_arch("__aarch64__" ARM64)

# -----------------------------
# Options
# -----------------------------
option(ENABLE_OGLRENDERER "Enable OpenGL Renderer" ON)
option(ENABLE_GDBSTUB "Enable GDB Stub Debugging" ON)
option(BUILD_QT_SDL "Build Qt/SDL Frontend" ON)
option(ENABLE_LTO "Enable Link Time Optimization" OFF)

# JIT Support
if (ARCHITECTURE STREQUAL "X86_64" OR ARCHITECTURE STREQUAL "ARM64")
    option(ENABLE_JIT "Enable JIT Recompiler" ON)
else()
    option(ENABLE_JIT "Enable JIT Recompiler" OFF)
endif()

if (ENABLE_JIT)
    option(ENABLE_JIT_PROFILING "Enable VTune Profiling" OFF)
endif()

if (ENABLE_GDBSTUB)
    add_compile_definitions(GDBSTUB_ENABLED)
endif()

# -----------------------------
# LTO (Interprocedural Optimization)
# -----------------------------
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_ok)

if(ipo_ok AND ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# -----------------------------
# Platform Specific Options
# -----------------------------
if (WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# -----------------------------
# Subdirectories
# -----------------------------
add_subdirectory(src)

if (BUILD_QT_SDL)
    add_subdirectory(src/frontend/qt_sdl)
endif()
