#ifdef __SWITCH__
#include "switch/compat_switch.h"
#endif

#include "ARMJIT_Memory.h"

#include "ARMJIT_Internal.h"
#include "ARMJIT_Compiler.h"

#include "GPU.h"
#include "GPU3D.h"
#include "Wifi.h"
#include "NDSCart.h"
#include "SPU.h"

#include <malloc.h>

/*
	We're handling fastmem here.

	Basically we're repurposing a big piece of virtual memory
	and map the memory regions as they're structured on the DS
	in it.

	On most systems you have a single piece of main ram, 
	maybe some video ram and faster cache RAM and that's about it.
	Here we have not only a lot more different memory regions,
	but also two address spaces. Not only that but they all have
	mirrors (the worst case is 16kb SWRAM which is mirrored 1024x).

	We handle this by only mapping those regions which are actually
	used and by praying the games don't go wild.

	Beware, this file is full of platform specific code.

*/

namespace ARMJIT_Memory
{
#ifdef __aarch64__
struct FaultDescription
{
	u64 IntegerRegisters[33];
	u64 FaultAddr;

	u32 GetEmulatedAddr()
	{
		// now this is podracing
		return (u32)IntegerRegisters[0];
	}
	u64 RealAddr()
	{
		return FaultAddr;
	}

	u64 GetPC()
	{
		return IntegerRegisters[32];
	}

	void RestoreAndRepeat(s64 offset);
};
#else
struct FaultDescription
{
	u64 GetPC()
	{
		return 0;
	}
	
	u32 GetEmulatedAddr()
	{
		return 0;
	}
	u64 RealAddr()
	{
		return 0;
	}

	void RestoreAndRepeat(s64 offset);
};
#endif

void FaultHandler(FaultDescription* faultDesc);
}


#ifdef __aarch64__

extern "C" void ARM_RestoreContext(u64* registers) __attribute__((noreturn));

#endif

#ifdef __SWITCH__
// with LTO the symbols seem to be not properly overriden
// if they're somewhere else

extern "C"
{
extern char __start__;
extern char __rodata_start;

alignas(16) u8 __nx_exception_stack[0x8000];
u64 __nx_exception_stack_size = 0x8000;

void __libnx_exception_handler(ThreadExceptionDump* ctx)
{
	ARMJIT_Memory::FaultDescription desc;
	memcpy(desc.IntegerRegisters, &ctx->cpu_gprs[0].x, 8*29);
	desc.IntegerRegisters[29] = ctx->fp.x;
	desc.IntegerRegisters[30] = ctx->lr.x;
	desc.IntegerRegisters[31] = ctx->sp.x;
	desc.IntegerRegisters[32] = ctx->pc.x;

	ARMJIT_Memory::FaultHandler(&desc);

	if (ctx->pc.x >= (u64)&__start__ && ctx->pc.x < (u64)&__rodata_start)
	{
		printf("non JIT fault in .text at 0x%x (type %d) (trying to access 0x%x?)\n", 
			ctx->pc.x - (u64)&__start__, ctx->error_desc, ctx->far.x);
	}
	else
	{
		printf("non JIT fault somewhere in deep (address) space at %x (type %d)\n", ctx->pc.x, ctx->error_desc);
	}
}

}
#endif

