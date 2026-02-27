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

#include "CartHomebrew.h"
#include "../NDS.h"
#include "melonDLDI.h"

// CartHomebrew: high-level "flashcart" for homebrew ROMs
// implements simple SD card access commands, which are used by the melonDS DLDI driver
// this class does not reflect any actual hardware

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace NDSCart
{

CartHomebrew::CartHomebrew(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, void* userdata, std::optional<FATStorage>&& sdcard) :
    CartSD(rom, len, chipid, romparams, userdata, std::move(sdcard))
{}

CartHomebrew::CartHomebrew(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, void* userdata, std::optional<FATStorage>&& sdcard) :
    CartSD(std::move(rom), len, chipid, romparams, userdata, std::move(sdcard))
{}

CartHomebrew::~CartHomebrew() = default;

void CartHomebrew::Reset()
{
    CartSD::Reset();

    if (SD)
        ApplyDLDIPatch(melonDLDI, sizeof(melonDLDI), SD->IsReadOnly());
}

void CartHomebrew::SetupDirectBoot(const std::string& romname, NDS& nds)
{
    CartCommon::SetupDirectBoot(romname, nds);

    if (SD)
    {
        // add the ROM to the SD volume

        if (!SD->InjectFile(romname, ROM.get(), ROMLength))
            return;

        // setup argv command line

        char argv[512] = {0};
        int argvlen;

        strncpy(argv, "fat:/", 511);
        strncat(argv, romname.c_str(), 511);
        argvlen = strlen(argv);

        const NDSHeader& header = GetHeader();

        u32 argvbase = header.ARM9RAMAddress + header.ARM9Size;
        argvbase = (argvbase + 0xF) & ~0xF;

        for (u32 i = 0; i <= argvlen; i+=4)
            nds.ARM9Write32(argvbase+i, *(u32*)&argv[i]);

        nds.ARM9Write32(0x02FFFE70, 0x5F617267);
        nds.ARM9Write32(0x02FFFE74, argvbase);
        nds.ARM9Write32(0x02FFFE78, argvlen+1);
        // The DSi version of ARM9Write32 will be called if nds is really a DSi
    }
}

void CartHomebrew::ROMCommandStart(NDSCart::NDSCartSlot& cartslot, const u8* cmd)
{
    if (CmdEncMode != 2) return CartSD::ROMCommandStart(cartslot, cmd);

    memcpy(ROMCmd, cmd, 8);

    switch (ROMCmd[0])
    {
    case 0xC0: // SD read
        SectorAddr = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];
        if (SD) SD->ReadSectors(SectorAddr, 1, SectorBuffer);
        SectorPos = 0;
        return;

    case 0xC1: // SD write
        SectorAddr = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];
        SectorPos = 0;
        return;

    default:
        return CartSD::ROMCommandStart(cartslot, cmd);
    }
}

u32 CartHomebrew::ROMCommandReceive()
{
    if (CmdEncMode != 2) return CartSD::ROMCommandReceive();

    switch (ROMCmd[0])
    {
    case 0xC0:
        {
            u32 ret = *(u32*)&SectorBuffer[SectorPos];
            SectorPos += 4;
            if (SectorPos >= 512)
            {
                SectorAddr++;
                if (SD) SD->ReadSectors(SectorAddr, 1, SectorBuffer);
                SectorPos = 0;
            }
            return ret;
        }

    default:
        return CartSD::ROMCommandReceive();
    }
}

void CartHomebrew::ROMCommandTransmit(u32 val)
{
    if (CmdEncMode != 2) return CartSD::ROMCommandTransmit(val);

    switch (ROMCmd[0])
    {
    case 0xC1:
        *(u32*)&SectorBuffer[SectorPos] = val;
        SectorPos += 4;
        if (SectorPos >= 512)
        {
            if (SD && !SD->IsReadOnly()) SD->WriteSectors(SectorAddr, 1, SectorBuffer);
            SectorAddr++;
            SectorPos = 0;
        }
        return;

    default:
        return CartSD::ROMCommandTransmit(val);
    }
}

void CartHomebrew::ROMCommandFinish()
{
    if (CmdEncMode != 2) return CartCommon::ROMCommandFinish();

    // TODO: delayed SD writing? like we have for SRAM

    switch (ROMCmd[0])
    {
    case 0xC1:
        if (SectorPos < 512)
            printf("incomplete SD write command?? len=%d\n", SectorPos);
        //if (SD && !SD->IsReadOnly()) SD->WriteSectors(SectorAddr, 1, SectorBuffer);
        return;

    default:
        return CartCommon::ROMCommandFinish();
    }
}


}

}
