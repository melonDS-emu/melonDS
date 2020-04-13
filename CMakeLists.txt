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

option(BUILD_FRONTEND "Build frontend" OFF)
if(BUILD_FRONTEND)
	add_definitions(-DBUILD_FRONTEND)
endif()

add_subdirectory(src)
add_subdirectory(src/libui_sdl)

# resource embedding - https://stackoverflow.com/a/56006001/4926282
set(RC_DEPENDS "")
function(add_resource input)
    string(MAKE_C_IDENTIFIER ${input} input_identifier)
    set(output "${input_identifier}.o")
    target_link_libraries(${PROJECT_NAME} ${CMAKE_BINARY_DIR}/${output})

	configure_file(${CMAKE_SOURCE_DIR}/${input} ${input} COPYONLY)

    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_LINKER} --relocatable --format binary --output ${output} ${input}
        DEPENDS ${input}
    )

    set(RC_DEPENDS ${RC_DEPENDS} ${output} PARENT_SCOPE)
endfunction()

# Resource file list
add_resource("romlist.bin")
add_resource("fakeBios7.bin")
add_resource("fakeBios9.bin")
add_custom_target(rc ALL DEPENDS ${RC_DEPENDS})
