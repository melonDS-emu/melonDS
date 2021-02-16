project(core)

set (CMAKE_CXX_STANDARD 17)

add_library(core STATIC
	ARCodeFile.cpp
	AREngine.cpp
	ARM.cpp
	ARM_InstrTable.h
	ARMInterpreter.cpp
	ARMInterpreter_ALU.cpp
	ARMInterpreter_Branch.cpp
	ARMInterpreter_LoadStore.cpp
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
	GPU2D.cpp
	GPU2D_Soft.cpp
	GPU3D.cpp
	GPU3D_Soft.cpp
	melonDLDI.h
	NDS.cpp
	NDSCart.cpp
	NDSCart_SRAMManager.cpp
	Platform.h
	ROMList.h
	FreeBIOS.h
	RTC.cpp
	Savestate.cpp
	SPI.cpp
	SPU.cpp
	types.h
	version.h
	Wifi.cpp
	WifiAP.cpp

	tiny-AES-c/aes.c
	xxhash/xxhash.c
)

if (ENABLE_OGLRENDERER)
	target_sources(core PRIVATE
		GPU_OpenGL.cpp
		GPU_OpenGL_shaders.h
		GPU3D_OpenGL.cpp
		GPU3D_OpenGL_shaders.h
		OpenGLSupport.cpp
	)
endif()

if (ENABLE_JIT)
	enable_language(ASM)

	target_sources(core PRIVATE
		ARM_InstrInfo.cpp

		ARMJIT.cpp
		ARMJIT_Memory.cpp

		dolphin/CommonFuncs.cpp
	)

	if (ARCHITECTURE STREQUAL x86_64)
		target_sources(core PRIVATE
			dolphin/x64ABI.cpp
			dolphin/x64CPUDetect.cpp
			dolphin/x64Emitter.cpp

			ARMJIT_x64/ARMJIT_Compiler.cpp
			ARMJIT_x64/ARMJIT_ALU.cpp
			ARMJIT_x64/ARMJIT_LoadStore.cpp
			ARMJIT_x64/ARMJIT_Branch.cpp

			ARMJIT_x64/ARMJIT_Linkage.S
		)
	endif()
	if (ARCHITECTURE STREQUAL ARM64)
		target_sources(core PRIVATE
			dolphin/Arm64Emitter.cpp
			dolphin/MathUtil.cpp

			ARMJIT_A64/ARMJIT_Compiler.cpp
			ARMJIT_A64/ARMJIT_ALU.cpp
			ARMJIT_A64/ARMJIT_LoadStore.cpp
			ARMJIT_A64/ARMJIT_Branch.cpp

			ARMJIT_A64/ARMJIT_Linkage.S
		)
	endif()
endif()

if (APPLE)
        target_include_directories(core PUBLIC /usr/local/include /opt/homebrew/include)
        target_link_directories(core PUBLIC /usr/local/lib /opt/homebrew/lib)
endif()

if (ENABLE_OGLRENDERER)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EPOXY REQUIRED epoxy)

    target_include_directories(core PRIVATE ${EPOXY_INCLUDE_DIRS})
    if (WIN32)
        target_link_libraries(core ole32 comctl32 ws2_32 ${EPOXY_LIBRARIES})
    elseif (APPLE)
        target_link_libraries(core ${EPOXY_LIBRARIES})
    else()
        target_link_libraries(core rt ${EPOXY_LIBRARIES})
    endif()
else()
	if (WIN32)
		target_link_libraries(core ole32 comctl32 ws2_32)
	else()
		target_link_libraries(core rt)
	endif()
endif()
