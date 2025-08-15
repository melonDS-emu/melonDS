set (CMAKE_CXX_STANDARD 17)

include(FixInterfaceIncludes)

add_library(core STATIC
    ARCodeFile.cpp
    AREngine.cpp
    ARM.cpp
    ARM_InstrTable.h
    ARMInterpreter.cpp
    ARMInterpreter_ALU.cpp
    ARMInterpreter_Branch.cpp
    ARMInterpreter_LoadStore.cpp
    CP15.cpp
    CRC32.cpp
    DMA.cpp
    DMA_Timings.h
    DMA_Timings.cpp
    DSi.cpp
    DSi_AES.cpp
    DSi_Camera.cpp
    DSi_DSP.cpp
    DSi_I2C.cpp
    DSi_I2S.cpp
    DSi_NAND.cpp
    DSi_NDMA.cpp
    DSi_NWifi.cpp
    DSi_SD.cpp
    DSi_SPI_TSC.cpp
    FATIO.cpp
    FATStorage.cpp
    FIFO.h
    GBACart.cpp
    GBACartMotionPak.cpp
    GPU.cpp
    GPU2D.cpp
    GPU2D_Soft.cpp
    GPU3D.cpp
    GPU3D_Soft.cpp
    GPU3D_Texcache.cpp
    GPU3D_Texcache.h
    melonDLDI.h
    Mic.cpp
    NDS.cpp
    NDSCart.cpp
    NDSCartR4.cpp
    Platform.h
    ROMList.h
    ROMList.cpp
    FreeBIOS.h
    FreeBIOS.cpp
    RTC.cpp
    Savestate.cpp
    SPI.cpp
    SPI_Firmware.cpp
    SPU.cpp
    types.h
    Utils.cpp
    Utils.h
    Wifi.cpp
    WifiAP.cpp

    DSP_HLE/UcodeBase.cpp
    DSP_HLE/AACUcode.cpp
    DSP_HLE/G711Ucode.cpp
    DSP_HLE/GraphicsUcode.cpp

    fatfs/ff.c
    fatfs/ffsystem.c
    fatfs/ffunicode.c
    fatfs/ffconf.h

    sha1/sha1.c
    tiny-AES-c/aes.c
    xxhash/xxhash.c)

if (ENABLE_GDBSTUB)
    message(NOTICE "Enabling GDB stub")
    target_sources(core PRIVATE
        debug/GdbStub.cpp
        debug/GdbProto.cpp
        debug/GdbCmds.cpp
    )
endif()

if (ENABLE_OGLRENDERER)
    target_sources(core PRIVATE
        GPU_OpenGL.cpp
        GPU_OpenGL_shaders.h
        GPU3D_OpenGL.cpp
        GPU3D_Compute.cpp
        GPU3D_TexcacheOpenGL.cpp
        GPU3D_TexcacheOpenGL.h
        GPU3D_OpenGL_shaders.h
        OpenGLSupport.cpp)

    target_compile_definitions(core PUBLIC OGLRENDERER_ENABLED)
endif()

if (ENABLE_JIT)
    enable_language(ASM)

    target_sources(core PRIVATE
        ARM_InstrInfo.cpp

        ARMJIT.cpp
        ARMJIT_Memory.cpp
        ARMJIT_Global.cpp

        dolphin/CommonFuncs.cpp)
    
    if (WIN32)
        # Required for memory mapping-related functions introduced in Windows 8
        target_compile_definitions(core PRIVATE -D_WIN32_WINNT=_WIN32_WINNT_WIN8)
        target_link_libraries(core PRIVATE onecore)
    endif()

    if (ARCHITECTURE STREQUAL x86_64)
        target_sources(core PRIVATE
            dolphin/x64ABI.cpp
            dolphin/x64CPUDetect.cpp
            dolphin/x64Emitter.cpp

            ARMJIT_x64/ARMJIT_Compiler.cpp
            ARMJIT_x64/ARMJIT_ALU.cpp
            ARMJIT_x64/ARMJIT_LoadStore.cpp
            ARMJIT_x64/ARMJIT_Branch.cpp

            ARMJIT_x64/ARMJIT_Linkage.S)
    endif()
    if (ARCHITECTURE STREQUAL ARM64)
        target_sources(core PRIVATE
            dolphin/Arm64Emitter.cpp
            dolphin/MathUtil.cpp

            ARMJIT_A64/ARMJIT_Compiler.cpp
            ARMJIT_A64/ARMJIT_ALU.cpp
            ARMJIT_A64/ARMJIT_LoadStore.cpp
            ARMJIT_A64/ARMJIT_Branch.cpp

            ARMJIT_A64/ARMJIT_Linkage.S)
    endif()
