cmake_minimum_required(VERSION 3.13)

cmake_policy(VERSION 3.13)
if (POLICY CMP0076)
    cmake_policy(SET CMP0076 NEW)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

project(melonDS)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if (CMAKE_BUILD_TYPE STREQUAL Release)
	option(ENABLE_LTO "Enable link-time optimization" ON)
else()
	option(ENABLE_LTO "Enable link-time optimization" OFF)
endif()

if(ENABLE_LTO)
	add_compile_options(-O3 -flto)
	set(CMAKE_AR "gcc-ar")
	set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
	set(CMAKE_C_ARCHIVE_FINISH   true)
	set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
	set(CMAKE_CXX_ARCHIVE_FINISH   true)
endif()

add_compile_options(-fno-pic)
add_link_options(-no-pie)

option(BUILD_LIBUI "Build libui frontend" ON)

add_subdirectory(src)

if (BUILD_LIBUI)
	add_subdirectory(src/libui_sdl)
endif()

configure_file(
	${CMAKE_SOURCE_DIR}/romlist.bin
	${CMAKE_BINARY_DIR}/romlist.bin COPYONLY)
