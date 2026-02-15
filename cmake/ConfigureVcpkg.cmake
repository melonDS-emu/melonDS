set(_DEFAULT_VCPKG_ROOT "${CMAKE_SOURCE_DIR}/vcpkg")
set(VCPKG_ROOT "${_DEFAULT_VCPKG_ROOT}" CACHE STRING "The path to the vcpkg repository")

if (VCPKG_ROOT STREQUAL "${_DEFAULT_VCPKG_ROOT}")
    file(LOCK "${CMAKE_SOURCE_DIR}/.vcpkg.lock")

    find_package(Git)

    if (GIT_FOUND)
        if (NOT EXISTS "${_DEFAULT_VCPKG_ROOT}")
            execute_process(
                COMMAND ${GIT_EXECUTABLE} clone https://github.com/Microsoft/vcpkg.git "${_DEFAULT_VCPKG_ROOT}"
                RESULT_VARIABLE git_result)

            if (NOT git_result EQUAL 0)
                message(FATAL_ERROR "Git clone of vcpkg failed. Please provide vcpkg yourself at ${_VCPKG_DEFAULT_ROOT} or set VCPKG_ROOT.")
            endif()
        endif()
    endif()

    file(LOCK "${CMAKE_SOURCE_DIR}/.vcpkg.lock" RELEASE)
    file(REMOVE "${CMAKE_SOURCE_DIR}/.vcpkg.lock")
endif()

option(USE_RECOMMENDED_TRIPLETS "Use the recommended triplets that are used for official builds" ON)

# Required for Windows non-MinGW debug builds, optional for all other platforms
if (WIN32)
    if (DEFINED ENV{MINGW_PREFIX})
        option(ENABLE_DEBUG_DEPS "Enable debug builds of vcpkg dependencies" OFF)
    else()
        option(ENABLE_DEBUG_DEPS "Enable debug builds of vcpkg dependencies" ON)
    endif()
endif()

# Duplicated here because it needs to be set before project()
option(USE_QT6 "Use Qt 6 instead of Qt 5" ON)

# Since the Linux build pulls in glib anyway, we can just use upstream libslirp
if (UNIX AND NOT APPLE)
    option(USE_SYSTEM_LIBSLIRP "Use system libslirp instead of the bundled version" ON)
endif()

if (NOT USE_QT6)
    list(APPEND VCPKG_MANIFEST_FEATURES qt5)
    set(VCPKG_MANIFEST_NO_DEFAULT_FEATURES ON)
endif()

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
        set(_CAN_TARGET_AS_HOST ON)

        if (ENABLE_DEBUG_DEPS)
            set(_RELEASE_SUFFIX "")
        else()
            set(_RELEASE_SUFFIX "-release")
        endif()

        if (DEFINED ENV{MINGW_PREFIX})
            set(_WIN32_ENV mingw)
        else()
            set(_WIN32_ENV windows)
            set(_WIN32_ARCH "$ENV{VSCMD_ARG_TGT_ARCH}")
            if (NOT _WIN32_ARCH STREQUAL "$ENV{VSCMD_ARG_HOST_ARCH}")
                set(_CAN_TARGET_AS_HOST OFF)

                # HACK: CMake wants to use the arm64 pkg-config on x86_64 hosts, this won't work
                # so we gotta override the executable manually
                set(PKG_CONFIG_EXECUTABLE "${CMAKE_BINARY_DIR}/vcpkg_installed/$ENV{VSCMD_ARG_HOST_ARCH}-windows/tools/pkgconf/pkgconf.exe")
                # CMake does not correctly set the target arch for assembly when cross compiling for Windows
                set(CMAKE_ASM_FLAGS_INIT "${CMAKE_ASM_FLAGS_INIT} --target=${_WIN32_ARCH}-pc-windows-msvc")
            endif()
        endif()

	if (NOT _WIN32_ARCH)
            if ("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "AMD64")
                set(_WIN32_ARCH x64)
            elseif ("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "ARM64")
                set(_WIN32_ARCH arm64)
            elseif ("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "x86")
                set(_WIN32_ARCH x86)
            else()
                message(FATAL_ERROR "Unknown processor architecture '$ENV{PROCESSOR_ARCHITECTURE}'. Please disable USE_RECOMMENDED_TRIPLETS and set your vcpkg settings manually.")
            endif()
        endif()

        set(_WANTED_TRIPLET ${_WIN32_ARCH}-${_WIN32_ENV}-static${_RELEASE_SUFFIX})
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL Linux)
        # Can't really detect cross compiling here.
        set(_CAN_TARGET_AS_HOST ON)
        if (_HOST_PROCESSOR STREQUAL x86_64)
            set(_WANTED_TRIPLET x64-linux-release)
        elseif(_HOST_PROCESSOR STREQUAL "aarch64")
            set(_WANTED_TRIPLET arm64-linux-release)
        endif()
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
