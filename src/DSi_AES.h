/*
    Copyright 2016-2022 melonDS team

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

#ifndef DSI_AES_H
#define DSI_AES_H

#include "types.h"
#include "Savestate.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#if defined(__GNUC__) && (__GNUC__ >= 11) // gcc 11.*
// NOTE: Yes, the compiler does *not* recognize this code pattern, so it is indeed an optimization.
__attribute((always_inline)) static void Bswap128(void* Dst, const void* Src)
{
    *(__int128*)Dst = __builtin_bswap128(*(__int128*)Src);
}
#else
__attribute((always_inline)) static void Bswap128(void* Dst, const void* Src)
{
    for (int i = 0; i < 16; ++i) 
    { 
        ((u8*)Dst)[i] = ((u8*)Src)[15 - i]; 
    }
}
#endif
#pragma GCC diagnostic pop

namespace DSi_AES
{

extern u32 Cnt;

bool Init();
void DeInit();
void Reset();

void DoSavestate(Savestate* file);

u32 ReadCnt();
void WriteCnt(u32 val);
void WriteBlkCnt(u32 val);

u32 ReadOutputFIFO();
void WriteInputFIFO(u32 val);
void CheckInputDMA();
void CheckOutputDMA();
void Update();

void WriteIV(u32 offset, u32 val, u32 mask);
void WriteMAC(u32 offset, u32 val, u32 mask);
void WriteKeyNormal(u32 slot, u32 offset, u32 val, u32 mask);
void WriteKeyX(u32 slot, u32 offset, u32 val, u32 mask);
void WriteKeyY(u32 slot, u32 offset, u32 val, u32 mask);

void DeriveNormalKey(u8* keyX, u8* keyY, u8* normalkey);

}

#endif // DSI_AES_H
