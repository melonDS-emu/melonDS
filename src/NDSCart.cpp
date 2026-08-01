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
#include <inttypes.h>
#include <assert.h>
#include "NDS.h"
#include "DSi.h"
#include "NDSCart.h"
#include "CRC32.h"
#include "Platform.h"
#include "ROMList.h"
#include "FATStorage.h"
#include "Utils.h"

#ifdef RETROACHIEVEMENTS_ENABLED
#include "RetroAchievements/RAClient.h"
#include <rc_hash.h>
#endif

#include "NDSCart/CartCommon.h"
#include "NDSCart/CartRetail.h"
#include "NDSCart/CartRetailNAND.h"
#include "NDSCart/CartRetailIR.h"
#include "NDSCart/CartRetailBT.h"
#include "NDSCart/CartHomebrew.h"
#include "NDSCart/CartR4.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace NDSCart
{

enum
{
    ROMTransfer_ReceiveData = 0,
    ROMTransfer_SendData,
    ROMTransfer_End
};


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


NDSCartSlot::NDSCartSlot(melonDS::NDS& nds, u32 num, std::unique_ptr<CartCommon>&& rom) noexcept
: NDS(nds), Num(num)
{
<<<<<<< HEAD
}

CartCommon::CartCommon(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, melonDS::NDSCart::CartType type, void* userdata) :
    ROM(std::move(rom)),
    ROMLength(len),
    ChipID(chipid),
    ROMParams(romparams),
    CartType(type),
    UserData(userdata)
{
    #ifdef RETROACHIEVEMENTS_ENABLED
        if (ROM && ROMLength > 0)
        {
            const bool ok = rc_hash_generate_from_buffer(
                this->ra_hash,
                RC_CONSOLE_NINTENDO_DS,
                ROM.get(),
                ROMLength
            );

            if (ok)
            {
                if (ra)
                {
                    ra->SetPendingGameHash(this->ra_hash);
                }
            }
        }
    #endif
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
    file->Bool32(&DSiMode);
}

int CartCommon::ROMCommandStart(NDS& nds, NDSCartSlot& cartslot, const u8* cmd, u8* data, u32 len)
{
    if (CmdEncMode == 0)
    {
        switch (cmd[0])
        {
        case 0x9F:
            memset(data, 0xFF, len);
            return 0;

        case 0x00:
            memset(data, 0, len);
            if (len > 0x1000)
            {
                ReadROM(0, 0x1000, data, 0);
                for (u32 pos = 0x1000; pos < len; pos += 0x1000)
                    memcpy(data+pos, data, 0x1000);
            }
            else
                ReadROM(0, len, data, 0);
            return 0;

        case 0x90:
            for (u32 pos = 0; pos < len; pos += 4)
                *(u32*)&data[pos] = ChipID;
            return 0;

        case 0x3C:
            CmdEncMode = 1;
            cartslot.Key1_InitKeycode(false, *(u32*)&ROM[0xC], 2, 2);
            DSiMode = false;
            return 0;

        case 0x3D:
            if (IsDSi)
            {
                CmdEncMode = 1;
                cartslot.Key1_InitKeycode(true, *(u32*)&ROM[0xC], 1, 2);
                DSiMode = true;
            }
            return 0;

        default:
            return 0;
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

        // TODO eventually: verify all the command parameters and shit

        switch (cmddec[0] & 0xF0)
        {
        case 0x40:
            DataEncMode = 2;
            return 0;

        case 0x10:
            for (u32 pos = 0; pos < len; pos += 4)
                *(u32*)&data[pos] = ChipID;
            return 0;

        case 0x20:
            {
                u32 addr = (cmddec[2] & 0xF0) << 8;
                if (DSiMode)
                {
                    // the DSi region starts with 0x3000 unreadable bytes
                    // similarly to how the DS region starts at 0x1000 with 0x3000 unreadable bytes
                    // these contain data for KEY1 crypto
                    addr -= 0x1000;
                    addr += DSiBase;
                }
                ReadROM(addr, 0x1000, data, 0);
            }
            return 0;

        case 0xA0:
            CmdEncMode = 2;
            return 0;

        default:
            return 0;
        }
    }
    else if (CmdEncMode == 2)
    {
        switch (cmd[0])
        {
        case 0xB8:
            for (u32 pos = 0; pos < len; pos += 4)
                *(u32*)&data[pos] = ChipID;
            return 0;

        default:
            return 0;
        }
    }

    return 0;
}

void CartCommon::ROMCommandFinish(const u8* cmd, u8* data, u32 len)
{
}

u8 CartCommon::SPIWrite(u8 val, u32 pos, bool last)
{
    return 0xFF;
}

void CartCommon::ReadROM(u32 addr, u32 len, u8* data, u32 offset) const
{
    if (addr >= ROMLength) return;
    if ((addr+len) > ROMLength)
        len = ROMLength - addr;

    memcpy(data+offset, ROM.get()+addr, len);
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

CartRetail::CartRetail(const u8* rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata, melonDS::NDSCart::CartType type) :
    CartRetail(CopyToUnique(rom, len), len, chipid, badDSiDump, romparams, std::move(sram), sramlen, userdata, type)
{
}

CartRetail::CartRetail(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata, melonDS::NDSCart::CartType type) :
    CartCommon(std::move(rom), len, chipid, badDSiDump, romparams, type, userdata)
{
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

    SRAMCmd = 0;
    SRAMAddr = 0;
    SRAMStatus = 0;
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

    file->Var8(&SRAMCmd);
    file->Var32(&SRAMAddr);
    file->Var8(&SRAMStatus);

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

int CartRetail::ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd, u8* data, u32 len)
{
    if (CmdEncMode != 2) return CartCommon::ROMCommandStart(nds, cartslot, cmd, data, len);

    switch (cmd[0])
    {
    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            memset(data, 0, len);

            if (((addr + len - 1) >> 12) != (addr >> 12))
            {
                u32 len1 = 0x1000 - (addr & 0xFFF);
                ReadROM_B7(addr, len1, data, 0);
                ReadROM_B7(addr+len1, len-len1, data, len1);
            }
            else
                ReadROM_B7(addr, len, data, 0);
        }
        return 0;

    default:
        return CartCommon::ROMCommandStart(nds, cartslot, cmd, data, len);
    }
}

u8 CartRetail::SPIWrite(u8 val, u32 pos, bool last)
{
    if (SRAMType == 0) return 0;

    if (pos == 0)
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
        }

        return 0xFF;
    }

    switch (SRAMType)
    {
    case 1: return SRAMWrite_EEPROMTiny(val, pos, last);
    case 2: return SRAMWrite_EEPROM(val, pos, last);
    case 3: return SRAMWrite_FLASH(val, pos, last);
    default: return 0xFF;
    }
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

u8 CartRetail::SRAMWrite_EEPROMTiny(u8 val, u32 pos, bool last)
{
    switch (SRAMCmd)
    {
    case 0x01: // write status register
        // TODO: WP bits should be nonvolatile!
        if (pos == 1)
            SRAMStatus = (SRAMStatus & 0x01) | (val & 0x0C);
        return 0;

    case 0x05: // read status register
        return SRAMStatus | 0xF0;

    case 0x02: // write low
    case 0x0A: // write high
        if (pos < 2)
        {
            SRAMAddr = val;
            SRAMFirstAddr = SRAMAddr;
        }
        else
        {
            // TODO: implement WP bits!
            if (SRAMStatus & (1<<1))
            {
                SRAM[(SRAMAddr + ((SRAMCmd==0x0A)?0x100:0)) & 0x1FF] = val;
            }
            SRAMAddr++;
        }
        if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   (SRAMFirstAddr + ((SRAMCmd==0x0A)?0x100:0)) & 0x1FF, SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }
        return 0;

    case 0x03: // read low
    case 0x0B: // read high
        if (pos < 2)
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
        if (pos == 1)
            Log(LogLevel::Warn, "unknown tiny EEPROM save command %02X\n", SRAMCmd);
        return 0xFF;
    }
}

