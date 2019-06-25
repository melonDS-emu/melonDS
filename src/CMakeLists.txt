project(core)

set (CMAKE_CXX_STANDARD 14)

add_library(core STATIC
	ARM.cpp
	ARMInterpreter.cpp
	ARMInterpreter_ALU.cpp
	ARMInterpreter_Branch.cpp
	ARMInterpreter_LoadStore.cpp
	ARM_InstrInfo.cpp
	Config.cpp
	CP15.cpp
	CRC32.cpp
	DMA.cpp
	GPU.cpp
	GPU2D.cpp
	GPU3D.cpp
	GPU3D_OpenGL.cpp
	GPU3D_Soft.cpp
	NDS.cpp
	NDSCart.cpp
	OpenGLSupport.cpp
	RTC.cpp
	Savestate.cpp
	SPI.cpp
	SPU.cpp
	Wifi.cpp
	WifiAP.cpp

	ARMJIT.cpp
	ARMJIT_x64/ARMJIT_Compiler.cpp
	ARMJIT_x64/ARMJIT_ALU.cpp

	dolphin/CommonFuncs.cpp
	dolphin/x64ABI.cpp
	dolphin/x64CPUDetect.cpp
	dolphin/x64Emitter.cpp
	dolphin/MemoryUtil.cpp
)

if (WIN32)
	target_link_libraries(core ole32 comctl32 ws2_32 opengl32)
else()
	target_link_libraries(core GL EGL)
endif()
