include(FetchContent)

set(_DEFAULT_VCPKG_ROOT "${CMAKE_SOURCE_DIR}/vcpkg")
set(VCPKG_ROOT "${_DEFAULT_VCPKG_ROOT}" CACHE STRING "The path to the vcpkg repository")

if (VCPKG_ROOT STREQUAL "${_DEFAULT_VCPKG_ROOT}")
    file(LOCK "${_DEFAULT_VCPKG_ROOT}" DIRECTORY GUARD FILE)
    FetchContent_Declare(vcpkg
        GIT_REPOSITORY "https://github.com/Microsoft/vcpkg.git"
        GIT_TAG 2024.01.12
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/vcpkg")
    FetchContent_MakeAvailable(vcpkg)
endif()

set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_SOURCE_DIR}/cmake/overlay-triplets")

option(USE_RECOMMENDED_TRIPLETS "Use the recommended triplets that are used for official builds" ON)

if (CMAKE_OSX_ARCHITECTURES MATCHES ";")
    message(FATAL_ERROR "macOS universal builds are not supported. Build them individually and combine afterwards instead.")
endif()

if (USE_RECOMMENDED_TRIPLETS)
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE _HOST_PROCESSOR
        OUTPUT_STRIP_TRAILING_WHITESPACE)

    set(_CAN_TARGET_AS_HOST OFF)

    if (APPLE)
        if (NOT CMAKE_OSX_ARCHITECTURES)
            if (_HOST_PROCESSOR STREQUAL arm64)
                set(CMAKE_OSX_ARCHITECTURES arm64)
            else()
                set(CMAKE_OSX_ARCHITECTURES x86_64)
            endif()
        endif()

        if (CMAKE_OSX_ARCHITECTURES STREQUAL arm64)
            set(_WANTED_TRIPLET arm64-osx-11-release)
            set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
        else()
            set(_WANTED_TRIPLET x64-osx-1015-release)
            set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15)
        endif()
    elseif(WIN32)
        # TODO Windows arm64 if possible
        set(_CAN_TARGET_AS_HOST ON)
        set(_WANTED_TRIPLET x64-mingw-static)
    endif()

    # Don't override it if the user set something else
    if (NOT VCPKG_TARGET_TRIPLET)
        set(VCPKG_TARGET_TRIPLET "${_WANTED_TRIPLET}")
    else()
        set(_WANTED_TRIPLET "${VCPKG_TARGET_TRIPLET}")
    endif()

    if (APPLE)
        if (_HOST_PROCESSOR MATCHES arm64)
            if (_WANTED_TRIPLET MATCHES "^arm64-osx-")
                set(_CAN_TARGET_AS_HOST ON)
            elseif (_WANTED_TRIPLET STREQUAL "x64-osx-1015-release")
                # Use the default triplet for when building for arm64
                # because we're probably making a universal build
                set(VCPKG_HOST_TRIPLET arm64-osx-11-release)
            endif()
        else()
            if (_WANTED_TRIPLET MATCHES "^x64-osx-")
                set(_CAN_TARGET_AS_HOST ON)
            elseif (_WANTED_TRIPLET STREQUAL "arm64-osx-11-release")
                set(VCPKG_HOST_TRIPLET x64-osx-1015-release)
            endif()
        endif()
    endif()

    # If host and target triplet differ, vcpkg seems to always assume that the host can't run the target's binaries.
    # In cases like cross compiling from ARM -> Intel macOS, or target being an older version of the host OS, we *can* do that so the packages built targeting the host are redundant.
    if (_CAN_TARGET_AS_HOST AND NOT VCPKG_HOST_TRIPLET)
        option(VCPKG_TARGET_AS_HOST "Use the target as host triplet to speed up builds" ON)
    else()
        option(VCPKG_TARGET_AS_HOST "Use the target as host triplet to speed up builds" OFF)
    endif()

    if (VCPKG_TARGET_AS_HOST)
        set(VCPKG_HOST_TRIPLET "${VCPKG_TARGET_TRIPLET}" CACHE STRING "Host triplet to use for vcpkg")
    endif()
endif()

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
