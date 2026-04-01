if (CMAKE_C_COMPILER_ID STREQUAL GNU)
	set(CMAKE_C_FLAGS_DEBUG_INIT "-g -Og")
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
	set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -Og")
endif()

string(REPLACE "-O2" "-O3" CMAKE_C_FLAGS_RELEASE_INIT "${CMAKE_C_FLAGS_RELEASE_INIT}")
string(REPLACE "-O2" "-O3" CMAKE_CXX_FLAGS_RELEASE_INIT "${CMAKE_CXX_FLAGS_RELEASE_INIT}")

# -march=native: use all CPU instructions available on the build machine (SSE4, AVX2, etc.)
# Only apply for native (non-cross) builds on GCC and Clang.
# This produces a binary that is NOT portable to other machines — intended for local builds only.
if (NOT CMAKE_CROSSCOMPILING)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_C_FLAGS_RELEASE_INIT "${CMAKE_C_FLAGS_RELEASE_INIT} -march=native")
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS_RELEASE_INIT "${CMAKE_CXX_FLAGS_RELEASE_INIT} -march=native")
    endif()
endif()

# -fno-semantic-interposition: allows the compiler to inline and devirtualize
# across translation units in shared libraries (Linux/ELF only).
# Improves inlining of hot paths like memory dispatch in NDS.cpp.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT APPLE AND NOT WIN32)
    set(CMAKE_CXX_FLAGS_RELEASE_INIT "${CMAKE_CXX_FLAGS_RELEASE_INIT} -fno-semantic-interposition")
    set(CMAKE_C_FLAGS_RELEASE_INIT   "${CMAKE_C_FLAGS_RELEASE_INIT}   -fno-semantic-interposition")
endif()

# Building with LTO causes an internal compiler error in GCC 15.0+
if (CMAKE_CXX_COMPILER_ID STREQUAL GNU AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15.0)
	set(ENABLE_LTO_RELEASE OFF CACHE BOOL "Enable LTO for release builds" FORCE)
	set(ENABLE_LTO OFF CACHE BOOL "Enable LTO" FORCE)
endif()
