cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_CXX_STANDARD 11)

SET(PROJECT_WX melonDS)
PROJECT(${PROJECT_WX})

SET(SOURCES
	src/libui_sdl/main.cpp
	src/libui_sdl/Platform.cpp
	src/libui_sdl/DlgAudioSettings.cpp
	src/libui_sdl/DlgEmuSettings.cpp
	src/libui_sdl/DlgInputConfig.cpp
	src/ARM.cpp
	src/ARMInterpreter.cpp
	src/ARMInterpreter_ALU.cpp
	src/ARMInterpreter_Branch.cpp
	src/ARMInterpreter_LoadStore.cpp
	src/Config.cpp
	src/CP15.cpp
	src/CRC32.cpp
	src/DMA.cpp
	src/GPU.cpp
	src/GPU2D.cpp
	src/GPU3D.cpp
	src/GPU3D_Soft.cpp
	src/melon_fopen.cpp
	src/NDS.cpp
	src/NDSCart.cpp
	src/RTC.cpp
	src/Savestate.cpp
	src/SPI.cpp
	src/SPU.cpp
	src/Wifi.cpp
	src/WifiAP.cpp
	src/libui_sdl/libui/common/areaevents.c
	src/libui_sdl/libui/common/control.c
	src/libui_sdl/libui/common/debug.c
	src/libui_sdl/libui/common/matrix.c
	src/libui_sdl/libui/common/shouldquit.c
	src/libui_sdl/libui/common/userbugs.c
)

if (UNIX)
    LIST(APPEND SOURCES
        src/libui_sdl/libui/unix/alloc.c
        src/libui_sdl/libui/unix/area.c
        src/libui_sdl/libui/unix/box.c
        src/libui_sdl/libui/unix/button.c
        src/libui_sdl/libui/unix/cellrendererbutton.c
        src/libui_sdl/libui/unix/checkbox.c
        src/libui_sdl/libui/unix/child.c
        src/libui_sdl/libui/unix/colorbutton.c
        src/libui_sdl/libui/unix/combobox.c
        src/libui_sdl/libui/unix/control.c
        src/libui_sdl/libui/unix/datetimepicker.c
        src/libui_sdl/libui/unix/debug.c
        src/libui_sdl/libui/unix/draw.c
        src/libui_sdl/libui/unix/drawmatrix.c
        src/libui_sdl/libui/unix/drawpath.c
        src/libui_sdl/libui/unix/drawtext.c
        src/libui_sdl/libui/unix/editablecombo.c
        src/libui_sdl/libui/unix/entry.c
        src/libui_sdl/libui/unix/fontbutton.c
        src/libui_sdl/libui/unix/form.c
        src/libui_sdl/libui/unix/future.c
        src/libui_sdl/libui/unix/graphemes.c
        src/libui_sdl/libui/unix/grid.c
        src/libui_sdl/libui/unix/group.c
        src/libui_sdl/libui/unix/image.c
        src/libui_sdl/libui/unix/label.c
        src/libui_sdl/libui/unix/main.c
        src/libui_sdl/libui/unix/menu.c
        src/libui_sdl/libui/unix/multilineentry.c
        src/libui_sdl/libui/unix/progressbar.c
        src/libui_sdl/libui/unix/radiobuttons.c
        src/libui_sdl/libui/unix/separator.c
        src/libui_sdl/libui/unix/slider.c
        src/libui_sdl/libui/unix/spinbox.c
        src/libui_sdl/libui/unix/stddialogs.c
        src/libui_sdl/libui/unix/tab.c
        src/libui_sdl/libui/unix/text.c
        src/libui_sdl/libui/unix/util.c
        src/libui_sdl/libui/unix/window.c
        melon_grc.c
    )

    FIND_PACKAGE(PkgConfig REQUIRED)
    PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)
    PKG_CHECK_MODULES(SDL2 REQUIRED sdl2)

    INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
    LINK_LIBRARIES(${GTK3_LIBRARIES} ${SDL2_LIBRARIES})

    ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

    add_custom_command(OUTPUT melon_grc.c
        COMMAND glib-compile-resources --sourcedir="${CMAKE_CURRENT_SOURCE_DIR}"
                --target="${CMAKE_CURRENT_BINARY_DIR}/melon_grc.c"
                --generate-source "${CMAKE_CURRENT_SOURCE_DIR}/melon_grc.xml"
        COMMAND glib-compile-resources --sourcedir="${CMAKE_CURRENT_SOURCE_DIR}"
                --target="${CMAKE_CURRENT_BINARY_DIR}/melon_grc.h"
                --generate-header "${CMAKE_CURRENT_SOURCE_DIR}/melon_grc.xml")

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        LINK_LIBRARIES("dl")
    endif ()
