#ifndef ARMJIT_INTERNAL_H
#define ARMJIT_INTERNAL_H

#include "types.h"
#include <stdint.h>

#include "ARMJIT.h"

// here lands everything which doesn't fit into ARMJIT.h
// where it would be included by pretty much everything
namespace ARMJIT
{

enum
{
	branch_IdleBranch = 1 << 0,
	branch_FollowCondTaken = 1 << 1,
	branch_FollowCondNotTaken = 1 << 2,
	branch_StaticTarget = 1 << 3,
};

struct FetchedInstr
{
    u32 A_Reg(int pos) const
    {
        return (Instr >> pos) & 0xF;
    }

    u32 T_Reg(int pos) const
    {
        return (Instr >> pos) & 0x7;
    }

    u32 Cond() const
    {
        return Instr >> 28;
    }

	u8 BranchFlags;
	u8 SetFlags;
    u32 Instr;
	u32 Addr;

	u8 DataCycles;
    u16 CodeCycles;
	u32 DataRegion;

    ARMInstrInfo::Info Info;
};

/*
	TinyVector
		- because reinventing the wheel is the best!
	
	- meant to be used very often, with not so many elements
	max 1 << 16 elements
	- doesn't allocate while no elements are inserted
	- not stl confirmant of course
	- probably only works with POD types
	- remove operations don't preserve order, but O(1)!
*/
template <typename T>
struct __attribute__((packed)) TinyVector
{
	T* Data = NULL;
	u16 Capacity = 0;
	u16 Length = 0;

	~TinyVector()
	{
		delete[] Data;
	}

	void MakeCapacity(u32 capacity)
	{
		assert(capacity <= UINT16_MAX);
		assert(capacity > Capacity);
		T* newMem = new T[capacity];
		if (Data != NULL)
			memcpy(newMem, Data, sizeof(T) * Length);

		T* oldData = Data;
		Data = newMem;
		if (oldData != NULL)
			delete[] oldData;
		
		Capacity = capacity;
	}

	void SetLength(u16 length)
	{
		if (Capacity < length)
			MakeCapacity(length);
		
		Length = length;
	}

	void Clear()
	{
		Length = 0;
	}

	void Add(T element)
	{
		assert(Length + 1 <= UINT16_MAX);
		if (Length + 1 > Capacity)
			MakeCapacity(((Capacity + 4) * 3) / 2);
		
		Data[Length++] = element;
	}

	void Remove(int index)
	{
		assert(index >= 0 && index < Length);

		Length--;
		Data[index] = Data[Length];
		/*for (int i = index; i < Length; i++)
			Data[i] = Data[i + 1];*/
	}

	int Find(T needle)
	{
		for (int i = 0; i < Length; i++)
		{
			if (Data[i] == needle)
				return i;
		}
		return -1;
	}

	bool RemoveByValue(T needle)
	{
		for (int i = 0; i < Length; i++)
		{
			if (Data[i] == needle)
			{
				Remove(i);
				return true;
			}
		}
		return false;
	}

	T& operator[](int index)
	{
		assert(index >= 0 && index < Length);
		return Data[index];
	}
};

class JitBlock
{
public:
	JitBlock(u32 num, u32 literalHash, u32 numAddresses, u32 numLiterals)
	{
		Num = num;
		NumAddresses = numAddresses;
		NumLiterals = numLiterals;
		Data.SetLength(numAddresses * 2 + numLiterals);
	}

	u32 PseudoPhysicalAddr;

	u32 InstrHash, LiteralHash;
	u8 Num;
	u16 NumAddresses;
	u16 NumLiterals;

	JitBlockEntry EntryPoint;

	u32* AddressRanges()
	{ return &Data[0]; }
	u32* AddressMasks()
	{ return &Data[NumAddresses]; }
	u32* Literals()
	{ return &Data[NumAddresses * 2]; }
	u32* Links()
	{ return &Data[NumAddresses * 2 + NumLiterals]; }

	u32 NumLinks()
	{ return Data.Length - NumAddresses * 2 - NumLiterals; }

	void AddLink(u32 link)
	{
		Data.Add(link);
	}

	void ResetLinks()
	{
		Data.SetLength(NumAddresses * 2 + NumLiterals);
	}

private:
	/*
		0..<NumInstrs - the instructions of the block
		NumInstrs..<(NumLinks + NumInstrs) - pseudo physical addresses where the block is located
			(atleast one, the pseudo physical address of the block)
	*/
	TinyVector<u32> Data;
};

// size should be 16 bytes because I'm to lazy to use mul and whatnot
struct __attribute__((packed)) AddressRange
{
	TinyVector<JitBlock*> Blocks;
	u32 Code;
};

extern AddressRange CodeRanges[ExeMemSpaceSize / 512];

typedef void (*InterpreterFunc)(ARM* cpu);
extern InterpreterFunc InterpretARM[];
extern InterpreterFunc InterpretTHUMB[];

extern u8 MemoryStatus9[0x800000];
extern u8 MemoryStatus7[0x800000];

extern TinyVector<u32> InvalidLiterals;

void* GetFuncForAddr(ARM* cpu, u32 addr, bool store, int size);

template <u32 Num>
void LinkBlock(ARM* cpu, u32 codeOffset);

enum
{
	memregion_Other = 0,
	memregion_ITCM,
	memregion_DTCM,
	memregion_BIOS9,
	memregion_MainRAM,
	memregion_SWRAM9,
	memregion_SWRAM7,
	memregion_IO9,
	memregion_VRAM,
	memregion_BIOS7,
	memregion_WRAM7,
	memregion_IO7,
	memregion_Wifi,
	memregion_VWRAM,
};

int ClassifyAddress9(u32 addr);
int ClassifyAddress7(u32 addr);

template <typename T> T SlowRead9(ARMv5* cpu, u32 addr);
template <typename T> void SlowWrite9(ARMv5* cpu, u32 addr, T val);
template <typename T> T SlowRead7(u32 addr);
template <typename T> void SlowWrite7(u32 addr, T val);

template <bool PreInc, bool Write> void SlowBlockTransfer9(u32 addr, u64* data, u32 num, ARMv5* cpu);
template <bool PreInc, bool Write> void SlowBlockTransfer7(u32 addr, u64* data, u32 num);

}

#endif