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

if (CMAKE_BUILD_TYPE STREQUAL Debug)
	add_compile_options(-Og)
endif()

if (CMAKE_BUILD_TYPE STREQUAL Release)
	add_compile_options(-O3)
endif()

add_compile_options(-fno-pic)
add_link_options(-no-pie)

option(BUILD_QT_SDL "Build Qt/SDL frontend" ON)

add_subdirectory(src)

if (BUILD_QT_SDL)
	add_subdirectory(src/frontend/qt_sdl)
endif()

configure_file(
	${CMAKE_SOURCE_DIR}/romlist.bin
	${CMAKE_BINARY_DIR}/romlist.bin COPYONLY)
