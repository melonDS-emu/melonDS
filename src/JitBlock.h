/*
    Copyright 2016-2023 melonDS team

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

#ifndef MELONDS_JITBLOCK_H
#define MELONDS_JITBLOCK_H

#include "types.h"
#include "TinyVector.h"

namespace melonDS
{
typedef void (*JitBlockEntry)();

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

    const u32* AddressRanges() const { return &Data[0]; }
    u32* AddressRanges() { return &Data[0]; }
    const u32* AddressMasks() const { return &Data[NumAddresses]; }
    u32* AddressMasks() { return &Data[NumAddresses]; }
    const u32* Literals() const { return &Data[NumAddresses * 2]; }
    u32* Literals() { return &Data[NumAddresses * 2]; }

private:
    TinyVector<u32> Data;
};
}

#endif //MELONDS_JITBLOCK_H
