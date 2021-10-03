CXXFLAGS := -I./src -I./src/teakra/include -I.src/teakra/src \
	-Wall -Wextra -Werror=int-to-pointer-cast \
	-Wcast-qual -Wfatal-errors -Wno-missing-braces \
	-Wno-unused-parameter -Wno-parentheses -Wno-sign-compare \
	-Wno-unused-variable -Wno-unused-function \
	-pedantic -pedantic-errors \
	-std=c++17 -flto -fPIC \

TARGET = melonds.wbx

CORE_SRCS = \
	ARCodeFile.cpp \
	AREngine.cpp \
	ARM.cpp \
	ARMInterpreter.cpp \
	ARMInterpreter_ALU.cpp \
	ARMInterpreter_Branch.cpp \
	ARMInterpreter_LoadStore.cpp \
	BizInterface.cpp \
	BizPlatform.cpp \
	Config.cpp \
	CP15.cpp \
	CRC32.cpp \
	DMA.cpp \
	DSi.cpp \
	DSi_AES.cpp \
	DSi_Camera.cpp \
	DSi_DSP.cpp \
	DSi_I2C.cpp \
	DSi_NAND.cpp \
	DSi_NDMA.cpp \
	DSi_NWifi.cpp \
	DSi_SD.cpp \
	DSi_SPI_TSC.cpp \
	GBACart.cpp \
	GPU.cpp \
	GPU2D.cpp \
	GPU2D_Soft.cpp \
	GPU3D.cpp \
	GPU3D_Soft.cpp \
	NDS.cpp \
	NDSCart.cpp \
	NDSCart_SRAMManager.cpp \
	RTC.cpp \
	Savestate.cpp \
	SPI.cpp \
	SPU.cpp \
	Wifi.cpp \
	WifiAP.cpp

TEAKRA_SRCS = \
	ahbm.cpp \
	apbp.cpp \
	btdmp.cpp \
	disassembler.cpp \
	disassembler_c.cpp \
	dma.cpp \
	timer.cpp \
	memory_interface.cpp \
	mmio.cpp \
	parser.cpp \
	processor.cpp \
	teakra.cpp \
	teakra_c.cpp \
	test_generator.cpp

FATFS_SRCS = \
	diskio.c \
	ff.c \
	ffsystem.c \
	ffunicode.c

MISC_SRCS = \
	sha1/sha1.c \
	tiny-AES-c/aes.c \
	xxhash/xxhash.c

SRCS = \
	$(addprefix melonds/src/,$(CORE_SRCS)) \
	$(addprefix melonds/src/teakra/src/,$(TEAKRA_SRCS)) \
	$(addprefix melonds/src/fatfs/,$(FATFS_SRCS)) \
	$(addprefix melonds/src/,$(MISC_SRCS))

include ../common.mak