u8 CartRetail::SRAMWrite_EEPROM(u8 val, u32 pos, bool last)
{
    u32 addrsize = 2;
    if (SRAMLength > 65536) addrsize++;

    switch (SRAMCmd)
    {
    case 0x01: // write status register
        // TODO: WP bits should be nonvolatile!
        if (pos == 1)
            SRAMStatus = (SRAMStatus & 0x01) | (val & 0x0C);
        return 0;

    case 0x05: // read status register
        return SRAMStatus;

    case 0x02: // write
        if (pos <= addrsize)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            SRAMFirstAddr = SRAMAddr;
        }
        else
        {
            // TODO: implement WP bits
            if (SRAMStatus & (1<<1))
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = val;
            }
            SRAMAddr++;
        }
        if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   SRAMFirstAddr & (SRAMLength-1), SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }
        return 0;

    case 0x03: // read
        if (pos <= addrsize)
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
        if (pos == 1)
            Log(LogLevel::Warn, "unknown EEPROM save command %02X\n", SRAMCmd);
        return 0xFF;
    }
}

u8 CartRetail::SRAMWrite_FLASH(u8 val, u32 pos, bool last)
{
    switch (SRAMCmd)
    {
    case 0x05: // read status register
        return SRAMStatus;

    case 0x02: // page program
        if (pos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            SRAMFirstAddr = SRAMAddr;
        }
        else
        {
            if (SRAMStatus & (1<<1))
            {
                // CHECKME: should it be &=~val ??
                SRAM[SRAMAddr & (SRAMLength-1)] = 0;
            }
            SRAMAddr++;
        }
        if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   SRAMFirstAddr & (SRAMLength-1), SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }
        return 0;

    case 0x03: // read
        if (pos <= 3)
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
        if (pos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            SRAMFirstAddr = SRAMAddr;
        }
        else
        {
            if (SRAMStatus & (1<<1))
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = val;
            }
            SRAMAddr++;
        }
        if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   SRAMFirstAddr & (SRAMLength-1), SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }
        return 0;

    case 0x0B: // fast read
        if (pos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            return 0;
        }
        else if (pos == 4)
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
        if (pos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            SRAMFirstAddr = SRAMAddr;
        }
        if ((pos == 3) && (SRAMStatus & (1<<1)))
        {
            for (u32 i = 0; i < 0x10000; i++)
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = 0;
                SRAMAddr++;
            }
        }
        if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   SRAMFirstAddr & (SRAMLength-1), SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }
        return 0;

    case 0xDB: // page erase
        if (pos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            SRAMFirstAddr = SRAMAddr;
        }
        if ((pos == 3) && (SRAMStatus & (1<<1)))
        {
            for (u32 i = 0; i < 0x100; i++)
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = 0;
                SRAMAddr++;
            }
        }
        if (last)
        {
            SRAMStatus &= ~(1<<1);
            Platform::WriteNDSSave(SRAM.get(), SRAMLength,
                                   SRAMFirstAddr & (SRAMLength-1), SRAMAddr-SRAMFirstAddr,
                                   UserData);
        }
        return 0;

    default:
        if (pos == 1)
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

    if (!file->Saving)
        BuildSRAMID();
}

