#ifndef ARMJIT_INTERNAL_H
#define ARMJIT_INTERNAL_H

#include "types.h"
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "ARMJIT.h"
#include "ARMJIT_Memory.h"

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
        assert(Length > 0);
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

    u32 StartAddr;
    u32 StartAddrLocal;
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

private:
    TinyVector<u32> Data;
};

// size should be 16 bytes because I'm to lazy to use mul and whatnot
struct __attribute__((packed)) AddressRange
{
    TinyVector<JitBlock*> Blocks;
    u32 Code;
};


typedef void (*InterpreterFunc)(ARM* cpu);
extern InterpreterFunc InterpretARM[];
extern InterpreterFunc InterpretTHUMB[];

extern TinyVector<u32> InvalidLiterals;

extern AddressRange* const CodeMemRegions[ARMJIT_Memory::memregions_Count];

inline bool PageContainsCode(AddressRange* range)
{
    for (int i = 0; i < 8; i++)
    {
        if (range[i].Blocks.Length > 0)
            return true;
    }
    return false;
}

u32 LocaliseCodeAddress(u32 num, u32 addr);

template <u32 Num>
void LinkBlock(ARM* cpu, u32 codeOffset);

template <typename T, int ConsoleType> T SlowRead9(u32 addr, ARMv5* cpu);
template <typename T, int ConsoleType> void SlowWrite9(u32 addr, ARMv5* cpu, u32 val);
template <typename T, int ConsoleType> T SlowRead7(u32 addr);
template <typename T, int ConsoleType> void SlowWrite7(u32 addr, u32 val);

template <bool Write, int ConsoleType> void SlowBlockTransfer9(u32 addr, u64* data, u32 num, ARMv5* cpu);
template <bool Write, int ConsoleType> void SlowBlockTransfer7(u32 addr, u64* data, u32 num);

}

#endif