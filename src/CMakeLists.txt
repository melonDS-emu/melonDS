project(core)

add_library(core STATIC
	ARM.cpp
	ARMInterpreter.cpp
	ARMInterpreter_ALU.cpp
	ARMInterpreter_Branch.cpp
	ARMInterpreter_LoadStore.cpp
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
	slot2/GBACart.cpp
	slot2/GBACartGBAMP.cpp
	slot2/GBACartMemoryPak.cpp
	slot2/GBACartSuperCardCF.cpp
	slot2/GBACartNone.cpp
	slot2/IDEDrive.cpp
)

if (WIN32)
	target_link_libraries(core ole32 comctl32 ws2_32 opengl32)
else()
	target_link_libraries(core GL EGL)
endif()
