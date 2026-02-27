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

#include "CartRetail.h"
#include "../NDS.h"
#include "../Utils.h"

// CartRetail: basic retail NDS cartridge (ROM + SRAM)

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace NDSCart
{

// SRAM TODO: emulate write delays???

CartRetail::CartRetail(const u8* rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata, melonDS::NDSCart::CartType type) :
    CartRetail(CopyToUnique(rom, len), len, chipid, badDSiDump, romparams, std::move(sram), sramlen, userdata, type)
{
}

CartRetail::CartRetail(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata, melonDS::NDSCart::CartType type) :
    CartCommon(std::move(rom), len, chipid, badDSiDump, romparams, type, userdata)
{
    LenientAddressing = false;

    u32 savememtype = ROMParams.SaveMemType <= 10 ? ROMParams.SaveMemType : 0;
    constexpr int sramlengths[] =
    {
        0,
        512,
        8192, 65536, 128*1024,
        256*1024, 512*1024, 1024*1024,
        8192*1024, 16384*1024, 65536*1024
    };
    SRAMLength = sramlengths[savememtype];

    if (SRAMLength)
    { // If this cart should have any save data...
        if (sram && sramlen == SRAMLength)
        { // If we were given save data that already has the correct length...
            SRAM = std::move(sram);
        }
        else
        { // Copy in what we can, truncate the rest.
            SRAM = std::make_unique<u8[]>(SRAMLength);
            memset(SRAM.get(), 0xFF, SRAMLength);

            if (sram)
            { // If we have anything to copy, that is.
                memcpy(SRAM.get(), sram.get(), std::min(sramlen, SRAMLength));
            }
        }
    }

    switch (savememtype)
    {
    case 1: SRAMType = 1; break; // EEPROM, small
    case 2:
    case 3:
    case 4: SRAMType = 2; break; // EEPROM, regular
    case 5:
    case 6:
    case 7: SRAMType = 3; break; // FLASH
    case 8:
    case 9:
    case 10: SRAMType = 4; break; // NAND
    default: SRAMType = 0; break; // ...whatever else
    }
}

CartRetail::~CartRetail() = default;
// std::unique_ptr cleans up the SRAM and ROM

void CartRetail::Reset()
{
    CartCommon::Reset();

    SRAMPos = 0;
    SRAMCmd = 0;
    SRAMAddr = 0;
    SRAMStatus = 0;

    SRAMSaveAddr = 0;
    SRAMSaveLen = 0;
}

void CartRetail::DoSavestate(Savestate* file)
{
    CartCommon::DoSavestate(file);

    // we reload the SRAM contents.
    // it should be the same file, but the contents may change

    u32 oldlen = SRAMLength;

    file->Var32(&SRAMLength);
    if (SRAMLength != oldlen)
    {
        Log(LogLevel::Warn, "savestate: VERY BAD!!!! SRAM LENGTH DIFFERENT. %d -> %d\n", oldlen, SRAMLength);
        Log(LogLevel::Warn, "oh well. loading it anyway. adsfgdsf\n");

        SRAM = SRAMLength ? std::make_unique<u8[]>(SRAMLength) : nullptr;
    }
    if (SRAMLength)
    {
        file->VarArray(SRAM.get(), SRAMLength);
    }

    // SPI status shito

    file->Var32(&SRAMPos);
    file->Var8(&SRAMCmd);
    file->Var32(&SRAMAddr);
    file->Var8(&SRAMStatus);

    file->Var32(&SRAMSaveAddr);
    file->Var32(&SRAMSaveLen);

    if ((!file->Saving) && SRAM)
        Platform::WriteNDSSave(SRAM.get(), SRAMLength, 0, SRAMLength, UserData);
}

void CartRetail::SetSaveMemory(const u8* savedata, u32 savelen)
{
    if (!SRAM) return;

    u32 len = std::min(savelen, SRAMLength);
    memcpy(SRAM.get(), savedata, len);
    Platform::WriteNDSSave(savedata, len, 0, len, UserData);
}

void CartRetail::SPISelect()
{
    SRAMPos = 0;
}

void CartRetail::SPIRelease()
{
    if ((SRAMStatus & (1<<1)) && (SRAMSaveLen > 0))
    {
        Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                               SRAMSaveAddr & (SRAMLength-1),
                               SRAMSaveLen & (SRAMLength-1),
                               UserData);

        SRAMStatus &= ~(1<<1);
        SRAMSaveAddr = 0;
        SRAMSaveLen = 0;
    }
}

u8 CartRetail::SPITransmitReceive(u8 val)
{
    if (SRAMType == 0) return 0;

    u8 ret = 0xFF;

    if (SRAMPos == 0)
    {
        // handle generic commands with no parameters
        switch (val)
        {
        case 0x04: // write disable
            SRAMStatus &= ~(1<<1);
            return 0;
        case 0x06: // write enable
            SRAMStatus |= (1<<1);
            return 0;

        default:
            SRAMCmd = val;
            SRAMAddr = 0;
            break;
        }
    }
    else
    {
        switch (SRAMType)
        {
            case 1: ret = SRAMWrite_EEPROMTiny(val); break;
            case 2: ret = SRAMWrite_EEPROM(val); break;
            case 3: ret = SRAMWrite_FLASH(val); break;
            default: break;
        }
    }

    SRAMPos++;
    return ret;
}

