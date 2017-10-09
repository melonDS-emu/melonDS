cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_CXX_STANDARD 11)

SET(PROJECT_WX melonDS)
PROJECT(${PROJECT_WX})

SET(SOURCES
	src/libui_sdl/main.cpp
	src/libui_sdl/Platform.cpp
	src/libui_sdl/DlgEmuSettings.cpp
	src/libui_sdl/DlgInputConfig.cpp
	src/ARM.cpp
	src/ARMInterpreter.cpp
	src/ARMInterpreter_ALU.cpp
	src/ARMInterpreter_Branch.cpp
	src/ARMInterpreter_LoadStore.cpp
	src/Config.cpp
	src/CP15.cpp
	src/DMA.cpp
	src/GPU.cpp
	src/GPU2D.cpp
	src/GPU3D.cpp
	src/GPU3D_Soft.cpp
	src/NDS.cpp
	src/NDSCart.cpp
	src/RTC.cpp
	src/SPI.cpp
	src/SPU.cpp
	src/Wifi.cpp
	src/libui_sdl/libui/common/areaevents.c
	src/libui_sdl/libui/common/control.c
	src/libui_sdl/libui/common/debug.c
	src/libui_sdl/libui/common/matrix.c
	src/libui_sdl/libui/common/shouldquit.c
	src/libui_sdl/libui/common/userbugs.c
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
	src/libui_sdl/libui/unix/window.c)

if (UNIX)
    FIND_PACKAGE(PkgConfig REQUIRED)
    PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

    INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
    LINK_LIBRARIES(${GTK3_LIBRARIES})

    ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        LINK_LIBRARIES("dl")
    endif ()
endif (UNIX)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
link_libraries(${SDL2_LIBRARIES})

add_executable(${PROJECT_WX} ${SOURCES})
target_link_libraries(${PROJECT_WX})

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE EQUAL Debug)
set(CMAKE_CXX_FLAGS "-g")
endif()
if(CMAKE_BUILD_TYPE EQUAL Release)
set(CMAKE_CXX_FLAGS "-O3 -flto")
endif()

install(TARGETS ${PROJECT_WX} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
