/*
    Copyright 2016-2021 Arisotura

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

namespace DSi_AES
{

extern u32 Cnt;

bool Init();
void DeInit();
void Reset();

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

void GetModcryptKey(u8* romheader, u8* key);
void ApplyModcrypt(u8* data, u32 len, u8* key, u8* iv);

}

#endif // DSI_AES_H
