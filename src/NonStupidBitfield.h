#ifndef NONSTUPIDBITFIELD_H
#define NONSTUPIDBITFIELD_H

#include "types.h"

#include <memory.h>

#include <initializer_list>
#include <algorithm>

// like std::bitset but less stupid and optimised for 
// our use case (keeping track of memory invalidations)

template <u32 Size>
struct NonStupidBitField
{
    static_assert((Size % 8) == 0, "bitfield size must be a multiple of 8");
    static const u32 DataLength = Size / 8;
    u8 Data[DataLength];

    struct Ref
    {
        NonStupidBitField<Size>& BitField;
        u32 Idx;

        operator bool()
        {
            return BitField.Data[Idx >> 3] & (1 << (Idx & 0x7));
        }

        Ref& operator=(bool set)
        {
            BitField.Data[Idx >> 3] &= ~(1 << (Idx & 0x7));
            BitField.Data[Idx >> 3] |= ((u8)set << (Idx & 0x7));
            return *this;
        }
    };

    struct Iterator
    {
        NonStupidBitField<Size>& BitField;
        u32 DataIdx;
        u32 BitIdx;
        u64 RemainingBits;

        u32 operator*() { return DataIdx * 8 + BitIdx; }

        bool operator==(const Iterator& other) { return other.DataIdx == DataIdx; }
        bool operator!=(const Iterator& other) { return other.DataIdx != DataIdx; }

        template <typename T>
        void Next()
        {
            while (RemainingBits == 0 && DataIdx < DataLength)
            {
                DataIdx += sizeof(T);
                RemainingBits = *(T*)&BitField.Data[DataIdx];
            }

            BitIdx = __builtin_ctzll(RemainingBits);
            RemainingBits &= ~(1ULL << BitIdx);
        }

        Iterator operator++(int)
        {
            Iterator prev(*this);
            ++*this;
            return prev;
        }

        Iterator& operator++()
        {
            if ((DataLength % 8) == 0)
                Next<u64>();
            else if ((DataLength % 4) == 0)
                Next<u32>();
            else if ((DataLength % 2) == 0)
                Next<u16>();
            else
                Next<u8>();

            return *this;
        }
    };

    NonStupidBitField(u32 start, u32 size)
    {
        memset(Data, 0, sizeof(Data));

        if (size == 0)
            return;

        u32 roundedStartBit = (start + 7) & ~7;
        u32 roundedEndBit = (start + size) & ~7;
        if (roundedStartBit != roundedEndBit)
            memset(Data + roundedStartBit / 8, 0xFF, (roundedEndBit - roundedStartBit) / 8);

        if (start & 0x7)
            Data[start >> 3] = 0xFF << (start & 0x7);
        if ((start + size) & 0x7)
            Data[(start + size) >> 3] = 0xFF >> ((start + size) & 0x7);
    }

    NonStupidBitField()
    {
        memset(Data, 0, sizeof(Data));
    }

    Iterator End()
    {
        return Iterator{*this, DataLength, 0, 0};
    }
    Iterator Begin()
    {
        if ((DataLength % 8) == 0)
            return ++Iterator{*this, 0, 0, *(u64*)Data};
        else if ((DataLength % 4) == 0)
            return ++Iterator{*this, 0, 0, *(u32*)Data};
        else if ((DataLength % 2) == 0)
            return ++Iterator{*this, 0, 0, *(u16*)Data};
        else
            return ++Iterator{*this, 0, 0, *Data};
    }

    Ref operator[](u32 idx)
    {
        return Ref{*this, idx};
    }

    NonStupidBitField& operator|=(const NonStupidBitField<Size>& other)
    {
        for (u32 i = 0; i < DataLength; i++)
        {
            Data[i] |= other.Data[i];
        }
        return *this;
    }
    NonStupidBitField& operator&=(const NonStupidBitField<Size>& other)
    {
        for (u32 i = 0; i < DataLength; i++)
        {
            Data[i] &= other.Data[i];
        }
        return *this;
    }
};


#endif