void CartRetailNAND::SetSaveMemory(const u8* savedata, u32 savelen)
{
    CartRetail::SetSaveMemory(savedata, savelen);
    BuildSRAMID();
}

int CartRetailNAND::ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd, u8* data, u32 len)
{
    if (CmdEncMode != 2) return CartCommon::ROMCommandStart(nds, cartslot, cmd, data, len);

    switch (cmd[0])
    {
    case 0x81: // write data
        if ((SRAMStatus & (1<<4)) && SRAMWindow >= SRAMBase && SRAMWindow < (SRAMBase+SRAMLength))
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];

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
        return 1;

    case 0x82: // commit write
        if (SRAMAddr && SRAMWritePos)
        {
            if (SRAMLength && SRAMAddr < (SRAMBase+SRAMLength-0x20000))
            {
                memcpy(&SRAM[SRAMAddr - SRAMBase], SRAMWriteBuffer, 0x800);
                Platform::WriteNDSSave(SRAM.get(), SRAMLength, SRAMAddr - SRAMBase, 0x800, UserData);
            }

            SRAMAddr = 0;
            SRAMWritePos = 0;
        }
        SRAMStatus &= ~(1<<4);
        return 0;

    case 0x84: // discard write buffer
        SRAMAddr = 0;
        SRAMWritePos = 0;
        return 0;

    case 0x85: // write enable
        if (SRAMWindow)
        {
            SRAMStatus |= (1<<4);
            SRAMWritePos = 0;
        }
        return 0;

    case 0x8B: // revert to ROM read mode
        SRAMWindow = 0;
        return 0;

    case 0x94: // return ID data
        {
            // TODO: check what the data really is. probably the NAND chip's ID.
            // also, might be different between different games or even between different carts.
            // this was taken from a Jam with the Band cart.
            u8 iddata[0x30] =
            {
                0xEC, 0xF1, 0x00, 0x95, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            };

            if (SRAMLength) memcpy(&iddata[0x18], &SRAM[SRAMLength - 0x800], 16);

            memset(data, 0, len);
            memcpy(data, iddata, std::min(len, 0x30u));
        }
        return 0;

    case 0xB2: // set window for accessing SRAM
        {
            u32 addr = (cmd[1]<<24) | ((cmd[2]&0xFE)<<16);

            // window is 0x20000 bytes, address is aligned to that boundary
            // NAND remains stuck 'busy' forever if this is less than the starting SRAM address
            // TODO.
            if (addr < SRAMBase) Log(LogLevel::Warn,"NAND: !! BAD ADDR %08X < %08X\n", addr, SRAMBase);
            if (addr >= (SRAMBase+SRAMLength)) Log(LogLevel::Warn,"NAND: !! BAD ADDR %08X > %08X\n", addr, SRAMBase+SRAMLength);

            SRAMWindow = addr;
        }
        return 0;

    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];

            if (SRAMWindow == 0)
            {
                // regular ROM mode
                memset(data, 0, len);

                if (((addr + len - 1) >> 12) != (addr >> 12))
                {
                    u32 len1 = 0x1000 - (addr & 0xFFF);
                    ReadROM_B7(addr, len1, data, 0);
                    ReadROM_B7(addr+len1, len-len1, data, len1);
                }
                else
                    ReadROM_B7(addr, len, data, 0);
            }
            else
            {
                // SRAM mode
                memset(data, 0xFF, len);

                if (SRAMWindow >= SRAMBase && SRAMWindow < (SRAMBase+SRAMLength) &&
                    addr >= SRAMWindow && addr < (SRAMWindow+0x20000))
                {
                    memcpy(data, &SRAM[addr - SRAMBase], len);
                }
            }
        }
        return 0;

    case 0xD6: // read NAND status
        {
            // status bits
            // bit5: ready
            // bit4: write enable

            for (u32 i = 0; i < len; i+=4)
                *(u32*)&data[i] = SRAMStatus * 0x01010101;
        }
        return 0;

    default:
        return CartRetail::ROMCommandStart(nds, cartslot, cmd, data, len);
    }
}

