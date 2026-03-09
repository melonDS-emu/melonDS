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
    SetLogicalNum(Num);

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