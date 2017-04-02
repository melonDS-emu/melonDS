cmake_minimum_required(VERSION 2.8.11)

SET(PROJECT_WX melonDS)
PROJECT(${PROJECT_WX})
SET(APP_VERSION 0.1)
SET(INSTALL_PATH /usr/local/bin/)

SET(VERSION ${APP_VERSION})

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.cmake
			   ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h)

SET(SOURCES
	src/wx/main.cpp
	src/wx/EmuConfig.cpp
	src/wx/InputConfig.cpp
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
	src/Wifi.cpp)

find_package(GTK2 2.0 REQUIRED)
include_directories(${GTK2_INCLUDE_DIRS})

find_package(wxWidgets REQUIRED)
include(${wxWidgets_USE_FILE})
link_libraries(${wxWidgets_LIBRARIES})

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
link_libraries(${SDL2_LIBRARIES})

add_executable(${PROJECT_WX} ${SOURCES})
target_link_libraries(${PROJECT_WX})

install(TARGETS ${PROJECT_WX} DESTINATION ${INSTALL_PATH})