void CartRetailNAND::ROMCommandFinish(const u8* cmd, u8* data, u32 len)
{
    if (CmdEncMode != 2) return CartCommon::ROMCommandFinish(cmd, data, len);

    switch (cmd[0])
    {
    case 0x81: // write data
        if (SRAMAddr)
        {
            if ((SRAMWritePos + len) > 0x800)
                len = 0x800 - SRAMWritePos;

            memcpy(&SRAMWriteBuffer[SRAMWritePos], data, len);
            SRAMWritePos += len;
        }
        return;

    default:
        return CartCommon::ROMCommandFinish(cmd, data, len);
    }
}

u8 CartRetailNAND::SPIWrite(u8 val, u32 pos, bool last)
{
    return 0xFF;
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

u8 CartRetailIR::SPIWrite(u8 val, u32 pos, bool last)
{
    if (pos == 0)
    {
        IRCmd = val;
        return 0;
    }

    // TODO: emulate actual IR comm

    switch (IRCmd)
    {
    case 0x00: // pass-through
        return CartRetail::SPIWrite(val, pos-1, last);

    case 0x08: // ID
        return 0xAA;
    }

    return 0;
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

u8 CartRetailBT::SPIWrite(u8 val, u32 pos, bool last)
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

int CartHomebrew::ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd, u8* data, u32 len)
{
    if (CmdEncMode != 2) return CartCommon::ROMCommandStart(nds, cartslot, cmd, data, len);

    switch (cmd[0])
    {
    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            memset(data, 0, len);

            if (((addr + len - 1) >> 12) != (addr >> 12))
            {
                u32 len1 = 0x1000 - (addr & 0xFFF);
                ReadROM_B7(addr, len1, data, 0);
                ReadROM_B7(addr+len1, len-len1, data, len1);
            }
            else
                ReadROM_B7(addr, len, data, 0);
        }
        return 0;

    case 0xC0: // SD read
        {
            u32 sector = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            if (SD) SD->ReadSectors(sector, len>>9, data);
        }
        return 0;

    case 0xC1: // SD write
        return 1;

    default:
        return CartCommon::ROMCommandStart(nds, cartslot, cmd, data, len);
    }
}