namespace ARMJIT_Memory
{

#ifdef __aarch64__
void FaultDescription::RestoreAndRepeat(s64 offset)
{
	IntegerRegisters[32] += offset;

	ARM_RestoreContext(IntegerRegisters);
}
#else
void FaultDescription::RestoreAndRepeat(s64 offset)
{
	
}
#endif

void* FastMem9Start, *FastMem7Start;

const u32 MemoryTotalSize =
	NDS::MainRAMSize
	+ NDS::SharedWRAMSize
	+ NDS::ARM7WRAMSize
	+ DTCMPhysicalSize;

const u32 MemBlockMainRAMOffset = 0;
const u32 MemBlockSWRAMOffset = NDS::MainRAMSize;
const u32 MemBlockARM7WRAMOffset = NDS::MainRAMSize + NDS::SharedWRAMSize;
const u32 MemBlockDTCMOffset = NDS::MainRAMSize + NDS::SharedWRAMSize + NDS::ARM7WRAMSize;

const u32 OffsetsPerRegion[memregions_Count] =
{
	UINT32_MAX,
	UINT32_MAX,
	MemBlockDTCMOffset,
	UINT32_MAX,
	MemBlockMainRAMOffset,
	MemBlockSWRAMOffset,
	UINT32_MAX,
	UINT32_MAX,
	UINT32_MAX,
	MemBlockARM7WRAMOffset,
	UINT32_MAX,
	UINT32_MAX,
	UINT32_MAX,
};

enum
{
	memstate_Unmapped,
	memstate_MappedRW,
	// on switch this is unmapped as well
	memstate_MappedProtected,
};

u8 MappingStatus9[1 << (32-12)];
u8 MappingStatus7[1 << (32-12)];

#ifdef __SWITCH__
u8* MemoryBase;
u8* MemoryBaseCodeMem;
#else
u8* MemoryBase;
#endif

bool MapIntoRange(u32 addr, u32 num, u32 offset, u32 size)
{
	u8* dst = (u8*)(num == 0 ? FastMem9Start : FastMem7Start) + addr;
#ifdef __SWITCH__
	Result r = (svcMapProcessMemory(dst, envGetOwnProcessHandle(), 
		(u64)(MemoryBaseCodeMem + offset), size));
	return R_SUCCEEDED(r);
#endif
}

bool UnmapFromRange(u32 addr, u32 num, u32 offset, u32 size)
{
	u8* dst = (u8*)(num == 0 ? FastMem9Start : FastMem7Start) + addr;
#ifdef __SWITCH__
	Result r = svcUnmapProcessMemory(dst, envGetOwnProcessHandle(),
		(u64)(MemoryBaseCodeMem + offset), size);
	printf("%x\n", r);
	return R_SUCCEEDED(r);
#endif
}

struct Mapping
{
	u32 Addr;
	u32 Size, LocalOffset;
	u32 Num;

