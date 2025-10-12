/*
    Copyright 2016-2025 melonDS team, RSDuck

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef NONSTUPIDBITFIELD_H
#define NONSTUPIDBITFIELD_H

#include "types.h"

#include <memory.h>

#include <initializer_list>
#include <algorithm>

namespace melonDS
{

inline u64 GetRangedBitMask(u32 idx, u32 startBit, u32 bitsCount)
{
    u32 startEntry = startBit >> 6;
    u64 entriesCount = ((startBit + bitsCount + 0x3F) >> 6) - startEntry;

    if (entriesCount > 1)
    {
        if (idx == startEntry)
            return 0xFFFFFFFFFFFFFFFF << (startBit & 0x3F);
        if (((startBit + bitsCount) & 0x3F) && idx == startEntry + entriesCount - 1)
            return ~(0xFFFFFFFFFFFFFFFF << ((startBit + bitsCount) & 0x3F));

        return 0xFFFFFFFFFFFFFFFF;
    }
    else if (idx == startEntry)
    {
        return bitsCount == 64
            ? 0xFFFFFFFFFFFFFFFF
            : ((1ULL << bitsCount) - 1) << (startBit & 0x3F);
    }
    else
    {
        return 0;
    }
}

// like std::bitset but less stupid and optimised for 
// our use case (keeping track of memory invalidations)

template <u32 Size>
struct NonStupidBitField
{
    static constexpr u32 DataLength = (Size + 0x3F) >> 6;
    u64 Data[DataLength];

    struct Ref
    {
        NonStupidBitField<Size>& BitField;
        u32 Idx;

        operator bool() const
        {
            return BitField.Data[Idx >> 6] & (1ULL << (Idx & 0x3F));
        }

        Ref& operator=(bool set)
        {
            BitField.Data[Idx >> 6] &= ~(1ULL << (Idx & 0x3F));
            BitField.Data[Idx >> 6] |= ((u64)set << (Idx & 0x3F));
            return *this;
        }
    };

    struct Iterator
    {
        NonStupidBitField<Size>& BitField;
        u32 DataIdx;
        u32 BitIdx;
        u64 RemainingBits;

        u32 operator*() const { return DataIdx * 64 + BitIdx; }

        bool operator==(const Iterator& other) const
        {
            return other.DataIdx == DataIdx;
        }
        bool operator!=(const Iterator& other) const
        {
            return other.DataIdx != DataIdx;
        }

        void Next()
        {
            if (RemainingBits == 0)
            {
                for (u32 i = DataIdx + 1; i < DataLength; i++)
                {
                    if (BitField.Data[i])
                    {
                        DataIdx = i;
                        RemainingBits = BitField.Data[i];
                        goto done;
                    }
                }
                DataIdx = DataLength;
                return;
            done:;
            }

            BitIdx = __builtin_ctzll(RemainingBits);
            RemainingBits &= ~(1ULL << BitIdx);

            if ((Size & 0x3F) && BitIdx >= Size)
                DataIdx = DataLength;
        }

        Iterator operator++(int)
        {
            Iterator prev(*this);
            ++*this;
            return prev;
        }

        Iterator& operator++()
        {
            Next();
            return *this;
        }
    };

    NonStupidBitField(u32 startBit, u32 bitsCount)
    {
        Clear();

        if (bitsCount == 0)
            return;

        SetRange(startBit, bitsCount);
        /*for (int i = 0; i < Size; i++)
        {
            bool state = (*this)[i];
            if (state != (i >= startBit && i < startBit + bitsCount))
            {
                for (u32 j = 0; j < DataLength; j++)
                    printf("data %016lx\n", Data[j]);
                printf("blarg %d %d %d %d\n", i, startBit, bitsCount, Size);
                abort();
            }
        }*/
    }

    NonStupidBitField()
    {
        Clear();
    }

    Iterator End()
    {
        return Iterator{*this, DataLength, 0, 0};
    }
    Iterator Begin()
    {
        for (u32 i = 0; i < DataLength; i++)
        {
            if (Data[i])
            {
                u32 idx = __builtin_ctzll(Data[i]);
                if (idx + i * 64 < Size)
                    return {*this, i, idx, Data[i] & ~(1ULL << idx)};
            }
        }
        return End();
    }

    void Clear()
    {
        memset(Data, 0, sizeof(Data));
    }

    Ref operator[](u32 idx)
    {
        return Ref{*this, idx};
    }

    bool operator[](u32 idx) const
    {
        return Data[idx >> 6] & (1ULL << (idx & 0x3F));
    }

    void SetRange(u32 startBit, u32 bitsCount)
    {
        u32 startEntry = startBit >> 6;
        u64 entriesCount = (((startBit + bitsCount + 0x3F) & ~0x3F) >> 6) - startEntry;

        if (entriesCount > 1)
        {
            Data[startEntry] |= 0xFFFFFFFFFFFFFFFF << (startBit & 0x3F);
            if ((startBit + bitsCount) & 0x3F)
                Data[startEntry + entriesCount - 1] |= ~(0xFFFFFFFFFFFFFFFF << ((startBit + bitsCount) & 0x3F));
            else
                Data[startEntry + entriesCount - 1] = 0xFFFFFFFFFFFFFFFF;
            for (u64 i = startEntry + 1; i < startEntry + entriesCount - 1; i++)
                Data[i] = 0xFFFFFFFFFFFFFFFF;
        }
        else
        {
            Data[startEntry] |= ((1ULL << bitsCount) - 1) << (startBit & 0x3F);
        }
    }

    int Min() const
    {
        for (int i = 0; i < DataLength; i++)
        {
            if (Data[i])
                return i * 64 + __builtin_ctzll(Data[i]);
        }
        return -1;
    }

    int Max() const
    {
        for (int i = DataLength - 1; i >= 0; i--)
        {
            if (Data[i])
                return i * 64 + (63 - __builtin_clzll(Data[i]));
        }
        return -1;
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

    operator bool() const
    {
        for (int i = 0; i < DataLength - 1; i++)
        {
            if (Data[i])
                return true;
        }
        if (Data[DataLength-1] & ((Size&0x3F) ? ~(0xFFFFFFFFFFFFFFFF << (Size&0x3F)) : 0xFFFFFFFFFFFFFFFF))
        {
            return true;
        }
        return false;
    }
};

}

#endif
