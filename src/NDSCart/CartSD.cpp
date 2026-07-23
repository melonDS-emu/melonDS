/*
    Copyright 2016-2026 melonDS team

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

#include "CartSD.h"
#include "../NDS.h"
#include "../Utils.h"

// CartSD: base class for homebrew cartridges with a SD card
// provides support for DLDI patching

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace NDSCart
{

CartSD::CartSD(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, void* userdata, std::optional<FATStorage>&& sdcard) :
    CartSD(CopyToUnique(rom, len), len, chipid, romparams, userdata, std::move(sdcard))
{}

CartSD::CartSD(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, void* userdata, std::optional<FATStorage>&& sdcard) :
    CartCommon(std::move(rom), len, chipid, false, romparams, CartType::Homebrew, userdata),
    SD(std::move(sdcard))
{
    LenientAddressing = true;

    sdcard = std::nullopt;
    // std::move on optionals usually results in an optional with a moved-from object
}

CartSD::~CartSD() = default;
// The SD card is destroyed by the optional's destructor


void CartSD::ApplyDLDIPatchAt(u8* binary, u32 dldioffset, const u8* patch, u32 patchlen, bool readonly) const
{
    if (patch[0x0D] > binary[dldioffset+0x0F])
    {
        Log(LogLevel::Error, "DLDI driver ain't gonna fit, sorry\n");
        return;
    }

    Log(LogLevel::Info, "existing driver is: %s\n", &binary[dldioffset+0x10]);
    Log(LogLevel::Info, "new driver is: %s\n", &patch[0x10]);

    u32 memaddr = *(u32*)&binary[dldioffset+0x40];
    if (memaddr == 0)
        memaddr = *(u32*)&binary[dldioffset+0x68] - 0x80;

    u32 patchbase = *(u32*)&patch[0x40];
    u32 delta = memaddr - patchbase;

    u32 patchsize = 1 << patch[0x0D];
    u32 patchend = patchbase + patchsize;

    memcpy(&binary[dldioffset], patch, patchlen);

    *(u32*)&binary[dldioffset+0x40] += delta;
    *(u32*)&binary[dldioffset+0x44] += delta;
    *(u32*)&binary[dldioffset+0x48] += delta;
    *(u32*)&binary[dldioffset+0x4C] += delta;
    *(u32*)&binary[dldioffset+0x50] += delta;
    *(u32*)&binary[dldioffset+0x54] += delta;
    *(u32*)&binary[dldioffset+0x58] += delta;
    *(u32*)&binary[dldioffset+0x5C] += delta;

    *(u32*)&binary[dldioffset+0x68] += delta;
    *(u32*)&binary[dldioffset+0x6C] += delta;
    *(u32*)&binary[dldioffset+0x70] += delta;
    *(u32*)&binary[dldioffset+0x74] += delta;
    *(u32*)&binary[dldioffset+0x78] += delta;
    *(u32*)&binary[dldioffset+0x7C] += delta;

    u8 fixmask = patch[0x0E];

    if (fixmask & 0x01)
    {
        u32 fixstart = *(u32*)&patch[0x40] - patchbase;
        u32 fixend = *(u32*)&patch[0x44] - patchbase;

        for (u32 addr = fixstart; addr < fixend; addr+=4)
        {
            u32 val = *(u32*)&binary[dldioffset+addr];
            if (val >= patchbase && val < patchend)
                *(u32*)&binary[dldioffset+addr] += delta;
        }
    }
    if (fixmask & 0x02)
    {
        u32 fixstart = *(u32*)&patch[0x48] - patchbase;
        u32 fixend = *(u32*)&patch[0x4C] - patchbase;

        for (u32 addr = fixstart; addr < fixend; addr+=4)
        {
            u32 val = *(u32*)&binary[dldioffset+addr];
            if (val >= patchbase && val < patchend)
                *(u32*)&binary[dldioffset+addr] += delta;
        }
    }
    if (fixmask & 0x04)
    {
        u32 fixstart = *(u32*)&patch[0x50] - patchbase;
        u32 fixend = *(u32*)&patch[0x54] - patchbase;

        for (u32 addr = fixstart; addr < fixend; addr+=4)
        {
            u32 val = *(u32*)&binary[dldioffset+addr];
            if (val >= patchbase && val < patchend)
                *(u32*)&binary[dldioffset+addr] += delta;
        }
    }
    if (fixmask & 0x08)
    {
        u32 fixstart = *(u32*)&patch[0x58] - patchbase;
        u32 fixend = *(u32*)&patch[0x5C] - patchbase;

        memset(&binary[dldioffset+fixstart], 0, fixend-fixstart);
    }

    if (readonly)
    {
        // clear the can-write feature flag
        binary[dldioffset+0x64] &= ~0x02;

        // make writeSectors() return failure
        u32 writesec_addr = *(u32*)&binary[dldioffset+0x74];
        writesec_addr -= memaddr;
        writesec_addr += dldioffset;
        *(u32*)&binary[writesec_addr+0x00] = 0xE3A00000; // mov r0, #0
        *(u32*)&binary[writesec_addr+0x04] = 0xE12FFF1E; // bx lr
    }

    Log(LogLevel::Debug, "applied DLDI patch at %08X\n", dldioffset);
}

void CartSD::ApplyDLDIPatch(const u8* patch, u32 patchlen, bool readonly)
{
    if (*(u32*)&patch[0] != 0xBF8DA5ED ||
        *(u32*)&patch[4] != 0x69684320 ||
        *(u32*)&patch[8] != 0x006D6873)
    {
        Log(LogLevel::Error, "bad DLDI patch\n");
        return;
    }

    u32 offset = *(u32*)&ROM[0x20];
    u32 size = *(u32*)&ROM[0x2C];

    u8* binary = &ROM[offset];

    for (u32 i = 0; i < size; )
    {
        if (*(u32*)&binary[i  ] == 0xBF8DA5ED &&
            *(u32*)&binary[i+4] == 0x69684320 &&
            *(u32*)&binary[i+8] == 0x006D6873)
        {
            Log(LogLevel::Debug, "DLDI structure found at %08X (%08X)\n", i, offset+i);
            ApplyDLDIPatchAt(binary, i, patch, patchlen, readonly);
            i += patchlen;
        }
        else
            i++;
    }
}


}

}
