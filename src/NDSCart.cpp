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

#include <string.h>
#include "NDS.h"
#include "DSi.h"
#include "NDSCart.h"
#include "CRC32.h"
#include "Platform.h"
#include "ROMList.h"
#include "melonDLDI.h"
#include "FATStorage.h"
#include "Utils.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace NDSCart
{

enum
{
    ROMTransfer_PrepareData = 0,
    ROMTransfer_End
};

// SRAM TODO: emulate write delays???

constexpr u32 ByteSwap(u32 val)
{
    return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | (val << 24);
}

void NDSCartSlot::Key1_Encrypt(u32* data) const noexcept
{
    u32 y = data[0];
    u32 x = data[1];
    u32 z;

    for (u32 i = 0x0; i <= 0xF; i++)
    {
        z = Key1_KeyBuf[i] ^ x;
        x =  Key1_KeyBuf[0x012 +  (z >> 24)        ];
        x += Key1_KeyBuf[0x112 + ((z >> 16) & 0xFF)];
        x ^= Key1_KeyBuf[0x212 + ((z >>  8) & 0xFF)];
        x += Key1_KeyBuf[0x312 +  (z        & 0xFF)];
        x ^= y;
        y = z;
    }

    data[0] = x ^ Key1_KeyBuf[0x10];
    data[1] = y ^ Key1_KeyBuf[0x11];
}

void NDSCartSlot::Key1_Decrypt(u32* data) const noexcept
{
    u32 y = data[0];
    u32 x = data[1];
    u32 z;

    for (u32 i = 0x11; i >= 0x2; i--)
    {
        z = Key1_KeyBuf[i] ^ x;
        x =  Key1_KeyBuf[0x012 +  (z >> 24)        ];
        x += Key1_KeyBuf[0x112 + ((z >> 16) & 0xFF)];
        x ^= Key1_KeyBuf[0x212 + ((z >>  8) & 0xFF)];
        x += Key1_KeyBuf[0x312 +  (z        & 0xFF)];
        x ^= y;
        y = z;
    }

    data[0] = x ^ Key1_KeyBuf[0x1];
    data[1] = y ^ Key1_KeyBuf[0x0];
}

void NDSCartSlot::Key1_ApplyKeycode(u32* keycode, u32 mod) noexcept
{
    Key1_Encrypt(&keycode[1]);
    Key1_Encrypt(&keycode[0]);

    u32 temp[2] = {0,0};

    for (u32 i = 0; i <= 0x11; i++)
    {
        Key1_KeyBuf[i] ^= ByteSwap(keycode[i % mod]);
    }
    for (u32 i = 0; i <= 0x410; i+=2)
    {
        Key1_Encrypt(temp);
        Key1_KeyBuf[i  ] = temp[1];
        Key1_KeyBuf[i+1] = temp[0];
    }
}

void NDSCartSlot::Key1_LoadKeyBuf(bool dsimode) noexcept
{
    if (NDS.ConsoleType == 1)
    {
        // DSi mode: grab the right key depending on the requested cart mode

        auto& dsi = static_cast<DSi&>(NDS);
        if (dsimode)
        {
            // load from ARM7 BIOS at 0xC6D0

            const u8* bios = dsi.ARM7iBIOS.data();
            memcpy(Key1_KeyBuf.data(), bios + 0xC6D0, sizeof(Key1_KeyBuf));
            Platform::Log(LogLevel::Debug, "NDSCart: Initialized Key1_KeyBuf from ARM7i BIOS\n");
        }
        else
        {
            // load from ARM9 BIOS at 0x99A0

            const u8* bios = dsi.ARM9iBIOS.data();
            memcpy(Key1_KeyBuf.data(), bios + 0x99A0, sizeof(Key1_KeyBuf));
            Platform::Log(LogLevel::Debug, "NDSCart: Initialized Key1_KeyBuf from ARM9i BIOS\n");
        }
    }
    else
    {
        // DS mode: load from ARM7 BIOS at 0x0030

        if (NDS.IsLoadedARM7BIOSKnownNative())
        {
            const u8* bios = NDS.GetARM7BIOS().data();
            memcpy(Key1_KeyBuf.data(), bios + 0x0030, sizeof(Key1_KeyBuf));
            Platform::Log(LogLevel::Debug, "NDSCart: Initialized Key1_KeyBuf from ARM7 BIOS\n");
        }
        else
        {
            // well
            memset(Key1_KeyBuf.data(), 0, sizeof(Key1_KeyBuf));
            Platform::Log(LogLevel::Debug, "NDSCart: Initialized Key1_KeyBuf to zero\n");
        }
    }
}

void NDSCartSlot::Key1_InitKeycode(bool dsi, u32 idcode, u32 level, u32 mod) noexcept
{
    Key1_LoadKeyBuf(dsi);

    u32 keycode[3] = {idcode, idcode>>1, idcode<<1};
    if (level >= 1) Key1_ApplyKeycode(keycode, mod);
    if (level >= 2) Key1_ApplyKeycode(keycode, mod);
    if (level >= 3)
    {
        keycode[1] <<= 1;
        keycode[2] >>= 1;
        Key1_ApplyKeycode(keycode, mod);
    }
}


void NDSCartSlot::Key2_Encrypt(const u8* data, u32 len) noexcept
{
    for (u32 i = 0; i < len; i++)
    {
        Key2_X = (((Key2_X >> 5) ^
                   (Key2_X >> 17) ^
                   (Key2_X >> 18) ^
                   (Key2_X >> 31)) & 0xFF)
                 + (Key2_X << 8);
        Key2_Y = (((Key2_Y >> 5) ^
                   (Key2_Y >> 23) ^
                   (Key2_Y >> 18) ^
                   (Key2_Y >> 31)) & 0xFF)
                 + (Key2_Y << 8);

        Key2_X &= 0x0000007FFFFFFFFFULL;
        Key2_Y &= 0x0000007FFFFFFFFFULL;
    }
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

void CartCommon::ROMCommandStart(NDS& nds, NDSCartSlot& cartslot, const u8* cmd)
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

/*void CartCommon::ReadROM(u32 addr, u32 len, u8* data, u32 offset) const
{
    if (addr >= ROMLength) return;
    if ((addr+len) > ROMLength)
        len = ROMLength - addr;

    memcpy(data+offset, ROM.get()+addr, len);
}*/

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
    if (SRAMStatus & (1<<1))
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

void CartRetail::ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset) const
{
    addr &= (ROMLength-1);

    if (addr < 0x8000)
        addr = 0x8000 + (addr & 0x1FF);

    if (IsDSi && (addr >= DSiBase))
    {
        // for DSi carts:
        // * in DSi mode: block the first 0x3000 bytes of the DSi area
        // * in DS mode: block the entire DSi area

        if ((!DSiMode) || (addr < (DSiBase+0x3000)))
            addr = 0x8000 + (addr & 0x1FF);
    }

    memcpy(data+offset, ROM.get()+addr, len);
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
            //SRAMFirstAddr = SRAMAddr;
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
        /*if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   (SRAMFirstAddr + ((SRAMCmd==0x0A)?0x100:0)) & 0x1FF, SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }*/
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
            //SRAMFirstAddr = SRAMAddr;
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
        /*if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   SRAMFirstAddr & (SRAMLength-1), SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }*/
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
    switch (SRAMCmd)
    {
    case 0x05: // read status register
        return SRAMStatus;

    case 0x02: // page program
        if (SRAMPos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            //SRAMFirstAddr = SRAMAddr;
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
        /*if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   SRAMFirstAddr & (SRAMLength-1), SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }*/
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
            //SRAMFirstAddr = SRAMAddr;
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
        /*if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   SRAMFirstAddr & (SRAMLength-1), SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }*/
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
        /*if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   SRAMFirstAddr & (SRAMLength-1), SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }*/
        return 0;

    case 0xDB: // page erase
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
            for (u32 i = 0; i < 0x100; i++)
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = 0;
                SRAMAddr++;
            }
            SRAMSaveLen = 0x100;
        }
        /*if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   SRAMFirstAddr & (SRAMLength-1), SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }*/
        return 0;

    default:
        if (SRAMPos == 1)
            Log(LogLevel::Warn, "unknown FLASH save command %02X\n", SRAMCmd);
        return 0xFF;
    }
}

