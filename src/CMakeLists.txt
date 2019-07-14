project(core)

set (CMAKE_CXX_STANDARD 14)

add_library(core STATIC
	ARCodeList.cpp
	AREngine.cpp
	ARM.cpp
	ARM_InstrTable.h
	ARMInterpreter.cpp
	ARMInterpreter_ALU.cpp
	ARMInterpreter_Branch.cpp
	ARMInterpreter_LoadStore.cpp
	ARM_InstrInfo.cpp
	Config.cpp
	CP15.cpp
	CRC32.cpp
	DMA.cpp
	DSi.cpp
	DSi_AES.cpp
	DSi_Camera.cpp
	DSi_I2C.cpp
	DSi_NDMA.cpp
	DSi_NWifi.cpp
	DSi_SD.cpp
	DSi_SPI_TSC.cpp
	FIFO.h
	GBACart.cpp
	GPU.cpp
	GPU_OpenGL.cpp
	GPU_OpenGL_shaders.h
	GPU2D.cpp
	GPU3D.cpp
	GPU3D_OpenGL.cpp
	GPU3D_OpenGL_shaders.h
	GPU3D_Soft.cpp
	NDS.cpp
	NDSCart.cpp
	OpenGLSupport.cpp
	Platform.h
	ROMList.h
	RTC.cpp
	Savestate.cpp
	SPI.cpp
	SPU.cpp
	types.h
	version.h
	Wifi.cpp
	WifiAP.cpp
	
	tiny-AES-c/aes.c
)

if (ENABLE_JIT)
	target_sources(core PRIVATE
		ARMJIT.cpp
		ARMJIT_x64/ARMJIT_Compiler.cpp
		ARMJIT_x64/ARMJIT_ALU.cpp
		ARMJIT_x64/ARMJIT_LoadStore.cpp
		ARMJIT_x64/ARMJIT_Branch.cpp

		dolphin/CommonFuncs.cpp
		dolphin/x64ABI.cpp
		dolphin/x64CPUDetect.cpp
		dolphin/x64Emitter.cpp
	)
endif()

if (WIN32)
	target_link_libraries(core ole32 comctl32 ws2_32 opengl32)
else()
	target_link_libraries(core GL EGL)
endif()
