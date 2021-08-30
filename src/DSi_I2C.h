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

#ifndef DSI_I2C_H
#define DSI_I2C_H

namespace DSi_BPTWL
{

u8 GetBootFlag();

}

namespace DSi_I2C
{

extern u8 Cnt;

bool Init();
void DeInit();
void Reset();
//void DoSavestate(Savestate* file);

void WriteCnt(u8 val);

u8 ReadData();
void WriteData(u8 val);

//void TransferDone(u32 param);

}

#endif // DSI_I2C_H