void CartHomebrew::ROMCommandFinish(const u8* cmd, u8* data, u32 len)
{
    if (CmdEncMode != 2) return CartCommon::ROMCommandFinish(cmd, data, len);

    // TODO: delayed SD writing? like we have for SRAM

    switch (cmd[0])
    {
    case 0xC1:
        {
            u32 sector = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            if (SD && !SD->IsReadOnly()) SD->WriteSectors(sector, len>>9, data);
        }
        break;

    default:
        return CartCommon::ROMCommandFinish(cmd, data, len);
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
=======
    SetLogicalNum(Num);
>>>>>>> upstream/master

    if (rom)
        SetCart(std::move(rom));
}

NDSCartSlot::~NDSCartSlot() noexcept
{
    // Cart is cleaned up automatically because it's a unique_ptr
}

void NDSCartSlot::Reset() noexcept
{
    SetLogicalNum(Num);

    // on DS, the cart interface is always powered on
    // on DSi, start powered off - SCFG_MC is used to power the interface up/down
    if (NDS.ConsoleType == 1)
        PowerState = 0;
    else
        PowerState = 2;

    for (auto& inter : Interfaces)
        inter.Reset();

    Key2_X = 0;
    Key2_Y = 0;

    if (Cart)
        Cart->Reset();
}

void NDSCartSlot::DoSavestate(Savestate* file) noexcept
{
    file->Section((Num==0) ? "NDSC" : "NC2i");

    file->Var8(&LogicalNum);

    file->Var64(&Key2_X);
    file->Var64(&Key2_Y);

    file->Var8(&CPUSelect);
    file->Var8(&PowerState);
    file->VarBool(&CartActive);

    for (auto& inter : Interfaces)
        inter.DoSavestate(file);

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

    if (Cart)
        Cart->DoSavestate(file);

    if (!file->Saving)
    {
        SetLogicalNum(LogicalNum);

        if (!Cart)
            CartActive = false;
    }
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
    {
        // If we're ejecting an existing cart without inserting a new one...
        CartActive = false;
        return;
    }

    CartActive = true;
    Cart->Reset();

    UpdateCartState();

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
    PowerState = 2;

    // TODO: determine actual values
    Interfaces[0].ROMCnt = (1<<29);
    Interfaces[1].ROMCnt = (1<<29);

    UpdateCartState();

    if (Cart)
        Cart->SetupDirectBoot(romname, NDS);
}

std::unique_ptr<CartCommon> NDSCartSlot::EjectCart() noexcept
{
    if (!Cart) return nullptr;

    // ejecting the cart triggers the gamecard IRQ
    RaiseCardIRQ();

    CartActive = false;
    auto oldcart = std::move(Cart);
    Cart = nullptr;

    UpdateCartState();

    return oldcart;
}

void NDSCartSlot::SetCPUSelect(u32 sel)
{
    // TODO: what happens if this is changed during a transfer?
    CPUSelect = sel;

    if ((Interfaces[0].ROMCnt ^ Interfaces[1].ROMCnt) & (1<<29))
        UpdateCartState();
}

void NDSCartSlot::SetPowerState(u8 power)
{
    assert(NDS.ConsoleType == 1);

    if (power == PowerState)
        return;
    PowerState = power;

    if (PowerState == 0)
    {
        // state 0 clears the "reset release" bit
        Interfaces[0].ROMCnt &= ~(1<<29);
        Interfaces[1].ROMCnt &= ~(1<<29);

        UpdateCartState();
    }

    // clock output is only active in power state 2
    CartActive = (Cart != nullptr) && (PowerState == 2);
}

void NDSCartSlot::SetLogicalNum(u8 num)
{
    LogicalNum = num;
    if (LogicalNum == 0)
    {
        TransferIRQ = IRQ_CartXferDone;
        CardIRQ = IRQ_CartIREQMC;
    }
    else
    {
        assert(NDS.ConsoleType == 1);
        TransferIRQ = IRQ_DSi_Cart2XferDone;
        CardIRQ = IRQ_DSi_Cart2IREQMC;
    }
}

void NDSCartSlot::RaiseCardIRQ()
{
    NDS.SetIRQ(0, CardIRQ);
    NDS.SetIRQ(1, CardIRQ);
}

void NDSCartSlot::UpdateCartState()
{
    if (!Cart)
        return;

    CartActive = (PowerState == 2);

    // /RES is held low if:
    // * ROMCTRL bit 29 is zero
    // * on DSi: power state is not 2
    bool reset = false;
    if (!CartActive || !(Interfaces[CPUSelect].ROMCnt & (1<<29)))
        reset = true;

    Cart->SetResetState(reset);
}


NDSCartSlot::Interface::Interface(NDSCartSlot& parent, u8 num)
: Parent(parent), Num(num)
{
    if (Parent.Num == 0)
    {
        // first cart slot
        ROMTransferEvent = (Num==0) ? Event_CartROMTransfer9 : Event_CartROMTransfer7;
        SPITransferEvent = (Num==0) ? Event_CartSPITransfer9 : Event_CartSPITransfer7;
    }
    else
    {
        // second cart slot, for DSi
        ROMTransferEvent = (Num==0) ? Event_DSi_Cart2ROMTransfer9 : Event_DSi_Cart2ROMTransfer7;
        SPITransferEvent = (Num==0) ? Event_DSi_Cart2SPITransfer9 : Event_DSi_Cart2SPITransfer7;
    }

    // due to how the event scheduler works, we need specific event IDs for each interface, which isn't ideal
    Parent.NDS.RegisterEventFuncs(ROMTransferEvent, this, {
        MakeEventThunk(Interface, ROMReceiveData),
        MakeEventThunk(Interface, ROMSendData),
        MakeEventThunk(Interface, ROMEndTransfer)
    });
    Parent.NDS.RegisterEventFuncs(SPITransferEvent, this,
                                  {MakeEventThunk(Interface, SPITransferDone)});
}

NDSCartSlot::Interface::~Interface()
{
    Parent.NDS.UnregisterEventFuncs(ROMTransferEvent);
    Parent.NDS.UnregisterEventFuncs(SPITransferEvent);
}

void NDSCartSlot::Interface::Reset()
{
    SPICnt = 0;
    SPIData = 0;

    ROMCnt = 0;
    memset(ROMCommand, 0, sizeof(ROMCommand));

    // TODO checkme
    Key2_Seed0 = 0;
    Key2_Seed1 = 0;

    ROMTransferPos = 0;
    ROMTransferLen = 0;

    memset(ROMData, 0, sizeof(ROMData));
    ROMDataPosCPU = 0;
    ROMDataPosCart = 0;
    ROMDataCount = 0;
    ROMDataLate = false;

    SPISelected = false;
}

void NDSCartSlot::Interface::DoSavestate(Savestate* file)
{
    file->Var16(&SPICnt);
    file->Var8(&SPIData);

    file->Var32(&ROMCnt);
    file->VarArray(ROMCommand, sizeof(ROMCommand));

    file->Var64(&Key2_Seed0);
    file->Var64(&Key2_Seed1);

    file->Var32(&ROMTransferPos);
    file->Var32(&ROMTransferLen);

    file->VarArray(ROMData, sizeof(ROMData));
    file->Var32(&ROMDataPosCPU);
    file->Var32(&ROMDataPosCart);
    file->Var32(&ROMDataCount);
    file->VarBool(&ROMDataLate);

    file->VarBool(&SPISelected);
}


void NDSCartSlot::Interface::WriteROMCnt(u32 val, u32 mask)
{
    val &= mask;
    u32 resetrel = (val & ~ROMCnt) & (1<<29);
    u32 xferstart = (val & ~ROMCnt) & (1<<31);
    ROMCnt = (ROMCnt & (~mask | 0x20800000)) | (val & 0xFF7F7FFF);

    if (resetrel)
    {
        if (Parent.CPUSelect == Num)
            Parent.UpdateCartState();
    }

    // all this junk would only really be useful if melonDS was interfaced to
    // a DS cart reader
    if (val & (1<<15))
    {
        Parent.Key2_X = 0;
        Parent.Key2_Y = 0;
        for (u32 i = 0; i < 39; i++)
        {
            if (Key2_Seed0 & (1ULL << i)) Parent.Key2_X |= (1ULL << (38-i));
            if (Key2_Seed1 & (1ULL << i)) Parent.Key2_Y |= (1ULL << (38-i));
        }

        Log(LogLevel::Debug, "seed0: %010" PRIx64 "\n", Key2_Seed0);
        Log(LogLevel::Debug, "seed1: %010" PRIx64 "\n", Key2_Seed1);
        Log(LogLevel::Debug, "key2 X: %010" PRIx64 "\n", Parent.Key2_X);
        Log(LogLevel::Debug, "key2 Y: %010" PRIx64 "\n", Parent.Key2_Y);
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

    ROMTransferPos = 0;
    ROMTransferLen = datasize;

    /*printf("ROM COMMAND %04X %08X %02X%02X%02X%02X%02X%02X%02X%02X SIZE %04X\n",
           SPICnt, ROMCnt,
           ROMCommand[0], ROMCommand[1], ROMCommand[2], ROMCommand[3],
           ROMCommand[4], ROMCommand[5], ROMCommand[6], ROMCommand[7],
           datasize);*/

    if (Parent.CartActive && Parent.CPUSelect == Num)
        Parent.Cart->ROMCommandStart(Parent, ROMCommand);

    // reset the FIFO
    ROMDataPosCPU = 0;
    ROMDataPosCart = 0;
    ROMDataCount = 0;
    ROMDataLate = false;

    // ROM transfer timings
    // the bus is parallel with 8 bits
    // thus a command would take 8 cycles to be transferred
    // and it would take 4 cycles to receive a word of data
    // gap1 delay applies before the data transfer
    // gap2 delay applies before each 0x200 byte block (including the first block)
    // TODO: advance read position if bit28 is set

    u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
    u32 cmddelay = 8 + (ROMCnt & 0x1FFF);
    if (datasize) cmddelay += ((ROMCnt >> 16) & 0x3F);

    if (!(ROMCnt & (1<<30)))
    {
        if (datasize == 0)
            Parent.NDS.ScheduleEvent(ROMTransferEvent, false, xfercycle * cmddelay, ROMTransfer_End, 0);
        else
            Parent.NDS.ScheduleEvent(ROMTransferEvent, false, xfercycle * (cmddelay + 4), ROMTransfer_ReceiveData, 0);
    }
    else
    {
        /*
         * Hardware bugs when WR=1:
         *
         * DRQ always gets raised when submitting a command, even if the data length is 0.
         *
         * After sending the command bytes, if the last non-zero delay isn't a multiple of 4,
         * and if the first data word isn't written to GCDATAIN in time,
         * the hardware will accidentally send out one data word even though the FIFO is empty.
         * This has consequences for the rest of the transfer (FIFO desync).
         * TODO: figure out if it is worth the trouble to emulate this
         * TODO: figure out if this also applies to gap2 delays between blocks
         */

        // raise DRQ for the first data word
        RaiseDRQ();

        if (datasize == 0)
            Parent.NDS.ScheduleEvent(ROMTransferEvent, false, xfercycle * cmddelay, ROMTransfer_End, 0);
        else
            Parent.NDS.ScheduleEvent(ROMTransferEvent, false, xfercycle * cmddelay, ROMTransfer_SendData, 0);
    }
}


void NDSCartSlot::Interface::ROMReceiveData(u32 param)
{
    u32 data = 0;
    if (Parent.CartActive)
    {
        if (Parent.CPUSelect == Num)
            data = Parent.Cart->ROMCommandReceive();
        else
            data = 0xFFFFFFFF;
    }
    else if (Parent.PowerState == 1)
        data = 0xFFFFFFFF;

    ROMData[ROMDataPosCart] = data;
    ROMDataPosCart ^= 1;
    ROMDataCount++;

    ROMTransferPos += 4;

    // raise DRQ and trigger DMA if needed

    RaiseDRQ();

    // if there is space in the FIFO, schedule the next transfer

    if (ROMDataCount < 2)
        ROMAdvanceReceive();
    else
        ROMDataLate = true;
}

void NDSCartSlot::Interface::ROMAdvanceReceive()
{
    // end-of-transfer condition is handled when the last data word is read from the FIFO
    if (ROMTransferPos >= ROMTransferLen)
        return;

    u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
    u32 delay = 4;
    if (!(ROMTransferPos & 0x1FF))
        delay += ((ROMCnt >> 16) & 0x3F);

    Parent.NDS.ScheduleEvent(ROMTransferEvent, false, xfercycle * delay, ROMTransfer_ReceiveData, 0);
}

void NDSCartSlot::Interface::ROMSendData(u32 param)
{
    if (ROMDataCount == 0)
    {
        // if we have no data available, keep track of this, and abort
        // the transfer will resume whenever data is written to the buffer

        ROMDataLate = true;
        return;
    }

    // fetch data from the buffer and send it to the cart

    if (Parent.CartActive && Parent.CPUSelect == Num)
    {
        u32 data = ROMData[ROMDataPosCart];
        Parent.Cart->ROMCommandTransmit(data);
    }

    ROMDataPosCart ^= 1;
    ROMDataCount--;

    // if needed, raise DRQ for the next data word, and schedule that transfer

    ROMTransferPos += 4;
    if (ROMTransferPos < ROMTransferLen)
        RaiseDRQ();

    ROMAdvanceSend();
}

void NDSCartSlot::Interface::ROMAdvanceSend()
{
    u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
    u32 delay = 4;

    if (ROMTransferPos < ROMTransferLen)
    {
        if (!(ROMTransferPos & 0x1FF))
            delay += ((ROMCnt >> 16) & 0x3F);

        Parent.NDS.ScheduleEvent(ROMTransferEvent, false, xfercycle * delay, ROMTransfer_SendData, 0);
    }
    else
        Parent.NDS.ScheduleEvent(ROMTransferEvent, false, xfercycle * delay, ROMTransfer_End, 0);
}

void NDSCartSlot::Interface::ROMEndTransfer(u32 param)
{
    ROMCnt &= ~(1<<31);

    ROMTransferPos = 0;
    ROMTransferLen = 0;

    if (SPICnt & (1<<14))
        Parent.NDS.SetIRQ(Num, Parent.TransferIRQ);

    if (Parent.CartActive && Parent.CPUSelect == Num)
        Parent.Cart->ROMCommandFinish();
}

void NDSCartSlot::Interface::RaiseDRQ()
{
    // TODO: the DMA trigger is level-sensitive
    // thus, if a cart DMA gets set up while DRQ is already active, it will start immediately
    // emulating this would require keeping track of the DMA trigger line states somewhere
    // the current handling for this is a hack
    // (DMA triggering needs a cleanup anyway)

    ROMCnt |= (1<<23);
    CheckDMA();
}

void NDSCartSlot::Interface::CheckDMA()
{
    if (!(ROMCnt & (1<<23)))
        return;

    // TODO: make this code suck less!!
    // maybe have a general "DMA trigger function" that covers both DMA types for DSi
    // use a proper enum instead of magic numbers (hardware values) for trigger IDs
    if (Parent.LogicalNum == 0)
    {
        if (Num)
            Parent.NDS.CheckDMAs(1, 0x12);
        else
            Parent.NDS.CheckDMAs(0, 0x05);
    }
    else
    {
        assert(Parent.NDS.ConsoleType == 1);
        auto& dsi = dynamic_cast<melonDS::DSi&>(Parent.NDS);

        // the second cart interface can only be used with NDMA
        if (Num)
            dsi.CheckNDMAs(1, 0x25);
        else
            dsi.CheckNDMAs(0, 0x05);
    }
}

u32 NDSCartSlot::Interface::ReadROMData()
{
    u32 ret = ROMData[ROMDataPosCPU];
    if (ROMCnt & (1<<30))
        return ret;

    ROMDataPosCPU ^= 1;
    if (ROMDataCount > 0)
        ROMDataCount--;

    ROMCnt &= ~(1<<23);

    if (ROMTransferPos < ROMTransferLen)
    {
        // if the FIFO was full, we need to get the transfer going again
        if (ROMDataLate)
        {
            ROMDataLate = false;
            ROMAdvanceReceive();
        }
    }
    else
    {
        if (ROMDataCount == 0)
            ROMEndTransfer(0);
        else
            RaiseDRQ();
    }

    return ret;
}

void NDSCartSlot::Interface::WriteROMData(u32 val, u32 mask)
{
    if (!(ROMCnt & (1<<30)))
        return;

    // FIFO is only advanced when writing to the MSB, same for DRQ logic

    ROMData[ROMDataPosCPU] = (ROMData[ROMDataPosCPU] & ~mask) | (val & mask);
    if (!(mask & 0xFF000000))
        return;

    ROMDataPosCPU ^= 1;
    if (ROMDataCount < 2)
        ROMDataCount++;

    ROMCnt &= ~(1<<23);

    // if we ran late, send data now

    if (ROMDataLate)
    {
        ROMDataLate = false;
        ROMSendData(0);
    }
}


void NDSCartSlot::Interface::WriteSPICnt(u16 val, u16 mask)
{
    val &= mask;

    if (SPISelected && Parent.CartActive && Parent.CPUSelect == Num)
    {
        // Bit 13 selects between ROM and SPI modes.
        // Clearing bit 13 during a SPI transfer causes the SPI chipselect line to go high.
        // Setting it again causes the chipselect line to go low again.
        // Pokémon Typing Adventure uses this when talking to its Bluetooth controller.
        // Setting bit 13 during a ROM transfer also affects the ROM chipselect line, but
        // it's unlikely anything uses this.
        // Toggling bit 15 doesn't affect the chipselect lines.

        if (SPICnt & ~val & (1<<13))
            Parent.Cart->SPIRelease();
        else if (~SPICnt & val & (1<<13))
            Parent.Cart->SPISelect();
    }

    SPICnt = (SPICnt & (~mask | 0x0080)) | (val & 0xE043);

    // AUXSPICNT can be changed during a transfer
    // in this case, the transfer continues until the end, even if bit13 or bit15 are cleared
    // if the transfer speed is changed, the transfer continues at the new speed (TODO)
    if (SPICnt & (1<<7))
        Log(LogLevel::Debug, "!! CHANGING AUXSPICNT DURING TRANSFER: %04X\n", val);
}

u8 NDSCartSlot::Interface::ReadSPIData() const
{
    if (!(SPICnt & (1<<15))) return 0;
    if (!(SPICnt & (1<<13))) return 0;
    if (SPICnt & (1<<7)) return 0; // no cheesing

    return SPIData;
}

void NDSCartSlot::Interface::WriteSPIData(u8 val)
{
    if (!(SPICnt & (1<<15))) return;
    if (!(SPICnt & (1<<13))) return;
    if (SPICnt & (1<<7)) return;

    SPICnt |= (1<<7);

    bool hold = !!(SPICnt & (1<<6));

    if (Parent.CartActive)
    {
        if (Parent.CPUSelect == Num)
        {
            if (!SPISelected)
                Parent.Cart->SPISelect();

            SPIData = Parent.Cart->SPITransmitReceive(val);

            if (!hold)
                Parent.Cart->SPIRelease();
        }
        else
            SPIData = 0xFF;
    }
    else if (Parent.PowerState == 1)
        SPIData = 0xFF;
    else
        SPIData = 0;

    SPISelected = hold;

    // SPI transfers one bit per cycle -> 8 cycles per byte
    u32 delay = 8 * (8 << (SPICnt & 0x3));
    Parent.NDS.ScheduleEvent(SPITransferEvent, false, delay, 0, 0);
}

void NDSCartSlot::Interface::SPITransferDone(u32 param)
{
    SPICnt &= ~(1<<7);
}

}

}