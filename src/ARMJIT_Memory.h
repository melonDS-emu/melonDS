#ifndef ARMJIT_MEMORY
#define ARMJIT_MEMORY

#include "types.h"

#include "ARM.h"

namespace ARMJIT_Memory
{

extern void* FastMem9Start;
extern void* FastMem7Start;

void Init();
void DeInit();

void Reset();

enum
{
	memregion_Other = 0,
	memregion_ITCM,
	memregion_DTCM,
	memregion_BIOS9,
	memregion_MainRAM,
	memregion_SWRAM,
	memregion_IO9,
	memregion_VRAM,
	memregion_BIOS7,
	memregion_WRAM7,
	memregion_IO7,
	memregion_Wifi,
	memregion_VWRAM,
	memregions_Count
};

int ClassifyAddress9(u32 addr);
int ClassifyAddress7(u32 addr);

bool GetRegionMapping(int region, u32 num, u32& mappingStart, u32& mappingSize, u32& memoryOffset, u32& memorySize);

bool IsMappable(int region);

void RemapDTCM(u32 newBase, u32 newSize);
void RemapSWRAM();

void SetCodeProtection(int region, u32 offset, bool protect);

void* GetFuncForAddr(ARM* cpu, u32 addr, bool store, int size);

}

#endif