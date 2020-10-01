/*
    Copyright 2019 Arisotura, Buenia0

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
#include <string>
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
    // GBATEK: For detection, AD1 seems to be pulled low when reading from it... (while) the other AD lines are open bus (containing the halfword address)...
    // GBATEK with bit 6 set is based on empirical data...
    // This should allow commercial games to properly detect the Rumble Pak.
    // Credit to rzumer for coming up with this algorithm...
    
    u16 lodata = ((addr | 0x40) & 0xFD);
    return (addr & 1) ? addr : lodata;
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

// Code ported from Desmume

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
        GuitarKeyStatus |= static_cast<int>(key);
    }
    else
    {
        GuitarKeyStatus &= ~static_cast<int>(key);
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
}

void DoSavestate(Savestate* file)
{
    file->Section("MemExpansionPack");
    file->Var8((u8*)&MemPakRAMLock);
    file->VarArray(MemPakMemory, 0x800000);
}

}

namespace Slot2Cart_SegaCardReader
{
    
// Code ported from shonumi
    
bool Enabled = false;

u8 HCVControl = 0;
std::string HCVData = "________________";

u8 Read8(u32 addr)
{
    if (addr < 0x8020000)
    {
        if (addr & 1) return 0xFD;
        else return (0xF0 | ((addr & 0x1F) >> 1));
    }
    else if (addr == 0xA000000) return HCVControl;
    else if ((addr >= 0xA000010) && (addr <= 0xA00001F)) return HCVData[addr & 0xF];
    else return 0xFF;
}

void Write8(u32 addr, u8 val)
{
    if (addr == 0xA000000) HCVControl = val;
}

u16 Read16(u32 addr)
{
    return (((u16)Read8(addr+1) << 8) | (u16) Read8(addr));
}
    
u32 Read32(u32 addr)
{
    return (((u32)Read8(addr+3) << 24) | ((u32)Read8(addr+2) << 16) | ((u32)Read8(addr+1) << 8) | (u32) Read8(addr));
}

void Load(std::string barcode)
{
    HCVData = std::string(barcode, 0, 15) + std::string((16 - barcode.length()), '_');
    HCVControl = 0;
}

}
