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
#include "FATStorage.h"
#include "Utils.h"

#include "NDSCart/CartCommon.h"
#include "NDSCart/CartRetail.h"
#include "NDSCart/CartRetailNAND.h"
#include "NDSCart/CartRetailIR.h"
#include "NDSCart/CartRetailBT.h"
#include "NDSCart/CartSD.h"
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
    ROMTransfer_PrepareData = 0,
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
        Cart->ROMCommandStart(*this, TransferCmd.data());

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