CartRetailNAND::CartRetailNAND(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata) :
    CartRetailNAND(CopyToUnique(rom, len), len, chipid, romparams, std::move(sram), sramlen, userdata)
{
}

CartRetailNAND::CartRetailNAND(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata) :
    CartRetail(std::move(rom), len, chipid, false, romparams, std::move(sram), sramlen, userdata, CartType::RetailNAND)
{
    BuildSRAMID();
}

CartRetailNAND::~CartRetailNAND() = default;

void CartRetailNAND::Reset()
{
    CartRetail::Reset();

    SRAMAddr = 0;
    SRAMStatus = 0x20;
    SRAMWindow = 0;

    // ROM header 94/96 = SRAM addr start / 0x20000
    SRAMBase = *(u16*)&ROM[0x96] << 17;

    memset(SRAMWriteBuffer, 0, 0x800);
}

void CartRetailNAND::DoSavestate(Savestate* file)
{
    CartRetail::DoSavestate(file);

    file->Var32(&SRAMBase);
    file->Var32(&SRAMWindow);

    file->VarArray(SRAMWriteBuffer, 0x800);
    file->Var32(&SRAMWritePos);
    file->Var32(&SRAMWriteLen);

    if (!file->Saving)
        BuildSRAMID();
}

void CartRetailNAND::SetSaveMemory(const u8* savedata, u32 savelen)
{
    CartRetail::SetSaveMemory(savedata, savelen);
    BuildSRAMID();
}

u32 CartRetailNAND::SRAMRead32()
{
    u32 addr = (SRAMAddr - SRAMBase) & (SRAMLength-1);
    u32 addrhi = addr & ~0xFFF;
    u32 addrlo = addr & 0xFFF;
    u32 ret;

    if (addrlo & 0x3)
    {
        ret = SRAM[addrhi | addrlo];
        addrlo = (addrlo + 1) & 0xFFF;
        ret |= (SRAM[addrhi | addrlo] << 8);
        addrlo = (addrlo + 1) & 0xFFF;
        ret |= (SRAM[addrhi | addrlo] << 16);
        addrlo = (addrlo + 1) & 0xFFF;
        ret |= (SRAM[addrhi | addrlo] << 24);
        addrlo = (addrlo + 1) & 0xFFF;
    }
    else
    {
        ret = *(u32*)&SRAM[addrhi | addrlo];
        addrlo = (addrlo + 4) & 0xFFF;
    }

    SRAMAddr = addrhi | addrlo;
    return ret;
}