endif()

target_include_directories(core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

set(MELONDS_VERSION_SUFFIX "$ENV{MELONDS_VERSION_SUFFIX}" CACHE STRING "Suffix to add to displayed melonDS version")
option(MELONDS_EMBED_BUILD_INFO "Embed detailed build info into the binary" OFF)
set(MELONDS_GIT_BRANCH "$ENV{MELONDS_GIT_BRANCH}" CACHE STRING "The Git branch used for this build")
set(MELONDS_GIT_HASH "$ENV{MELONDS_GIT_HASH}" CACHE STRING "The hash of the Git commit")
set(MELONDS_BUILD_PROVIDER "$ENV{MELONDS_BUILD_PROVIDER}" CACHE STRING "The name of the provider of this build")

if (MELONDS_EMBED_BUILD_INFO)
    target_compile_definitions(core PUBLIC MELONDS_EMBED_BUILD_INFO)
    if (NOT MELONDS_GIT_BRANCH OR NOT MELONDS_GIT_HASH OR NOT MELONDS_BUILD_PROVIDER)
        message(FATAL_ERROR "When embedding build information, all fields must be filled out. See src/CMakeLists.txt.")
    endif()
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h")
target_sources(core PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/version.h")
target_include_directories(core PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(teakra EXCLUDE_FROM_ALL)
# Workaround for building teakra with -O0 on Windows either failing or hanging forever
target_compile_options(teakra PRIVATE "$<$<CONFIG:DEBUG>:-Og>")
target_link_libraries(core PRIVATE teakra)

if (NOT MSVC)
    # MSVC has its own compiler flag syntax; if we ever support it,
    # be sure to add equivalent flags here.

    target_compile_options(core PUBLIC -fwrapv)

    target_compile_options(core PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Wno-invalid-offsetof>")
    # These warnings are excessive, and are only triggered in the ARMJIT code
    # (which is fundamentally non-portable, so this is fine)
endif()

find_library(m MATH_LIBRARY)

if (MATH_LIBRARY)
    target_link_libraries(core PRIVATE ${MATH_LIBRARY})
endif()

if (ENABLE_JIT)
    target_compile_definitions(core PUBLIC JIT_ENABLED)

    if (ENABLE_JIT_PROFILING)
        include(../cmake/FindVTune.cmake)
        add_definitions(-DJIT_PROFILING_ENABLED)
    endif()
endif()

if (WIN32)
    target_link_libraries(core PRIVATE ole32 comctl32 wsock32 ws2_32)
    target_compile_definitions(core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
elseif(NOT APPLE AND NOT HAIKU)
    check_library_exists(rt shm_open "" NEED_LIBRT)
    if (NEED_LIBRT)
        target_link_libraries(core PRIVATE rt)
    endif()
elseif(HAIKU)
    target_link_libraries(core PRIVATE network)
endif()

if (ENABLE_JIT_PROFILING)
    target_include_directories(core PRIVATE "${VTUNE_INCLUDE_DIR}")
    target_link_libraries(core PRIVATE "${VTUNE_LIBRARY}")
endif()

#if(CMAKE_BUILD_TYPE MATCHES "Debug")
#  set(
#    CMAKE_C_FLAGS
#    "${CMAKE_C_FLAGS} -fsanitize=undefined -fsanitize=address"
#  )
#  target_link_options(core
#    BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address
#  )
#endif()

