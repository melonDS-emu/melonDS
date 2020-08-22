/*
    Copyright 2019 Arisotura, Raphaël Zumer

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Slot2Cart.h"
#include "CRC32.h"
#include "Platform.h"

namespace Slot2Cart_RumblePak
{

bool RumblePakEnabled = false;
u16 RumbleState = 0;

u16 ReadRumble(u32 addr)
{
    // Detection value taken from Desmume
		return 0xFFFD;
}

void WriteRumble(u32 addr, u16 val)
{
    // Ported from GBE+...
    if (addr == 0x8000000 || addr == 0x8001000 && RumbleState != val)
    {
        Platform::StopRumble();
        RumbleState = val;
        Platform::StartRumble();
    }
}

}

namespace Slot2Cart_GuitarGrip
{

bool GuitarGripEnabled = false;
u8 GuitarKeyStatus = 0x00;

u8 ReadGrip8(u32 addr)
{
    if (addr == 0xA000000)
    {
        return ~GuitarKeyStatus;
    }

    return ((addr & 1) ? 0xF9 : 0xFF);
}

u16 ReadGrip16(u32 addr)
{
    return 0xF9FF;
}

void SetGripKey(GuitarKeys key, bool val)
{
    if (val == true)
    {
        GuitarKeyStatus |= key;
    }
    else
    {
        GuitarKeyStatus &= ~key;
    }
}

}

namespace Slot2Cart_MemExpansionPak
{

// Code (mostly) ported from Desmume

bool MemPakEnabled = false;

u8 MemPakHeader[] = 
{
    0xFF, 0xFF, 0x96, 0x00,
    0x00, 0x24, 0x24, 0x24,
    0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x7F
};

u8 MemPakMemory[0x800000];
bool MemPakRAMLock = true;

u8 ReadMemPak8(u32 addr)
{
    if ((addr >= 0x80000B0)  && (addr < 0x80000C0))
    {
        return *(u8*)&MemPakHeader[(addr & 0xF)];
    }
    else if ((addr >= 0x9000000) && (addr < 0x9800000))
    {
        return *(u8*)&MemPakMemory[(addr - 0x9000000)];
    }
		
    return 0xFF;
}

void WriteMemPak8(u32 addr, u8 val)
{
    if (MemPakRAMLock == true)
    {
        return;
    }

    if ((addr >= 0x9000000) && (addr < 0x9800000))
    {
        *(u8*)&MemPakMemory[(addr - 0x9000000)] = val;
    }
    
    return;
}

u16 ReadMemPak16(u32 addr)
{
    if ((addr >= 0x80000B0)  && (addr < 0x80000C0))
    {
        return *(u16*)&MemPakHeader[(addr & 0xF)];
    }
    else if (addr == 0x801FFFC)
    {
        return 0x7FFF;
    }
    else if (addr == 0x8240002)
    {
        return 0x0000;
    }
    else if ((addr >= 0x9000000) && (addr < 0x9800000))
    {
        return *(u16*)&MemPakMemory[(addr - 0x9000000)];
    }

    return 0xFFFF;
}

void WriteMemPak16(u32 addr, u16 val)
{
    if (addr == 0x8240000)
    {
        MemPakRAMLock = !(val & 0x1);
        return;
    }

    if (MemPakRAMLock == true)
    {
        return;
    }

    if ((addr >= 0x9000000) && (addr < 0x9800000))
    {
        *(u16*)&MemPakMemory[(addr - 0x9000000)] = val;
    }
    
    return;
}

u32 ReadMemPak32(u32 addr)
{
    if ((addr >= 0x80000B0) && (addr < 0x80000C0))
    {
        return *(u32*)&MemPakHeader[(addr & 0xF)];
    }
    else if ((addr >= 0x9000000) && (addr < 0x9800000))
    {
        return *(u32*)&MemPakMemory[(addr - 0x9000000)];
    }
    
    return 0xFFFFFFFF;
}

void WriteMemPak32(u32 addr, u32 val)
{
    if (MemPakRAMLock == true)
    {
        return;
    }

    if ((addr >= 0x9000000) && (addr < 0x9800000))
    {
        *(u32*)&MemPakMemory[(addr - 0x9000000)] = val;
    }
    
    return;
}

void DoSavestate(Savestate* file)
{
    file->Section("MemExpansionPack");
    file->Var8((u8*)&MemPakRAMLock);
    file->VarArray(MemPakMemory, 0x800000);
}

}