elseif (WIN32)
    LIST(APPEND SOURCES
        src/libui_sdl/libui/windows/alloc.cpp
        src/libui_sdl/libui/windows/area.cpp
        src/libui_sdl/libui/windows/areadraw.cpp
        src/libui_sdl/libui/windows/areaevents.cpp
        src/libui_sdl/libui/windows/areascroll.cpp
        src/libui_sdl/libui/windows/areautil.cpp
        src/libui_sdl/libui/windows/box.cpp
        src/libui_sdl/libui/windows/button.cpp
        src/libui_sdl/libui/windows/checkbox.cpp
        src/libui_sdl/libui/windows/colorbutton.cpp
        src/libui_sdl/libui/windows/colordialog.cpp
        src/libui_sdl/libui/windows/combobox.cpp
        src/libui_sdl/libui/windows/container.cpp
        src/libui_sdl/libui/windows/control.cpp
        src/libui_sdl/libui/windows/d2dscratch.cpp
        src/libui_sdl/libui/windows/datetimepicker.cpp
        src/libui_sdl/libui/windows/debug.cpp
        src/libui_sdl/libui/windows/draw.cpp
        src/libui_sdl/libui/windows/drawmatrix.cpp
        src/libui_sdl/libui/windows/drawpath.cpp
        src/libui_sdl/libui/windows/drawtext.cpp
        src/libui_sdl/libui/windows/dwrite.cpp
        src/libui_sdl/libui/windows/editablecombo.cpp
        src/libui_sdl/libui/windows/entry.cpp
        src/libui_sdl/libui/windows/events.cpp
        src/libui_sdl/libui/windows/fontbutton.cpp
        src/libui_sdl/libui/windows/fontdialog.cpp
        src/libui_sdl/libui/windows/form.cpp
        src/libui_sdl/libui/windows/graphemes.cpp
        src/libui_sdl/libui/windows/grid.cpp
        src/libui_sdl/libui/windows/group.cpp
        src/libui_sdl/libui/windows/init.cpp
        src/libui_sdl/libui/windows/label.cpp
        src/libui_sdl/libui/windows/main.cpp
        src/libui_sdl/libui/windows/menu.cpp
        src/libui_sdl/libui/windows/multilineentry.cpp
        src/libui_sdl/libui/windows/parent.cpp
        src/libui_sdl/libui/windows/progressbar.cpp
        src/libui_sdl/libui/windows/radiobuttons.cpp
        src/libui_sdl/libui/windows/separator.cpp
        src/libui_sdl/libui/windows/sizing.cpp
        src/libui_sdl/libui/windows/slider.cpp
        src/libui_sdl/libui/windows/spinbox.cpp
        src/libui_sdl/libui/windows/stddialogs.cpp
        #src/libui_sdl/libui/windows/tab.cpp
        #src/libui_sdl/libui/windows/tabpage.cpp
        src/libui_sdl/libui/windows/text.cpp
        src/libui_sdl/libui/windows/utf16.cpp
        src/libui_sdl/libui/windows/utilwin.cpp
        src/libui_sdl/libui/windows/window.cpp
        src/libui_sdl/libui/windows/winpublic.cpp
        src/libui_sdl/libui/windows/winutil.cpp
        src/libui_sdl/libui/windows/resources.rc
		melon.rc
    )

    LINK_LIBRARIES("comctl32")
    LINK_LIBRARIES("d2d1")
    LINK_LIBRARIES("dwrite")
    LINK_LIBRARIES("usp10")
    LINK_LIBRARIES("ws2_32")
    LINK_LIBRARIES("uxtheme")
endif (UNIX)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
link_libraries(${SDL2_LIBRARIES})

add_executable(${PROJECT_WX} ${SOURCES})
target_link_libraries(${PROJECT_WX})

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL Debug)
set(CMAKE_CXX_FLAGS "-g")
elseif(CMAKE_BUILD_TYPE STREQUAL Release)
set(CMAKE_CXX_FLAGS "-O3 -flto")
endif()

install(TARGETS ${PROJECT_WX} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
