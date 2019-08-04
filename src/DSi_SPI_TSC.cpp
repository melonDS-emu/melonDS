/*
    Copyright 2016-2019 Arisotura

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

#include <stdio.h>
#include <string.h>
#include "DSi.h"
#include "DSi_SPI_TSC.h"


namespace DSi_SPI_TSC
{

u32 DataPos;
u8 Index;
u8 Mode;
u8 Data;

u16 TouchX, TouchY;


bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    DataPos = 0;

    Mode = 0;
    Index = 0;
    Data = 0;
}

void DoSavestate(Savestate* file)
{
    /*file->Section("SPTi");

    file->Var32(&DataPos);
    file->Var8(&ControlByte);
    file->Var8(&Data);

    file->Var16(&ConvResult);*/
    // TODO!!
}

void SetTouchCoords(u16 x, u16 y)
{
    TouchX = x;
    TouchY = y;

    if (y == 0xFFF) return;

    TouchX <<= 4;
    TouchY <<= 4;
}

void MicInputFrame(s16* data, int samples)
{
    // TODO: forward to DS-mode TSC if needed
}

u8 Read()
{
    return Data;
}

void Write(u8 val, u32 hold)
{
#define READWRITE(var) { if (Index & 0x01) Data = var; else var = val; }
printf("TSC: %02X %d\n", val, hold?1:0);
    if (DataPos == 0)
    {
        Index = val;
    }
    else
    {
        if ((Index & 0xFE) == 0)
        {
            READWRITE(Mode);
        }
        else
        {
            printf("DSi_SPI_TSC: unknown IO, mode=%02X, index=%02X (%02X %s)\n", Mode, Index, Index>>1, (Index&1)?"read":"write");
        }

        Index += (1<<1); // increment index
    }

    if (hold) DataPos++;
    else      DataPos = 0;
}

}
