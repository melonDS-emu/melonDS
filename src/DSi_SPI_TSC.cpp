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

u8 Mode3Regs[0x80];

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

    memset(Mode3Regs, 0, 0x80);
    Mode3Regs[0x02] = 0x18;
    Mode3Regs[0x03] = 0x87;
    Mode3Regs[0x04] = 0x22;
    Mode3Regs[0x05] = 0x04;
    Mode3Regs[0x06] = 0x20;
    Mode3Regs[0x09] = 0x40;
    Mode3Regs[0x0E] = 0xAD;
    Mode3Regs[0x0F] = 0xA0;
    Mode3Regs[0x10] = 0x88;
    Mode3Regs[0x11] = 0x81;
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
printf("touching: %d/%d\n", x, y);
    u8 oldpress = Mode3Regs[0x0E] & 0x01;

    if (y == 0xFFF)
    {
        // released

        // TODO: GBAtek says it can also be 1000 or 3000??
        TouchX = 0x7000;
        TouchY = 0x7000;

        Mode3Regs[0x09] = 0x40;
        Mode3Regs[0x0E] |= 0x01;
    }
    else
    {
        // pressed

        TouchX <<= 4;
        TouchY <<= 4;

        Mode3Regs[0x09] = 0x80;
        Mode3Regs[0x0E] &= ~0x01;
    }

    if (oldpress ^ (Mode3Regs[0x0E] & 0x01))
    {
        TouchX |= 0x8000;
        TouchY |= 0x8000;
    }
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

    if (DataPos == 0)
    {
        Index = val;
    }
    else
    {
        u8 id = Index >> 1;

        if (id == 0)
        {
            READWRITE(Mode);
        }
        else if (Mode == 0x03)
        {
            if (Index & 0x01) Data = Mode3Regs[id];
            else
            {
                if (id == 0x0D || id == 0x0E)
                    Mode3Regs[id] = (Mode3Regs[id] & 0x03) | (val & 0xFC);
            }
        }
        else if ((Mode == 0xFC) && (Index & 0x01))
        {
            if (id < 0x0B)
            {
                // X coordinates

                if (id & 0x01) Data = TouchX >> 8;
                else           Data = TouchX & 0xFF;

                TouchX &= 0x7FFF;
            }
            else if (id < 0x15)
            {
                // Y coordinates

                if (id & 0x01) Data = TouchY >> 8;
                else           Data = TouchY & 0xFF;

                TouchY &= 0x7FFF; // checkme
            }
            else
            {
                // whatever (TODO)
                Data = 0;
            }
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