void CartRetailNAND::ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd)
{
    if (CmdEncMode != 2) return CartRetail::ROMCommandStart(nds, cartslot, cmd);

    memcpy(ROMCmd, cmd, 8);

    switch (ROMCmd[0])
    {
    case 0x81: // write data
        if ((SRAMStatus & (1<<4)) && SRAMWindow >= SRAMBase && SRAMWindow < (SRAMBase+SRAMLength))
        {
            u32 addr = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];

            if (addr >= SRAMWindow && addr < (SRAMWindow+0x20000))
            {
                // the command is issued 4 times, each with the same address
                // seems they use the one from the first command (CHECKME)
                if (!SRAMAddr)
                    SRAMAddr = addr;
            }
        }
        else
            SRAMAddr = 0;
        return;

    case 0x82: // commit write
        if (SRAMAddr && SRAMWriteLen)
        {
            if (SRAMLength && SRAMAddr < (SRAMBase+SRAMLength-0x20000))
            {
                u32 offset = SRAMAddr - SRAMBase;
                if ((offset + 0x800) > SRAMLength)
                {
                    u32 len1 = SRAMLength - offset;
                    u32 len2 = 0x800 - len1;

                    memcpy(&SRAM[offset], SRAMWriteBuffer, len1);
                    memcpy(&SRAM[0], &SRAMWriteBuffer[len1], len2);

                    Platform::WriteNDSSave(SRAM.get(), SRAMLength, offset, len1, UserData);
                    Platform::WriteNDSSave(SRAM.get(), SRAMLength, 0, len2, UserData);
                }
                else
                {
                    memcpy(&SRAM[offset], SRAMWriteBuffer, 0x800);
                    Platform::WriteNDSSave(SRAM.get(), SRAMLength, offset, 0x800, UserData);
                }
            }

            SRAMAddr = 0;
            SRAMWritePos = 0;
            SRAMWriteLen = 0;
        }
        SRAMStatus &= ~(1<<4);
        return;

    case 0x84: // discard write buffer
        SRAMAddr = 0;
        SRAMWritePos = 0;
        return;

    case 0x85: // write enable
        if (SRAMWindow)
        {
            SRAMStatus |= (1<<4);
            SRAMWritePos = 0;
        }
        return;

    case 0x8B: // revert to ROM read mode
        SRAMWindow = 0;
        return;

    case 0x94: // return ID data
        ROMAddr = 0;
        return;

    case 0xB2: // set window for accessing SRAM
        {
            u32 addr = (ROMCmd[1]<<24) | ((ROMCmd[2]&0xFE)<<16);

            // window is 0x20000 bytes, address is aligned to that boundary
            // NAND remains stuck 'busy' forever if this is less than the starting SRAM address
            // TODO.
            if (addr < SRAMBase)
                Log(LogLevel::Warn, "NAND: !! BAD ADDR %08X < %08X\n", addr, SRAMBase);
            if (addr >= (SRAMBase+SRAMLength))
                Log(LogLevel::Warn, "NAND: !! BAD ADDR %08X > %08X\n", addr, SRAMBase+SRAMLength);

            SRAMWindow = addr;
        }
        return;

    case 0xB7:
        {
            u32 addr = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];

            if (SRAMWindow == 0)
            {
                // regular ROM mode
                return CartRetail::ROMCommandStart(nds, cartslot, cmd);
            }
            else
            {
                // SRAM mode
                if (SRAMWindow >= SRAMBase && SRAMWindow < (SRAMBase+SRAMLength) &&
                    addr >= SRAMWindow && addr < (SRAMWindow+0x20000))
                {
                    SRAMAddr = addr;
                }
                else
                    SRAMAddr = 0;
            }
        }
        return;

    default:
        return CartRetail::ROMCommandStart(nds, cartslot, cmd);
    }
}

u32 CartRetailNAND::ROMCommandReceive()
{
    if (CmdEncMode != 2) return CartRetail::ROMCommandReceive();

    switch (ROMCmd[0])
    {
    case 0x94: // return ID data
        {
            // TODO: check what the data really is. probably the NAND chip's ID.
            // also, might be different between different games or even between different carts.
            // this was taken from a Jam with the Band cart.

            if (ROMAddr >= 0x30)
                return 0;

            u32 ret = *(u32*)&SRAMID[ROMAddr];
            ROMAddr += 4;
            return ret;
        }

    case 0xB7:
        if (SRAMWindow == 0)
        {
            // regular ROM mode
            if (ROMAddr >= SRAMBase && ROMAddr < (SRAMBase+SRAMLength))
                return 0xFFFFFFFF;

            return CartRetail::ROMCommandReceive();
        }
        else
        {
            // SRAM mode
            if (!SRAMAddr)
                return 0xFFFFFFFF;

            return SRAMRead32();
        }

    case 0xD6: // read NAND status
        // status bits
        // bit5: ready
        // bit4: write enable
        return SRAMStatus * 0x01010101;

    default:
        return CartRetail::ROMCommandReceive();
    }
}

void CartRetailNAND::ROMCommandTransmit(u32 val)
{
    if (CmdEncMode != 2) return CartRetail::ROMCommandTransmit(val);

    switch (ROMCmd[0])
    {
    case 0x81: // write SRAM data
        if (SRAMAddr)
        {
            *(u32*)&SRAMWriteBuffer[SRAMWritePos] = val;

            // TODO verify what happens when writing more than 0x800 bytes (or less)
            SRAMWritePos += 4;
            SRAMWritePos &= 0x7FF;
            if (SRAMWriteLen < 0x800)
                SRAMWriteLen += 4;
        }
        return;

    default:
        return CartRetail::ROMCommandTransmit(val);
    }
}

