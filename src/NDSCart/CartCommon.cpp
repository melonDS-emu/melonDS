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

#include "CartCommon.h"
#include "../NDS.h"
#include "../Utils.h"

// CartCommon: base functionality common to all NDS cartridges

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace NDSCart
{

constexpr u32 ByteSwap(u32 val)
{
    return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | (val << 24);
}


CartCommon::CartCommon(const u8* rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, melonDS::NDSCart::CartType type, void* userdata) :
    CartCommon(CopyToUnique(rom, len), len, chipid, badDSiDump, romparams, type, userdata)
{
}

CartCommon::CartCommon(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, melonDS::NDSCart::CartType type, void* userdata) :
    ROM(std::move(rom)),
    ROMLength(len),
    ChipID(chipid),
    ROMParams(romparams),
    CartType(type),
    UserData(userdata)
{
    ROMMask = ROMLength - 1;

    memcpy(&Header, ROM.get(), sizeof(Header));
    IsDSi = Header.IsDSi() && !badDSiDump;
    DSiBase = Header.DSiRegionStart << 19;
}

CartCommon::~CartCommon() = default;

u32 CartCommon::Checksum() const
{
    const NDSHeader& header = GetHeader();
    u32 crc = CRC32(ROM.get(), 0x40);

    crc = CRC32(&ROM[header.ARM9ROMOffset], header.ARM9Size, crc);
    crc = CRC32(&ROM[header.ARM7ROMOffset], header.ARM7Size, crc);

    if (IsDSi)
    {
        crc = CRC32(&ROM[header.DSiARM9iROMOffset], header.DSiARM9iSize, crc);
        crc = CRC32(&ROM[header.DSiARM7iROMOffset], header.DSiARM7iSize, crc);
    }

    return crc;
}

void CartCommon::Reset()
{
    CmdEncMode = 0;
    DataEncMode = 0;
    DSiMode = false;

    memset(ROMCmd, 0, sizeof(ROMCmd));
    ROMAddr = 0;

    SPISelected = false;
}

void CartCommon::SetupDirectBoot(const std::string& romname, NDS& nds)
{
    CmdEncMode = 2;
    DataEncMode = 2;
    DSiMode = IsDSi && (nds.ConsoleType==1);
}

void CartCommon::DoSavestate(Savestate* file)
{
    file->Section("NDCS");

    file->Var32(&CmdEncMode);
    file->Var32(&DataEncMode);
    file->VarBool(&DSiMode);

    file->VarArray(ROMCmd, sizeof(ROMCmd));
    file->Var32(&ROMAddr);

    file->VarBool(&SPISelected);
}

u32 CartCommon::ROMRead32()
{
    u32 addrhi = ROMAddr & ROMMask & ~0xFFF;
    u32 addrlo = ROMAddr & 0xFFF;
    u32 ret;

    if (addrlo & 0x3)
    {
        ret = ROM[addrhi | addrlo];
        addrlo = (addrlo + 1) & 0xFFF;
        ret |= (ROM[addrhi | addrlo] << 8);
        addrlo = (addrlo + 1) & 0xFFF;
        ret |= (ROM[addrhi | addrlo] << 16);
        addrlo = (addrlo + 1) & 0xFFF;
        ret |= (ROM[addrhi | addrlo] << 24);
        addrlo = (addrlo + 1) & 0xFFF;
    }
    else
    {
        ret = *(u32*)&ROM[addrhi | addrlo];
        addrlo = (addrlo + 4) & 0xFFF;
    }

    ROMAddr = addrhi | addrlo;
    return ret;
}

void CartCommon::ROMCommandStart(NDSCartSlot& cartslot, const u8* cmd)
{
    if (CmdEncMode == 0)
    {
        memcpy(ROMCmd, cmd, 8);

        switch (ROMCmd[0])
        {
        case 0x00:
            // TODO: some more recent carts allow this, but older ones probably don't
            ROMAddr = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];
            ROMAddr &= 0xFFF;
            return;

        case 0x3C:
            CmdEncMode = 1;
            cartslot.Key1_InitKeycode(false, *(u32*)&ROM[0xC], 2, 2);
            DSiMode = false;
            return;

        case 0x3D:
            if (IsDSi)
            {
                CmdEncMode = 1;
                cartslot.Key1_InitKeycode(true, *(u32*)&ROM[0xC], 1, 2);
                DSiMode = true;
            }
            return;

        default:
            return;
        }
    }
    else if (CmdEncMode == 1)
    {
        // decrypt the KEY1 command as needed
        // (KEY2 commands do not need decrypted because KEY2 is handled entirely by hardware,
        // but KEY1 is not, so DS software is responsible for encrypting KEY1 commands)
        u8 cmddec[8];
        *(u32*)&cmddec[0] = ByteSwap(*(u32*)&cmd[4]);
        *(u32*)&cmddec[4] = ByteSwap(*(u32*)&cmd[0]);
        cartslot.Key1_Decrypt((u32*)cmddec);
        u32 tmp = ByteSwap(*(u32*)&cmddec[4]);
        *(u32*)&cmddec[4] = ByteSwap(*(u32*)&cmddec[0]);
        *(u32*)&cmddec[0] = tmp;

        memcpy(ROMCmd, cmddec, 8);

        // TODO eventually: verify all the command parameters and shit

        switch (ROMCmd[0] & 0xF0)
        {
        case 0x40:
            DataEncMode = 2;
            return;

        case 0x20:
            ROMAddr = (ROMCmd[2] & 0xF0) << 8;
            if (DSiMode)
            {
                // the DSi region starts with 0x3000 unreadable bytes
                // similarly to how the DS region starts at 0x1000 with 0x3000 unreadable bytes
                // these contain data for KEY1 crypto
                ROMAddr -= 0x1000;
                ROMAddr += DSiBase;
            }
            return;

        case 0xA0:
            CmdEncMode = 2;
            return;

        default:
            return;
        }
    }
    else if (CmdEncMode == 2)
    {
        memcpy(ROMCmd, cmd, 8);

        switch (ROMCmd[0])
        {
        case 0xB7:
            ROMAddr = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];
            if (!LenientAddressing)
            {
                // redirect incorrect addresses if needed

                if (ROMAddr < 0x8000)
                    ROMAddr = 0x8000 + (ROMAddr & 0x1FF);

                if (IsDSi && (ROMAddr >= DSiBase))
                {
                    // for DSi carts:
                    // * in DSi mode: block the first 0x3000 bytes of the DSi area
                    // * in DS mode: block the entire DSi area

                    if ((!DSiMode) || (ROMAddr < (DSiBase+0x3000)))
                        ROMAddr = 0x8000 + (ROMAddr & 0x1FF);
                }
            }
            return;

        default:
            return;
        }
    }
}

u32 CartCommon::ROMCommandReceive()
{
    u32 ret;

    if (CmdEncMode == 0)
    {
        switch (ROMCmd[0])
        {
        case 0x9F:
            return 0xFFFFFFFF;

        case 0x00:
            return ROMRead32();

        case 0x90:
            return ChipID;
        }
    }
    else if (CmdEncMode == 1)
    {
        switch (ROMCmd[0] & 0xF0)
        {
        case 0x10:
            return ChipID;

        case 0x20:
            return ROMRead32();
        }
    }
    else if (CmdEncMode == 2)
    {
        switch (ROMCmd[0])
        {
        case 0xB7:
            return ROMRead32();

        case 0xB8:
            return ChipID;
        }
    }

    return 0;
}

const NDSBanner* CartCommon::Banner() const
{
    const NDSHeader& header = GetHeader();
    size_t bannersize = header.IsDSi() ? 0x23C0 : 0xA40;
    if (header.BannerOffset >= 0x200 && header.BannerOffset < (ROMLength - bannersize))
    {
        return reinterpret_cast<const NDSBanner*>(ROM.get() + header.BannerOffset);
    }

    return nullptr;
}



}

}