u8 CartRetail::SRAMWrite_EEPROMTiny(u8 val)
{
    switch (SRAMCmd)
    {
    case 0x01: // write status register
        // TODO: WP bits should be nonvolatile!
        if (SRAMPos == 1)
            SRAMStatus = (SRAMStatus & 0x01) | (val & 0x0C);
        return 0;

    case 0x05: // read status register
        return SRAMStatus | 0xF0;

    case 0x02: // write low
    case 0x0A: // write high
        if (SRAMPos < 2)
        {
            SRAMAddr = val;
            SRAMSaveAddr = SRAMAddr + ((SRAMCmd==0x0A)?0x100:0);
            SRAMSaveLen = 0;
        }
        else
        {
            // TODO: implement WP bits!
            // TODO: restrict writing to 16-byte page
            if (SRAMStatus & (1<<1))
            {
                SRAM[(SRAMAddr + ((SRAMCmd==0x0A)?0x100:0)) & 0x1FF] = val;
                SRAMSaveLen++;
            }
            SRAMAddr++;
        }
        return 0;

    case 0x03: // read low
    case 0x0B: // read high
        if (SRAMPos < 2)
        {
            SRAMAddr = val;
            return 0;
        }
        else
        {
            u8 ret = SRAM[(SRAMAddr + ((SRAMCmd==0x0B)?0x100:0)) & 0x1FF];
            SRAMAddr++;
            return ret;
        }

    case 0x9F: // read JEDEC ID
        return 0xFF;

    default:
        if (SRAMPos == 1)
            Log(LogLevel::Warn, "unknown tiny EEPROM save command %02X\n", SRAMCmd);
        return 0xFF;
    }
}

u8 CartRetail::SRAMWrite_EEPROM(u8 val)
{
    u32 addrsize = 2;
    if (SRAMLength > 65536) addrsize++;

    switch (SRAMCmd)
    {
    case 0x01: // write status register
        // TODO: WP bits should be nonvolatile!
        if (SRAMPos == 1)
            SRAMStatus = (SRAMStatus & 0x01) | (val & 0x0C);
        return 0;

    case 0x05: // read status register
        return SRAMStatus;

    case 0x02: // write
        if (SRAMPos <= addrsize)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            SRAMSaveAddr = SRAMAddr;
            SRAMSaveLen = 0;
        }
        else
        {
            // TODO: implement WP bits
            // TODO: restrict writing to page based on EEPROM size
            // except for FRAM????
            if (SRAMStatus & (1<<1))
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = val;
                SRAMSaveLen++;
            }
            SRAMAddr++;
        }
        return 0;

    case 0x03: // read
        if (SRAMPos <= addrsize)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            return 0;
        }
        else
        {
            // TODO: size limit!!
            u8 ret = SRAM[SRAMAddr & (SRAMLength-1)];
            SRAMAddr++;
            return ret;
        }

    case 0x9F: // read JEDEC ID
        // TODO: GBAtek implies it's not always all FF (FRAM)
        return 0xFF;

    default:
        if (SRAMPos == 1)
            Log(LogLevel::Warn, "unknown EEPROM save command %02X\n", SRAMCmd);
        return 0xFF;
    }
}

u8 CartRetail::SRAMWrite_FLASH(u8 val)
{
    // FLASH TODO: write support is done wrong!

    switch (SRAMCmd)
    {
    case 0x05: // read status register
        return SRAMStatus;

    case 0x02: // page program
        if (SRAMPos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            SRAMSaveAddr = SRAMAddr;
            SRAMSaveLen = 0;
        }
        else
        {
            if (SRAMStatus & (1<<1))
            {
                // CHECKME: should it be &=~val ??
                SRAM[SRAMAddr & (SRAMLength-1)] = 0;
                SRAMSaveLen++;
            }
            SRAMAddr++;
        }
        return 0;

    case 0x03: // read
        if (SRAMPos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            return 0;
        }
        else
        {
            u8 ret = SRAM[SRAMAddr & (SRAMLength-1)];
            SRAMAddr++;
            return ret;
        }

    case 0x0A: // page write
        if (SRAMPos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            SRAMSaveAddr = SRAMAddr;
            SRAMSaveLen = 0;
        }
        else
        {
            if (SRAMStatus & (1<<1))
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = val;
                SRAMSaveLen++;
            }
            SRAMAddr++;
        }
        return 0;

    case 0x0B: // fast read
        if (SRAMPos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            return 0;
        }
        else if (SRAMPos == 4)
        {
            // dummy byte
            return 0;
        }
        else
        {
            u8 ret = SRAM[SRAMAddr & (SRAMLength-1)];
            SRAMAddr++;
            return ret;
        }

    case 0x9F: // read JEDEC IC
        // GBAtek says it should be 0xFF. verify?
        return 0xFF;

    case 0xD8: // sector erase
        if (SRAMPos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            //SRAMFirstAddr = SRAMAddr;
            SRAMSaveAddr = SRAMAddr;
            SRAMSaveLen = 0;
        }
        if ((SRAMPos == 3) && (SRAMStatus & (1<<1)))
        {
            for (u32 i = 0; i < 0x10000; i++)
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = 0;
                SRAMAddr++;
            }
            SRAMSaveLen = 0x10000;
        }
        return 0;

    case 0xDB: // page erase
        if (SRAMPos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            SRAMSaveAddr = SRAMAddr;
            SRAMSaveLen = 0;
        }
        if ((SRAMPos == 3) && (SRAMStatus & (1<<1)))
        {
            for (u32 i = 0; i < 0x100; i++)
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = 0;
                SRAMAddr++;
            }
            SRAMSaveLen = 0x100;
        }
        return 0;

    default:
        if (SRAMPos == 1)
            Log(LogLevel::Warn, "unknown FLASH save command %02X\n", SRAMCmd);
        return 0xFF;
    }
}


}

}