void CartRetailNAND::BuildSRAMID()
{
    // the last 128K of the SRAM are read-only.
    // most of it is FF, except for the NAND ID at the beginning
    // of the last 0x800 bytes.

    if (SRAMLength > 0x20000)
    {
        memset(&SRAM[SRAMLength - 0x20000], 0xFF, 0x20000);

        // TODO: check what the data is all about!
        // this was pulled from a Jam with the Band cart. may be different on other carts.
        // WarioWare DIY may have different data or not have this at all.
        // the ID data is also found in the response to command 94, and JwtB checks it.
        // WarioWare doesn't seem to care.
        // there is also more data here, but JwtB doesn't seem to care.
        u8 iddata[0x10] = {0xEC, 0x00, 0x9E, 0xA1, 0x51, 0x65, 0x34, 0x35, 0x30, 0x35, 0x30, 0x31, 0x19, 0x19, 0x02, 0x0A};
        memcpy(&SRAM[SRAMLength - 0x800], iddata, 16);

        u8 iddata2[0x30] = {
                0xEC, 0xF1, 0x00, 0x95, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        memcpy(SRAMID, iddata2, 0x30);
        memcpy(&SRAMID[0x18], iddata, 16);
    }
}


CartRetailIR::CartRetailIR(const u8* rom, u32 len, u32 chipid, u32 irversion, bool badDSiDump, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata) :
    CartRetailIR(CopyToUnique(rom, len), len, chipid, irversion, badDSiDump, romparams, std::move(sram), sramlen, userdata)
{
}

CartRetailIR::CartRetailIR(
    std::unique_ptr<u8[]>&& rom,
    u32 len,
    u32 chipid,
    u32 irversion,
    bool badDSiDump,
    ROMListEntry romparams,
    std::unique_ptr<u8[]>&& sram,
    u32 sramlen,
    void* userdata
) :
    CartRetail(std::move(rom), len, chipid, badDSiDump, romparams, std::move(sram), sramlen, userdata, CartType::RetailIR),
    IRVersion(irversion)
{
}

CartRetailIR::~CartRetailIR() = default;

void CartRetailIR::Reset()
{
    CartRetail::Reset();

    IRCmd = 0;
}

void CartRetailIR::DoSavestate(Savestate* file)
{
    CartRetail::DoSavestate(file);

    file->Var8(&IRCmd);
}

void CartRetailIR::SPISelect()
{
    CartRetail::SPISelect();
    IRPos = 0;
}

u8 CartRetailIR::SPITransmitReceive(u8 val)
{
    if (IRPos == 0)
    {
        IRCmd = val;
        IRPos++;
        return 0;
    }

    // TODO: emulate actual IR comm

    u8 ret;
    switch (IRCmd)
    {
    case 0x00: // pass-through
        ret = CartRetail::SPITransmitReceive(val);
        break;
        //return CartRetail::SPIWrite(val, pos-1, last);

    case 0x08: // ID
        ret = 0xAA;
        //return 0xAA;
    }

    IRPos++;
    return ret;
}

CartRetailBT::CartRetailBT(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata) :
    CartRetailBT(CopyToUnique(rom, len), len, chipid, romparams, std::move(sram), sramlen, userdata)
{
}

CartRetailBT::CartRetailBT(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata) :
    CartRetail(std::move(rom), len, chipid, false, romparams, std::move(sram), sramlen, userdata, CartType::RetailBT)
{
    Log(LogLevel::Info,"POKETYPE CART\n");
}

CartRetailBT::~CartRetailBT() = default;

u8 CartRetailBT::SPITransmitReceive(u8 val)
{
    //Log(LogLevel::Debug,"POKETYPE SPI: %02X %d %d - %08X\n", val, pos, last, NDS::GetPC(0));

    /*if (pos == 0)
    {
        // TODO do something with it??
        if(val==0xFF)SetIRQ();
    }
    if(pos==7)SetIRQ();*/

    return 0;
}


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

void CartSD::ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset) const
{
    // TODO: how strict should this be for homebrew?

    addr &= (ROMLength-1);

    memcpy(data+offset, ROM.get()+addr, len);
}

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

