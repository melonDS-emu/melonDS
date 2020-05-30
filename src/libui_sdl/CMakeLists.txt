project(libui_sdl)

SET(SOURCES_LIBUI
	main.cpp
	Platform.cpp
	PlatformConfig.cpp
	LAN_Socket.cpp
	LAN_PCap.cpp
	DlgAudioSettings.cpp
	DlgEmuSettings.cpp
	DlgInputConfig.cpp
	DlgVideoSettings.cpp
	DlgWifiSettings.cpp
	OSD.cpp
)

if (WIN32)
	set(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -i <SOURCE> -o <OBJECT>")
endif()

option(BUILD_SHARED_LIBS "Whether to build libui as a shared library or a static library" ON)
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(libui)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
#string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)

add_executable(melonDS ${SOURCES_LIBUI})
target_link_libraries(melonDS
	core ${SDL2_LIBRARIES} libui)

if (UNIX)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
	pkg_check_modules(SDL2 REQUIRED sdl2)

	target_include_directories(melonDS
		PRIVATE ${GTK3_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS}
	)
	target_link_libraries(melonDS ${GTK3_LIBRARIES} ${SDL2_LIBRARIES})

	ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

	add_custom_command(OUTPUT melon_grc.c
		COMMAND glib-compile-resources --sourcedir=${CMAKE_SOURCE_DIR}
				--target=${CMAKE_CURRENT_BINARY_DIR}/melon_grc.c
				--generate-source "${CMAKE_SOURCE_DIR}/melon_grc.xml"
		COMMAND glib-compile-resources --sourcedir=${CMAKE_SOURCE_DIR}
				--target=${CMAKE_CURRENT_BINARY_DIR}/melon_grc.h
				--generate-header "${CMAKE_SOURCE_DIR}/melon_grc.xml")

	if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_link_libraries(melonDS dl)
	endif ()

	target_sources(melonDS PUBLIC melon_grc.c)
elseif (WIN32)
	target_sources(melonDS PUBLIC "${CMAKE_SOURCE_DIR}/melon.rc")
	target_include_directories(melonDS PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
	target_link_libraries(melonDS comctl32 d2d1 dwrite uxtheme ws2_32 iphlpapi)
endif ()

install(TARGETS melonDS RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