	void Unmap(int region)
	{
		bool skipDTCM = Num == 0 && region != memregion_DTCM;
		u8* statuses = Num == 0 ? MappingStatus9 : MappingStatus7;
		u32 offset = 0;
		while (offset < Size)
		{
			if (skipDTCM && Addr + offset == NDS::ARM9->DTCMBase)
			{
				offset += NDS::ARM9->DTCMSize;
				printf("%x skip\n", NDS::ARM9->DTCMSize);
			}
			else
			{
				u32 segmentOffset = offset;
				u8 status = statuses[(Addr + offset) >> 12];
				while (statuses[(Addr + offset) >> 12] == status
					&& offset < Size
					&& (!skipDTCM || Addr + offset != NDS::ARM9->DTCMBase))
				{
					assert(statuses[(Addr + offset) >> 12] != memstate_Unmapped);
					statuses[(Addr + offset) >> 12] = memstate_Unmapped;
					offset += 0x1000;
				}

				if (status == memstate_MappedRW)
				{
					u32 segmentSize = offset - segmentOffset;
					printf("unmapping %x %x %x %x\n", Addr + segmentOffset, Num, segmentOffset + LocalOffset + OffsetsPerRegion[region], segmentSize);
					bool success = UnmapFromRange(Addr + segmentOffset, Num, segmentOffset + LocalOffset + OffsetsPerRegion[region], segmentSize);
					assert(success);
				}
			}
		}
	}
};
ARMJIT::TinyVector<Mapping> Mappings[memregions_Count];

void SetCodeProtection(int region, u32 offset, bool protect)
{
	offset &= ~0xFFF;
	printf("set code protection %d %x %d\n", region, offset, protect);

	for (int i = 0; i < Mappings[region].Length; i++)
	{
		Mapping& mapping = Mappings[region][i];

		u32 effectiveAddr = mapping.Addr + (offset - mapping.LocalOffset);
		if (mapping.Num == 0
			&& region != memregion_DTCM 
			&& effectiveAddr >= NDS::ARM9->DTCMBase
			&& effectiveAddr < (NDS::ARM9->DTCMBase + NDS::ARM9->DTCMSize))
			continue;

		u8* states = (u8*)(mapping.Num == 0 ? MappingStatus9 : MappingStatus7);

		printf("%d %x %d\n", states[effectiveAddr >> 12], effectiveAddr, mapping.Num);
		assert(states[effectiveAddr >> 12] == (protect ? memstate_MappedRW : memstate_MappedProtected));
		states[effectiveAddr >> 12] = protect ? memstate_MappedProtected : memstate_MappedRW;

		bool success;
		if (protect)
			success = UnmapFromRange(effectiveAddr, mapping.Num, OffsetsPerRegion[region] + offset, 0x1000);
		else
			success = MapIntoRange(effectiveAddr, mapping.Num, OffsetsPerRegion[region] + offset, 0x1000);
		assert(success);
	}
}

void RemapDTCM(u32 newBase, u32 newSize)
{
	// this first part could be made more efficient
	// by unmapping DTCM first and then map the holes
	u32 oldDTCMBase = NDS::ARM9->DTCMBase;
	u32 oldDTCBEnd = oldDTCMBase + NDS::ARM9->DTCMSize;

	u32 newEnd = newBase + newSize;

	printf("remapping DTCM %x %x %x %x\n", newBase, newEnd, oldDTCMBase, oldDTCBEnd);
	// unmap all regions containing the old or the current DTCM mapping
	for (int region = 0; region < memregions_Count; region++)
	{
		if (region == memregion_DTCM)
			continue;

		for (int i = 0; i < Mappings[region].Length;)
		{
			Mapping& mapping = Mappings[region][i];

			u32 start = mapping.Addr;
			u32 end = mapping.Addr + mapping.Size;

			printf("mapping %d %x %x %x %x\n", region, mapping.Addr, mapping.Size, mapping.Num, mapping.LocalOffset);

			bool oldOverlap = NDS::ARM9->DTCMSize > 0 && ((oldDTCMBase >= start && oldDTCMBase < end) || (oldDTCBEnd >= start && oldDTCBEnd < end));
			bool newOverlap = newSize > 0 && ((newBase >= start && newBase < end) || (newEnd >= start && newEnd < end));

			if (mapping.Num == 0 && (oldOverlap || newOverlap))
			{
				mapping.Unmap(region);
				Mappings[region].Remove(i);
			}
			else
			{
				i++;
			}
		}
	}

	for (int i = 0; i < Mappings[memregion_DTCM].Length; i++)
	{
		Mappings[memregion_DTCM][i].Unmap(memregion_DTCM);
	}
	Mappings[memregion_DTCM].Clear();
}

void RemapSWRAM()
{
	printf("remapping SWRAM\n");
	for (int i = 0; i < Mappings[memregion_SWRAM].Length; i++)
	{
		Mappings[memregion_SWRAM][i].Unmap(memregion_SWRAM);
	}
	Mappings[memregion_SWRAM].Clear();
	for (int i = 0; i < Mappings[memregion_WRAM7].Length; i++)
	{
		Mappings[memregion_WRAM7][i].Unmap(memregion_WRAM7);
	}
	Mappings[memregion_WRAM7].Clear();
}

bool MapAtAddress(u32 addr)
{
	u32 num = NDS::CurCPU;

	int region = num == 0
		? ClassifyAddress9(addr)
		: ClassifyAddress7(addr);

	if (!IsMappable(region))
		return false;

	u32 mappingStart, mappingSize, memoryOffset, memorySize;
	bool isMapped = GetRegionMapping(region, num, mappingStart, mappingSize, memoryOffset, memorySize);

	if (!isMapped)
		return false;

	// this calculation even works with DTCM
	// which doesn't have to be aligned to it's own size
	u32 mirrorStart = (addr - mappingStart) / memorySize * memorySize + mappingStart;

	u8* states = num == 0 ? MappingStatus9 : MappingStatus7;
	printf("trying to create mapping %08x %d %x %d %x\n", addr, num, memorySize, region, memoryOffset);
	bool isExecutable = ARMJIT::CodeMemRegions[region];

	ARMJIT::AddressRange* range = ARMJIT::CodeMemRegions[region] + memoryOffset;

	// this overcomplicated piece of code basically just finds whole pieces of code memory
	// which can be mapped
	u32 offset = 0;	
	bool skipDTCM = num == 0 && region != memregion_DTCM;
	while (offset < memorySize)
	{
		if (skipDTCM && mirrorStart + offset == NDS::ARM9->DTCMBase)
		{
			offset += NDS::ARM9->DTCMSize;
		}
		else
		{
			u32 sectionOffset = offset;
			bool hasCode = isExecutable && ARMJIT::PageContainsCode(&range[offset / 512]);
			while ((!isExecutable || ARMJIT::PageContainsCode(&range[offset / 512]) == hasCode)
				&& offset < memorySize
				&& (!skipDTCM || mirrorStart + offset != NDS::ARM9->DTCMBase))
			{
				assert(states[(mirrorStart + offset) >> 12] == memstate_Unmapped);
				states[(mirrorStart + offset) >> 12] = hasCode ? memstate_MappedProtected : memstate_MappedRW;
				offset += 0x1000;
			}

			u32 sectionSize = offset - sectionOffset;

			if (!hasCode)
			{
				printf("trying to map %x (size: %x) from %x\n", mirrorStart + sectionOffset, sectionSize, sectionOffset + memoryOffset + OffsetsPerRegion[region]);
				bool succeded = MapIntoRange(mirrorStart + sectionOffset, num, sectionOffset + memoryOffset + OffsetsPerRegion[region], sectionSize);
				assert(succeded);
			}
		}
	}

	Mapping mapping{mirrorStart, memorySize, memoryOffset, num};
	Mappings[region].Add(mapping);

	printf("mapped mirror at %08x-%08x\n", mirrorStart, mirrorStart + memorySize - 1);

	return true;
}

void FaultHandler(FaultDescription* faultDesc)
{
	if (ARMJIT::JITCompiler->IsJITFault(faultDesc->GetPC()))
	{
		bool rewriteToSlowPath = true;

		u32 addr = faultDesc->GetEmulatedAddr();

		if ((NDS::CurCPU == 0 ? MappingStatus9 : MappingStatus7)[addr >> 12] == memstate_Unmapped)
			rewriteToSlowPath = !MapAtAddress(faultDesc->GetEmulatedAddr());

		s64 offset = 0;
		if (rewriteToSlowPath)
		{
			offset = ARMJIT::JITCompiler->RewriteMemAccess(faultDesc->GetPC());
		}
		faultDesc->RestoreAndRepeat(offset);
	}
}

void Init()
{
#if defined(__SWITCH__)
    MemoryBase = (u8*)memalign(0x1000, MemoryTotalSize);
	MemoryBaseCodeMem = (u8*)virtmemReserve(MemoryTotalSize);

    bool succeded = R_SUCCEEDED(svcMapProcessCodeMemory(envGetOwnProcessHandle(), (u64)MemoryBaseCodeMem, 
        (u64)MemoryBase, MemoryTotalSize));
    assert(succeded);
	succeded = R_SUCCEEDED(svcSetProcessMemoryPermission(envGetOwnProcessHandle(), (u64)MemoryBaseCodeMem, 
        MemoryTotalSize, Perm_Rw));
	assert(succeded);

	// 8 GB of address space, just don't ask...
	FastMem9Start = virtmemReserve(0x100000000);
	assert(FastMem9Start);
	FastMem7Start = virtmemReserve(0x100000000);
	assert(FastMem7Start);

	NDS::MainRAM = MemoryBaseCodeMem + MemBlockMainRAMOffset;
	NDS::SharedWRAM = MemoryBaseCodeMem + MemBlockSWRAMOffset;
	NDS::ARM7WRAM = MemoryBaseCodeMem + MemBlockARM7WRAMOffset;
	NDS::ARM9->DTCM = MemoryBaseCodeMem + MemBlockDTCMOffset;
#else
	MemoryBase = new u8[MemoryTotalSize];

	NDS::MainRAM = MemoryBase + MemBlockMainRAMOffset;
	NDS::SharedWRAM = MemoryBase + MemBlockSWRAMOffset;
	NDS::ARM7WRAM = MemoryBase + MemBlockARM7WRAMOffset;
	NDS::ARM9->DTCM = MemoryBase + MemBlockDTCMOffset;
#endif
}

void DeInit()
{
#if defined(__SWITCH__)
	virtmemFree(FastMem9Start, 0x100000000);
	virtmemFree(FastMem7Start, 0x100000000);

    svcUnmapProcessCodeMemory(envGetOwnProcessHandle(), (u64)MemoryBaseCodeMem, (u64)MemoryBase, MemoryTotalSize);
	virtmemFree(MemoryBaseCodeMem, MemoryTotalSize);
    free(MemoryBase);
#else
	delete[] MemoryBase;
#endif
}

void Reset()
{
	for (int region = 0; region < memregions_Count; region++)
	{
		for (int i = 0; i < Mappings[region].Length; i++)
			Mappings[region][i].Unmap(region);
		Mappings[region].Clear();
	}

	for (int i = 0; i < sizeof(MappingStatus9); i++)
	{
		assert(MappingStatus9[i] == memstate_Unmapped);
		assert(MappingStatus7[i] == memstate_Unmapped);
	}

	printf("done resetting jit mem\n");
}

bool IsMappable(int region)
{
	return OffsetsPerRegion[region] != UINT32_MAX;
}

bool GetRegionMapping(int region, u32 num, u32& mappingStart, u32& mappingSize, u32& memoryOffset, u32& memorySize)
{
	memoryOffset = 0;
	switch (region)
	{
	case memregion_ITCM:
		if (num == 0)
		{
			mappingStart = 0;
			mappingSize = NDS::ARM9->ITCMSize;
			memorySize = ITCMPhysicalSize;
			return true;
		}
		return false;
	case memregion_DTCM:
		if (num == 0)
		{
			mappingStart = NDS::ARM9->DTCMBase;
			mappingSize = NDS::ARM9->DTCMSize;
			memorySize = DTCMPhysicalSize;
			return true;
		}
		return false;
	case memregion_BIOS9:
		if (num == 0)
		{
			mappingStart = 0xFFFF0000;
			mappingSize = 0x10000;
			memorySize = 0x1000;
			return true;
		}
		return false;
	case memregion_MainRAM:
		mappingStart = 0x2000000;
		mappingSize = 0x1000000;
		memorySize = NDS::MainRAMSize;
		return true;
	case memregion_SWRAM:
		mappingStart = 0x3000000;
		if (num == 0 && NDS::SWRAM_ARM9.Mem)
		{
			mappingSize = 0x1000000;
			memoryOffset = NDS::SWRAM_ARM9.Mem - NDS::SharedWRAM;
			memorySize = NDS::SWRAM_ARM9.Mask + 1;
			return true;
		}
		else if (num == 1 && NDS::SWRAM_ARM7.Mem)
		{
			mappingSize = 0x800000;
			memoryOffset = NDS::SWRAM_ARM7.Mem - NDS::SharedWRAM;
			memorySize = NDS::SWRAM_ARM7.Mask + 1;
			return true;
		}
		return false;
	case memregion_VRAM:
		if (num == 0)
		{
			// this is a gross simplification
			// mostly to make code on vram working
			// it doesn't take any of the actual VRAM mappings into account
			mappingStart = 0x6000000;
			mappingSize = 0x1000000;
			memorySize = 0x100000;
			return true;
		}
		return false;
	case memregion_BIOS7:
		if (num == 1)
		{
			mappingStart = 0;
			mappingSize = 0x4000;
			memorySize = 0x4000;
			return true;
		}
		return false;
	case memregion_WRAM7:
		if (num == 1)
		{
			if (NDS::SWRAM_ARM7.Mem)
			{
				mappingStart = 0x3800000;
				mappingSize = 0x800000;
			}
			else
			{
				mappingStart = 0x3000000;
				mappingSize = 0x1000000;
			}
			memorySize = NDS::ARM7WRAMSize;
			return true;
		}
		return false;
	case memregion_VWRAM:
		if (num == 1)
		{
			mappingStart = 0x6000000;
			mappingSize = 0x1000000;
			memorySize = 0x20000;
			return true;
		}
		return false;
	default:
		// for the JIT we don't are about the rest
		return false;
	}
}

int ClassifyAddress9(u32 addr)
{
	if (addr < NDS::ARM9->ITCMSize)
		return memregion_ITCM;
	else if (addr >= NDS::ARM9->DTCMBase && addr < (NDS::ARM9->DTCMBase + NDS::ARM9->DTCMSize))
		return memregion_DTCM;
	else if ((addr & 0xFFFFF000) == 0xFFFF0000)
		return memregion_BIOS9;
	else
	{
		switch (addr & 0xFF000000)
		{
		case 0x02000000:
			return memregion_MainRAM;
		case 0x03000000:
			if (NDS::SWRAM_ARM9.Mem)
				return memregion_SWRAM;
			else
				return memregion_Other;
		case 0x04000000:
			return memregion_IO9;
		case 0x06000000:
			return memregion_VRAM;
		}
	}
	return memregion_Other;
}

int ClassifyAddress7(u32 addr)
{
	if (addr < 0x00004000)
		return memregion_BIOS7;
	else
	{
		switch (addr & 0xFF800000)
		{
		case 0x02000000:
		case 0x02800000:
			return memregion_MainRAM;
		case 0x03000000:
			if (NDS::SWRAM_ARM7.Mem)
				return memregion_SWRAM;
			else
				return memregion_WRAM7;
		case 0x03800000:
			return memregion_WRAM7;
		case 0x04000000:
			return memregion_IO7;
		case 0x04800000:
			return memregion_Wifi;
		case 0x06000000:
		case 0x06800000:
			return memregion_VWRAM;
		}
	}
	return memregion_Other;
}

void WifiWrite32(u32 addr, u32 val)
{
	Wifi::Write(addr, val & 0xFFFF);
	Wifi::Write(addr + 2, val >> 16);
}

u32 WifiRead32(u32 addr)
{
	return Wifi::Read(addr) | (Wifi::Read(addr + 2) << 16);
}

template <typename T>
void VRAMWrite(u32 addr, T val)
{
	switch (addr & 0x00E00000)
	{
	case 0x00000000: GPU::WriteVRAM_ABG<T>(addr, val); return;
	case 0x00200000: GPU::WriteVRAM_BBG<T>(addr, val); return;
	case 0x00400000: GPU::WriteVRAM_AOBJ<T>(addr, val); return;
	case 0x00600000: GPU::WriteVRAM_BOBJ<T>(addr, val); return;
	default: GPU::WriteVRAM_LCDC<T>(addr, val); return;
	}
}
template <typename T>
T VRAMRead(u32 addr)
{
	switch (addr & 0x00E00000)
	{
	case 0x00000000: return GPU::ReadVRAM_ABG<T>(addr);
	case 0x00200000: return GPU::ReadVRAM_BBG<T>(addr);
	case 0x00400000: return GPU::ReadVRAM_AOBJ<T>(addr);
	case 0x00600000: return GPU::ReadVRAM_BOBJ<T>(addr);
	default: return GPU::ReadVRAM_LCDC<T>(addr);
	}
}

void* GetFuncForAddr(ARM* cpu, u32 addr, bool store, int size)
{
	if (cpu->Num == 0)
	{
		switch (addr & 0xFF000000)
		{
		case 0x04000000:
			if (!store && size == 32 && addr == 0x04100010 && NDS::ExMemCnt[0] & (1<<11))
				return (void*)NDSCart::ReadROMData;

			/*
				unfortunately we can't map GPU2D this way
				since it's hidden inside an object

				though GPU3D registers are accessed much more intensive
			*/
			if (addr >= 0x04000320 && addr < 0x040006A4)
			{
				switch (size | store)
				{
				case 8: return (void*)GPU3D::Read8;		
				case 9: return (void*)GPU3D::Write8;		
				case 16: return (void*)GPU3D::Read16;
				case 17: return (void*)GPU3D::Write16;
				case 32: return (void*)GPU3D::Read32;
				case 33: return (void*)GPU3D::Write32;
				}
			}

			switch (size | store)
			{
			case 8: return (void*)NDS::ARM9IORead8;
			case 9: return (void*)NDS::ARM9IOWrite8;
			case 16: return (void*)NDS::ARM9IORead16;
			case 17: return (void*)NDS::ARM9IOWrite16;
			case 32: return (void*)NDS::ARM9IORead32;
			case 33: return (void*)NDS::ARM9IOWrite32;
			}
			break;
		case 0x06000000:
			switch (size | store)
			{
			case 8: return (void*)VRAMRead<u8>;		
			case 9: return NULL;
			case 16: return (void*)VRAMRead<u16>;
			case 17: return (void*)VRAMWrite<u16>;
			case 32: return (void*)VRAMRead<u32>;
			case 33: return (void*)VRAMWrite<u32>;
			}
			break;
		}
	}
	else
	{
		switch (addr & 0xFF800000)
		{
		case 0x04000000:
			if (addr >= 0x04000400 && addr < 0x04000520)
			{
				switch (size | store)
				{
				case 8: return (void*)SPU::Read8;		
				case 9: return (void*)SPU::Write8;		
				case 16: return (void*)SPU::Read16;
				case 17: return (void*)SPU::Write16;
				case 32: return (void*)SPU::Read32;
				case 33: return (void*)SPU::Write32;
				}
			}

			switch (size | store)
			{
			case 8: return (void*)NDS::ARM7IORead8;
			case 9: return (void*)NDS::ARM7IOWrite8;		
			case 16: return (void*)NDS::ARM7IORead16;
			case 17: return (void*)NDS::ARM7IOWrite16;
			case 32: return (void*)NDS::ARM7IORead32;
			case 33: return (void*)NDS::ARM7IOWrite32;
			}
			break;
		case 0x04800000:
			if (addr < 0x04810000 && size >= 16)
			{
				switch (size | store)
				{
				case 16: return (void*)Wifi::Read;
				case 17: return (void*)Wifi::Write;
				case 32: return (void*)WifiRead32;
				case 33: return (void*)WifiWrite32;
				}
			}
			break;
		case 0x06000000:
		case 0x06800000:
			switch (size | store)
			{
			case 8: return (void*)GPU::ReadVRAM_ARM7<u8>;
			case 9: return (void*)GPU::WriteVRAM_ARM7<u8>;
			case 16: return (void*)GPU::ReadVRAM_ARM7<u16>;
			case 17: return (void*)GPU::WriteVRAM_ARM7<u16>;
			case 32: return (void*)GPU::ReadVRAM_ARM7<u32>;
			case 33: return (void*)GPU::WriteVRAM_ARM7<u32>;
			}
		}
	}
	return NULL;
}

}