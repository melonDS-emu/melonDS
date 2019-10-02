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
	branch_FollowCondNotTaken = 1 << 2
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
    u32 NextInstr[2];
	u32 Addr;

    u8 CodeCycles;
	u8 DataCycles;
	u8 DataRegion;

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
	u32 Length = 0; // make it 32 bit so we don't need movzx

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
			memcpy(newMem, Data, sizeof(Data) * Length);

		T* oldData = Data;
		Data = newMem;
		if (oldData != NULL)
			delete[] oldData;
		
		Capacity = capacity;
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
	JitBlock(u32 numInstrs, u32 numAddresses)
	{
		NumInstrs = numInstrs;
		NumAddresses = numAddresses;
		Data = new u32[numInstrs + numAddresses];
	}

	~JitBlock()
	{
		delete[] Data;
	}

	u32 StartAddr;
	u32 PseudoPhysicalAddr;
	
	u32 NumInstrs;
	u32 NumAddresses;

	JitBlockEntry EntryPoint;

	u32* Instrs()
	{ return Data; }
	u32* AddressRanges()
	{ return Data + NumInstrs; }

private:
	/*
		0..<NumInstrs - the instructions of the block
		NumInstrs..<(NumLinks + NumInstrs) - pseudo physical addresses where the block is located
			(atleast one, the pseudo physical address of the block)
	*/
	u32* Data;
};

// size should be 16 bytes because I'm to lazy to use mul and whatnot
struct __attribute__((packed)) AddressRange
{
	TinyVector<JitBlock*> Blocks;
	u16 TimesInvalidated;
};

extern AddressRange CodeRanges[ExeMemSpaceSize / 256];

typedef void (*InterpreterFunc)(ARM* cpu);
extern InterpreterFunc InterpretARM[];
extern InterpreterFunc InterpretTHUMB[];

void* GetFuncForAddr(ARM* cpu, u32 addr, bool store, int size);

}

#endif