void CartHomebrew::ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd)
{
    if (CmdEncMode != 2) return CartSD::ROMCommandStart(nds, cartslot, cmd);

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
        return CartSD::ROMCommandStart(nds, cartslot, cmd);
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

NDSCartSlot::NDSCartSlot(melonDS::NDS& nds, std::unique_ptr<CartCommon>&& rom) noexcept : NDS(nds)
{
    NDS.RegisterEventFuncs(Event_ROMTransfer, this,
    {
        MakeEventThunk(NDSCartSlot, ROMPrepareData),
        MakeEventThunk(NDSCartSlot, ROMEndTransfer)
    });
    NDS.RegisterEventFuncs(Event_ROMSPITransfer, this, {MakeEventThunk(NDSCartSlot, SPITransferDone)});
    // All fields are default-constructed because they're listed as such in the class declaration

    if (rom)
        SetCart(std::move(rom));
}

NDSCartSlot::~NDSCartSlot() noexcept
{
    NDS.UnregisterEventFuncs(Event_ROMTransfer);
    NDS.UnregisterEventFuncs(Event_ROMSPITransfer);

    // Cart is cleaned up automatically because it's a unique_ptr
}

void NDSCartSlot::Reset() noexcept
{
    ResetCart();
}

void NDSCartSlot::DoSavestate(Savestate* file) noexcept
{
    file->Section("NDSC");

    file->Var16(&SPICnt);
    file->Var32(&ROMCnt);

    file->Var8(&SPIData);
    //file->Var32(&SPIDataPos);
    //file->Bool32(&SPIHold);
    file->VarBool(&SPISelected);

    file->VarArray(ROMCommand.data(), sizeof(ROMCommand));
    file->Var32(&ROMData);

    //file->VarArray(TransferData.data(), sizeof(TransferData));
    file->Var32(&TransferPos);
    file->Var32(&TransferLen);
    file->Var32(&TransferDir);
    file->VarArray(TransferCmd.data(), sizeof(TransferCmd));

    // cart inserted/len/ROM/etc should be already populated
    // savestate should be loaded after the right game is loaded
    // (TODO: system to verify that indeed the right ROM is loaded)
    // (what to CRC? whole ROM? code binaries? latter would be more convenient for ie. romhaxing)

    u32 carttype = 0;
    u32 cartchk = 0;
    if (Cart)
    {
        carttype = Cart->Type();
        cartchk = Cart->Checksum();
    }

    if (file->Saving)
    {
        file->Var32(&carttype);
        file->Var32(&cartchk);
    }
    else
    {
        u32 savetype;
        file->Var32(&savetype);
        if (savetype != carttype) return;

        u32 savechk;
        file->Var32(&savechk);
        if (savechk != cartchk) return;
    }

    if (Cart) Cart->DoSavestate(file);
}


bool ReadROMParams(u32 gamecode, ROMListEntry* params)
{
    u32 offset = 0;
    u32 chk_size = ROMListEntryCount >> 1;
    for (;;)
    {
        u32 key = 0;
        const ROMListEntry* curentry = &ROMList[offset + chk_size];
        key = curentry->GameCode;

        if (key == gamecode)
        {
            memcpy(params, curentry, sizeof(ROMListEntry));
            return true;
        }
        else
        {
            if (key < gamecode)
            {
                if (chk_size == 0)
                    offset++;
                else
                    offset += chk_size;
            }
            else if (chk_size == 0)
            {
                return false;
            }

            chk_size >>= 1;
        }

        if (offset >= ROMListEntryCount)
        {
            return false;
        }
    }
}


void NDSCartSlot::DecryptSecureArea(u8* out) noexcept
{
    const NDSHeader& header = Cart->GetHeader();
    const u8* cartrom = Cart->GetROM();

    u32 gamecode = header.GameCodeAsU32();
    u32 arm9base = header.ARM9ROMOffset;

    memcpy(out, &cartrom[arm9base], 0x800);

    Key1_InitKeycode(false, gamecode, 2, 2);
    Key1_Decrypt((u32*)&out[0]);

    Key1_InitKeycode(false, gamecode, 3, 2);
    for (u32 i = 0; i < 0x800; i += 8)
        Key1_Decrypt((u32*)&out[i]);

    if (!strncmp((const char*)out, "encryObj", 8))
    {
        Log(LogLevel::Info, "Secure area decryption OK\n");
        *(u32*)&out[0] = 0xE7FFDEFF;
        *(u32*)&out[4] = 0xE7FFDEFF;
    }
    else
    {
        Log(LogLevel::Warn, "Secure area decryption failed\n");
        for (u32 i = 0; i < 0x800; i += 4)
            *(u32*)&out[i] = 0xE7FFDEFF;
    }
}

bool ValidateROM(u32 romlen, NDSHeader& header)
{
    // basic sanity checks to ensure we have a workable ROM file

    if (header.ARM9ROMOffset < 0x200)
        return false;
    if ((header.ARM9ROMOffset + header.ARM9Size) > romlen)
        return false;
    if (header.ARM7ROMOffset < 0x200)
        return false;
    if ((header.ARM7ROMOffset + header.ARM7Size) > romlen)
        return false;

    return true;
}

std::unique_ptr<CartCommon> ParseROM(const u8* romdata, u32 romlen, void* userdata, std::optional<NDSCartArgs>&& args)
{
    return ParseROM(CopyToUnique(romdata, romlen), romlen, userdata, std::move(args));
}

std::unique_ptr<CartCommon> ParseROM(std::unique_ptr<u8[]>&& romdata, u32 romlen, void* userdata, std::optional<NDSCartArgs>&& args)
{
    if (romdata == nullptr)
    {
        Log(LogLevel::Error, "NDSCart: romdata is null\n");
        return nullptr;
    }

    if (romlen < 0x1000)
    {
        Log(LogLevel::Error, "NDSCart: ROM is too small\n");
        return nullptr;
    }

    if (romlen > 512*1024*1024)
    {
        Log(LogLevel::Error, "NDSCart: ROM is too large\n");
        return nullptr;
    }

    auto [cartrom, cartromsize] = PadToPowerOf2(std::move(romdata), romlen);

    NDSHeader header {};
    memcpy(&header, cartrom.get(), sizeof(header));

    if (!ValidateROM(cartromsize, header))
    {
        Log(LogLevel::Error, "NDSCart: ROM header verification failed\n");
        return nullptr;
    }

    bool dsi = header.IsDSi();
    bool badDSiDump = false;

    if (dsi && header.DSiRegionMask == RegionMask::NoRegion)
    {
        Log(LogLevel::Info, "DS header indicates DSi, but region is zero. Going in bad dump mode.\n");
        badDSiDump = true;
        dsi = false;
    }

    const char *gametitle = header.GameTitle;
    u32 gamecode = header.GameCodeAsU32();

    bool homebrew = header.IsHomebrew();

    ROMListEntry romparams {};
    if (!ReadROMParams(gamecode, &romparams))
    {
        // set defaults
        Log(LogLevel::Warn, "ROM entry not found for gamecode %d\n", gamecode);

        romparams.GameCode = gamecode;
        romparams.ROMSize = cartromsize;
        if (homebrew)
            romparams.SaveMemType = 0; // no saveRAM for homebrew
        else
            romparams.SaveMemType = 2; // assume EEPROM 64k (TODO FIXME)
    }

    if (romparams.ROMSize != romlen)
        Log(LogLevel::Warn, "!! bad ROM size %d (expected %d) rounded to %d\n", romlen, romparams.ROMSize, cartromsize);

    // generate a ROM ID
    // note: most games don't check the actual value
    // it just has to stay the same throughout gameplay
    u32 cartid = 0x000000C2;

    if (cartromsize >= 1024 * 1024 && cartromsize <= 128 * 1024 * 1024)
        cartid |= ((cartromsize >> 20) - 1) << 8;
    else
        cartid |= (0x100 - (cartromsize >> 28)) << 8;

    if (romparams.SaveMemType >= 8 && romparams.SaveMemType <= 10)
        cartid |= 0x08000000; // NAND flag

    if (dsi)
        cartid |= 0x40000000;

    // cart ID for Jam with the Band
    // TODO: this kind of ID triggers different KEY1 phase
    // (repeats commands a bunch of times)
    //cartid = 0x88017FEC;
    //cartid = 0x80007FC2; // pokémon typing adventure

    u32 irversion = 0;
    if ((gamecode & 0xFF) == 'I')
    {
        if (((gamecode >> 8) & 0xFF) < 'P')
            irversion = 1; // Active Health / Walk with Me
        else
            irversion = 2; // Pokémon HG/SS, B/W, B2/W2
    }

    std::unique_ptr<CartCommon> cart;
    std::unique_ptr<u8[]> sram = args ? std::move(args->SRAM) : nullptr;
    u32 sramlen = args ? args->SRAMLength : 0;
    if (homebrew)
    {
        std::optional<FATStorage> sdcard = args && args->SDCard ? std::make_optional<FATStorage>(std::move(*args->SDCard)) : std::nullopt;
        cart = std::make_unique<CartHomebrew>(std::move(cartrom), cartromsize, cartid, romparams, userdata, std::move(sdcard));
    }
    else if (gametitle[0] == 0 && !strncmp("SD/TF-NDS", gametitle + 1, 9) && gamecode == 0x414D5341)
    {
        std::optional<FATStorage> sdcard = args && args->SDCard ? std::make_optional<FATStorage>(std::move(*args->SDCard)) : std::nullopt;
        cart = std::make_unique<CartR4>(std::move(cartrom), cartromsize, cartid, romparams, CartR4TypeR4, CartR4LanguageEnglish, userdata, std::move(sdcard));
    }
    else if (cartid & 0x08000000)
        cart = std::make_unique<CartRetailNAND>(std::move(cartrom), cartromsize, cartid, romparams, std::move(sram), sramlen, userdata);
    else if (irversion != 0)
        cart = std::make_unique<CartRetailIR>(std::move(cartrom), cartromsize, cartid, irversion, badDSiDump, romparams, std::move(sram), sramlen, userdata);
    else if ((gamecode & 0xFFFFFF) == 0x505A55) // UZPx
        cart = std::make_unique<CartRetailBT>(std::move(cartrom), cartromsize, cartid, romparams, std::move(sram), sramlen, userdata);
    else
        cart = std::make_unique<CartRetail>(std::move(cartrom), cartromsize, cartid, badDSiDump, romparams, std::move(sram), sramlen, userdata);

    args = std::nullopt;
    return cart;
}

void NDSCartSlot::SetCart(std::unique_ptr<CartCommon>&& cart) noexcept
{
    if (Cart)
        EjectCart();

    // Why a move function? Because the Cart object is polymorphic,
    // and cloning polymorphic objects without knowing the underlying type is annoying.
    Cart = std::move(cart);

    if (!Cart)
        // If we're ejecting an existing cart without inserting a new one...
        return;

    Cart->Reset();

    const NDSHeader& header = Cart->GetHeader();
    const ROMListEntry romparams = Cart->GetROMParams();
    const u8* cartrom = Cart->GetROM();
    if (header.ARM9ROMOffset >= 0x4000 && header.ARM9ROMOffset < 0x8000)
    {
        // reencrypt secure area if needed
        if (*(u32*)&cartrom[header.ARM9ROMOffset] == 0xE7FFDEFF && *(u32*)&cartrom[header.ARM9ROMOffset + 0x10] != 0xE7FFDEFF)
        {
            Log(LogLevel::Debug, "Re-encrypting cart secure area\n");

            strncpy((char*)&cartrom[header.ARM9ROMOffset], "encryObj", 8);

            Key1_InitKeycode(false, romparams.GameCode, 3, 2);
            for (u32 i = 0; i < 0x800; i += 8)
                Key1_Encrypt((u32*)&cartrom[header.ARM9ROMOffset + i]);

            Key1_InitKeycode(false, romparams.GameCode, 2, 2);
            Key1_Encrypt((u32*)&cartrom[header.ARM9ROMOffset]);

            Log(LogLevel::Debug, "Re-encrypted cart secure area\n");
        }
        else
        {
            Log(LogLevel::Debug, "No need to re-encrypt cart secure area\n");
        }
    }

    Log(LogLevel::Info, "Inserted cart with game code: %.4s\n", header.GameCode);
    Log(LogLevel::Info, "Inserted cart with ID: %08X\n", Cart->ID());
    Log(LogLevel::Info, "ROM entry: %08X %08X\n", romparams.ROMSize, romparams.SaveMemType);
}

void NDSCartSlot::SetSaveMemory(const u8* savedata, u32 savelen) noexcept
{
    if (Cart)
        Cart->SetSaveMemory(savedata, savelen);
}

void NDSCartSlot::SetupDirectBoot(const std::string& romname) noexcept
{
    if (Cart)
        Cart->SetupDirectBoot(romname, NDS);
}

std::unique_ptr<CartCommon> NDSCartSlot::EjectCart() noexcept
{
    if (!Cart) return nullptr;

    // ejecting the cart triggers the gamecard IRQ
    NDS.SetIRQ(0, IRQ_CartIREQMC);
    NDS.SetIRQ(1, IRQ_CartIREQMC);

    return std::move(Cart);

    // CHECKME: does an eject imply anything for the ROM/SPI transfer registers?
}

void NDSCartSlot::ResetCart() noexcept
{
    // CHECKME: what if there is a transfer in progress?

    SPICnt = 0;
    ROMCnt = 0;

    SPIData = 0;
    //SPIDataPos = 0;
    //SPIHold = false;
    SPISelected = false;

    memset(ROMCommand.data(), 0, sizeof(ROMCommand));
    ROMData = 0;

    Key2_X = 0;
    Key2_Y = 0;

    //memset(TransferData.data(), 0, sizeof(TransferData));
    TransferPos = 0;
    TransferLen = 0;
    TransferDir = 0;
    memset(TransferCmd.data(), 0, sizeof(TransferCmd));
    TransferCmd[0] = 0xFF;

    if (Cart) Cart->Reset();
}


void NDSCartSlot::ROMEndTransfer(u32 param) noexcept
{
    ROMCnt &= ~(1<<31);

    if (SPICnt & (1<<14))
        NDS.SetIRQ((NDS.ExMemCnt[0]>>11)&0x1, IRQ_CartXferDone);

    if (Cart)
        Cart->ROMCommandFinish();
}

void NDSCartSlot::ROMPrepareData(u32 param) noexcept
{
    if (TransferDir == 0)
    {
        if (TransferPos >= TransferLen)
            ROMData = 0;
        else
            if (Cart) ROMData = Cart->ROMCommandReceive();
            //ROMData = *(u32*)&TransferData[TransferPos];

        TransferPos += 4;
    }

    ROMCnt |= (1<<23);

    if (NDS.ExMemCnt[0] & (1<<11))
        NDS.CheckDMAs(1, 0x12);
    else
        NDS.CheckDMAs(0, 0x05);
}

void NDSCartSlot::WriteROMCnt(u32 val) noexcept
{
    u32 xferstart = (val & ~ROMCnt) & (1<<31);
    ROMCnt = (val & 0xFF7F7FFF) | (ROMCnt & 0x20800000);

    // all this junk would only really be useful if melonDS was interfaced to
    // a DS cart reader
    if (val & (1<<15))
    {
        u32 snum = (NDS.ExMemCnt[0]>>8)&0x8;
        u64 seed0 = *(u32*)&NDS.ROMSeed0[snum] | ((u64)NDS.ROMSeed0[snum+4] << 32);
        u64 seed1 = *(u32*)&NDS.ROMSeed1[snum] | ((u64)NDS.ROMSeed1[snum+4] << 32);

        Key2_X = 0;
        Key2_Y = 0;
        for (u32 i = 0; i < 39; i++)
        {
            if (seed0 & (1ULL << i)) Key2_X |= (1ULL << (38-i));
            if (seed1 & (1ULL << i)) Key2_Y |= (1ULL << (38-i));
        }

        Log(LogLevel::Debug, "seed0: %02X%08X\n", (u32)(seed0>>32), (u32)seed0);
        Log(LogLevel::Debug, "seed1: %02X%08X\n", (u32)(seed1>>32), (u32)seed1);
        Log(LogLevel::Debug, "key2 X: %02X%08X\n", (u32)(Key2_X>>32), (u32)Key2_X);
        Log(LogLevel::Debug, "key2 Y: %02X%08X\n", (u32)(Key2_Y>>32), (u32)Key2_Y);
    }

    // transfers will only start when bit31 changes from 0 to 1
    // and if AUXSPICNT is configured correctly
    if (!(SPICnt & (1<<15))) return;
    if (SPICnt & (1<<13)) return;
    if (!xferstart) return;

    u32 datasize = (ROMCnt >> 24) & 0x7;
    if (datasize == 7)
        datasize = 4;
    else if (datasize > 0)
        datasize = 0x100 << datasize;

    TransferPos = 0;
    TransferLen = datasize;

    *(u32*)&TransferCmd[0] = *(u32*)&ROMCommand[0];
    *(u32*)&TransferCmd[4] = *(u32*)&ROMCommand[4];

    //memset(TransferData.data(), 0xFF, TransferLen);

    /*printf("ROM COMMAND %04X %08X %02X%02X%02X%02X%02X%02X%02X%02X SIZE %04X\n",
           SPICnt, ROMCnt,
           TransferCmd[0], TransferCmd[1], TransferCmd[2], TransferCmd[3],
           TransferCmd[4], TransferCmd[5], TransferCmd[6], TransferCmd[7],
           datasize);*/

    // default is read
    // commands that do writes will change this
    //TransferDir = 0;
    TransferDir = (ROMCnt >> 30) & 0x1;

    if (Cart)
        Cart->ROMCommandStart(NDS, *this, TransferCmd.data());

    //if ((datasize > 0) && (((ROMCnt >> 30) & 0x1) != TransferDir))
    //    Log(LogLevel::Debug, "NDSCART: !! BAD TRANSFER DIRECTION FOR CMD %02X, DIR=%d, ROMCNT=%08X\n", ROMCommand[0], TransferDir, ROMCnt);

    ROMCnt &= ~(1<<23);

    // ROM transfer timings
    // the bus is parallel with 8 bits
    // thus a command would take 8 cycles to be transferred
    // and it would take 4 cycles to receive a word of data
    // TODO: advance read position if bit28 is set
    // TODO: during a write transfer, bit23 is set immediately when beginning the transfer(?)

    u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
    u32 cmddelay = 8;

    // delays are only applied when the WR bit is cleared
    // CHECKME: do the delays apply at the end (instead of start) when WR is set?
    if (!(ROMCnt & (1<<30)))
    {
        cmddelay += (ROMCnt & 0x1FFF);
        if (datasize) cmddelay += ((ROMCnt >> 16) & 0x3F);
    }

    if (datasize == 0)
        NDS.ScheduleEvent(Event_ROMTransfer, false, xfercycle*cmddelay, ROMTransfer_End, 0);
    else
        NDS.ScheduleEvent(Event_ROMTransfer, false, xfercycle*(cmddelay+4), ROMTransfer_PrepareData, 0);
}

void NDSCartSlot::AdvanceROMTransfer() noexcept
{
    ROMCnt &= ~(1<<23);

    if (TransferPos < TransferLen)
    {
        u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
        u32 delay = 4;
        if (!(ROMCnt & (1<<30)))
        {
            if (!(TransferPos & 0x1FF))
                delay += ((ROMCnt >> 16) & 0x3F);
        }

        NDS.ScheduleEvent(Event_ROMTransfer, false, xfercycle*delay, ROMTransfer_PrepareData, 0);
    }
    else
        ROMEndTransfer(0);
}

u32 NDSCartSlot::ReadROMData() noexcept
{
    if (ROMCnt & (1<<30)) return 0;

    if (ROMCnt & (1<<23))
    {
        AdvanceROMTransfer();
    }

    return ROMData;
}

void NDSCartSlot::WriteROMData(u32 val) noexcept
{
    if (!(ROMCnt & (1<<30))) return;

    ROMData = val;

    if (ROMCnt & (1<<23))
    {
        if (TransferDir == 1)
        {
            if (TransferPos < TransferLen)
            {
                if (Cart) Cart->ROMCommandTransmit(ROMData);
            }
                //*(u32*)&TransferData[TransferPos] = ROMData;

            TransferPos += 4;
        }

        AdvanceROMTransfer();
    }
}


void NDSCartSlot::WriteSPICnt(u16 val) noexcept
{
    if ((SPICnt & 0x2040) == 0x2040 && (val & 0x2000) == 0x0000)
    {
        // forcefully reset SPI hold
        //SPIHold = false;
        // CHECKME
        if (Cart && SPISelected)
            Cart->SPIRelease();
        SPISelected = false;
    }

    SPICnt = (SPICnt & 0x0080) | (val & 0xE043);

    // AUXSPICNT can be changed during a transfer
    // in this case, the transfer continues until the end, even if bit13 or bit15 are cleared
    // if the transfer speed is changed, the transfer continues at the new speed (TODO)
    if (SPICnt & (1<<7))
        Log(LogLevel::Debug, "!! CHANGING AUXSPICNT DURING TRANSFER: %04X\n", val);
}

void NDSCartSlot::SPITransferDone(u32 param) noexcept
{
    SPICnt &= ~(1<<7);
}

u8 NDSCartSlot::ReadSPIData() const noexcept
{
    if (!(SPICnt & (1<<15))) return 0;
    if (!(SPICnt & (1<<13))) return 0;
    if (SPICnt & (1<<7)) return 0; // checkme

    return SPIData;
}

void NDSCartSlot::WriteSPIData(u8 val) noexcept
{
    if (!(SPICnt & (1<<15))) return;
    if (!(SPICnt & (1<<13))) return;
    if (SPICnt & (1<<7)) return;

    SPICnt |= (1<<7);

    bool hold = SPICnt&(1<<6);
    /*bool islast = false;
    if (!hold)
    {
        if (SPIHold) SPIDataPos++;
        else         SPIDataPos = 0;
        islast = true;
        SPIHold = false;
    }
    else if (hold && (!SPIHold))
    {
        SPIHold = true;
        SPIDataPos = 0;
    }
    else
    {
        SPIDataPos++;
    }

    if (Cart) SPIData = Cart->SPIWrite(val, SPIDataPos, islast);
    else      SPIData = 0;*/

    if (Cart)
    {
        if (!SPISelected) Cart->SPISelect();
        SPIData = Cart->SPITransmitReceive(val);
        if (!hold) Cart->SPIRelease();
    }
    else
        SPIData = 0;

    SPISelected = hold;

    // SPI transfers one bit per cycle -> 8 cycles per byte
    u32 delay = 8 * (8 << (SPICnt & 0x3));
    NDS.ScheduleEvent(Event_ROMSPITransfer, false, delay, 0, 0);
}

}

}