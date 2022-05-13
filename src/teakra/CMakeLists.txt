cmake_minimum_required(VERSION 3.8)
project(teakra CXX)

# Determine if we're built as a subproject (using add_subdirectory)
# or if this is the master project.
set(MASTER_PROJECT OFF)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(MASTER_PROJECT ON)
endif()

option(TEAKRA_WARNINGS_AS_ERRORS "Warnings as errors" ${MASTER_PROJECT})
option(TEAKRA_BUILD_TOOLS "Build tools" ${MASTER_PROJECT})
option(TEAKRA_BUILD_UNIT_TESTS "Build unit tests" ${MASTER_PROJECT})
option(TEAKRA_RUN_TESTS "Run Teakra accuracy tests" OFF)

# Set hard requirements for C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warn on CMake API deprecations
set(CMAKE_WARN_DEPRECATED ON)

# Disable in-source builds
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(SEND_ERROR "In-source builds are not allowed.")
endif()

# Add the module directory to the list of paths
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMakeModules")

# Compiler flags
if (MSVC)
    set(TEAKRA_CXX_FLAGS
        /std:c++latest # CMAKE_CXX_STANDARD as no effect on MSVC until CMake 3.10.
        /W3
        /permissive- # Stricter C++ standards conformance
        /MP
        /Zi
        /Zo
        /EHsc
        /Zc:throwingNew # Assumes new never returns null
        /Zc:inline # Omits inline functions from object-file output
        /DNOMINMAX
        /D_CRT_SECURE_NO_WARNINGS)

    if (TEAKRA_WARNINGS_AS_ERRORS)
        list(APPEND TEAKRA_CXX_FLAGS
             /WX)
    endif()
else()
    set(TEAKRA_CXX_FLAGS
        -Wall
        -Wextra
        -Wcast-qual
        -pedantic
        -pedantic-errors
        -Wfatal-errors
        -Wno-error=maybe-uninitialized
        -Wno-missing-braces
        -Wno-unused-parameter)

    if (TEAKRA_WARNINGS_AS_ERRORS)
        list(APPEND TEAKRA_CXX_FLAGS
             -Werror)
    endif()
endif()

# Prefer the -pthread flag on Linux.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

enable_testing()

if (NOT TEAKRA_TEST_ASSETS_DIR)
  set(TEAKRA_TEST_ASSETS_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif()

# External libraries
add_subdirectory(externals)

# Teakra project files
add_subdirectory(src)

# Teakra tests
if (TEAKRA_BUILD_UNIT_TESTS)
  add_subdirectory(tests)
endif()
