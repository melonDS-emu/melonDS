/*
    Copyright 2016-2023 melonDS team

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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "GBACart.h"
#include "CRC32.h"
#include "Platform.h"
#include "Utils.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace GBACart
{

const char SOLAR_SENSOR_GAMECODES[10][5] =
{
    "U3IJ", // Bokura no Taiyou - Taiyou Action RPG (Japan)
    "U3IE", // Boktai - The Sun Is in Your Hand (USA)
    "U3IP", // Boktai - The Sun Is in Your Hand (Europe)
    "U32J", // Zoku Bokura no Taiyou - Taiyou Shounen Django (Japan)
    "U32E", // Boktai 2 - Solar Boy Django (USA)
    "U32P", // Boktai 2 - Solar Boy Django (Europe)
    "U33J", // Shin Bokura no Taiyou - Gyakushuu no Sabata (Japan)
    "A3IJ"  // Boktai - The Sun Is in Your Hand (USA) (Sample)
};

CartCommon::CartCommon(GBACart::CartType type) : CartType(type)
{
}

void CartCommon::Reset()
{
}

void CartCommon::DoSavestate(Savestate* file)
{
    file->Section("GBCS");
}

void CartCommon::SetSaveMemory(const u8* savedata, u32 savelen)
{
}

int CartCommon::SetInput(int num, bool pressed)
{
    return -1;
}

u16 CartCommon::ROMRead(u32 addr) const
{
    return 0;
}

void CartCommon::ROMWrite(u32 addr, u16 val)
{
}

u8 CartCommon::SRAMRead(u32 addr)
{
    return 0;
}

void CartCommon::SRAMWrite(u32 addr, u8 val)
{
}

u8* CartCommon::GetSaveMemory() const
{
    return nullptr;
}

u32 CartCommon::GetSaveMemoryLength() const
{
    return 0;
}

CartGame::CartGame(const u8* rom, u32 len, const u8* sram, u32 sramlen, GBACart::CartType type) :
    CartGame(CopyToUnique(rom, len), len, CopyToUnique(sram, sramlen), sramlen, type)
{
}

CartGame::CartGame(std::unique_ptr<u8[]>&& rom, u32 len, std::unique_ptr<u8[]>&& sram, u32 sramlen, GBACart::CartType type) :
    CartCommon(type),
    ROM(std::move(rom)),
    ROMLength(len),
    SRAM(std::move(sram)),
    SRAMLength(sramlen)
{
    if (SRAM && SRAMLength)
    {
        SetupSave(sramlen);
    }
}

CartGame::~CartGame() = default;
// unique_ptr cleans up the allocated memory

u32 CartGame::Checksum() const
{
    u32 crc = CRC32(ROM.get(), 0xC0, 0);

    // TODO: hash more contents?

    return crc;
}

void CartGame::Reset()
{
    memset(&GPIO, 0, sizeof(GPIO));
}

void CartGame::DoSavestate(Savestate* file)
{
    CartCommon::DoSavestate(file);

    file->Var16(&GPIO.control);
    file->Var16(&GPIO.data);
    file->Var16(&GPIO.direction);

    u32 oldlen = SRAMLength;

    file->Var32(&SRAMLength);

    if (SRAMLength != oldlen)
    {
        // reallocate save memory
        SRAM = SRAMLength ? std::make_unique<u8[]>(SRAMLength) : nullptr;
    }
    if (SRAMLength)
    {
        // fill save memory if data is present
        file->VarArray(SRAM.get(), SRAMLength);
    }
    else
    {
        // no save data, clear the current state
        SRAMType = SaveType::S_NULL;
        SRAM = nullptr;
        return;
    }

    // persist some extra state info
    file->Var8(&SRAMFlashState.bank);
    file->Var8(&SRAMFlashState.cmd);
    file->Var8(&SRAMFlashState.device);
    file->Var8(&SRAMFlashState.manufacturer);
    file->Var8(&SRAMFlashState.state);

    file->Var8((u8*)&SRAMType);

    if ((!file->Saving) && SRAM)
        Platform::WriteGBASave(SRAM.get(), SRAMLength, 0, SRAMLength);
}

void CartGame::SetupSave(u32 type)
{
    // TODO: have type be determined from some list, like in NDSCart
    // and not this gross hack!!
    SRAMLength = type;
    switch (SRAMLength)
    {
    case 512:
        SRAMType = S_EEPROM4K;
        break;
    case 8192:
        SRAMType = S_EEPROM64K;
        break;
    case 32768:
        SRAMType = S_SRAM256K;
        break;
    case 65536:
        SRAMType = S_FLASH512K;
        break;
    case 128*1024:
    case (128*1024 + 0x10): // .sav file with appended real time clock data (ex: emulator mGBA)
        SRAMType = S_FLASH1M;
        break;
    case 0:
        SRAMType = S_NULL;
        break;
    default:
        Log(LogLevel::Warn, "!! BAD GBA SAVE LENGTH %d\n", SRAMLength);
    }

    if (SRAMType == S_FLASH512K)
    {
        // Panasonic 64K chip
        SRAMFlashState.device = 0x1B;
        SRAMFlashState.manufacturer = 0x32;
    }
    else if (SRAMType == S_FLASH1M)
    {
        // Sanyo 128K chip
        SRAMFlashState.device = 0x13;
        SRAMFlashState.manufacturer = 0x62;
    }
}

void CartGame::SetSaveMemory(const u8* savedata, u32 savelen)
{
    SetupSave(savelen);

    u32 len = std::min(savelen, SRAMLength);
    memcpy(SRAM.get(), savedata, len);
    Platform::WriteGBASave(savedata, len, 0, len);
}

u16 CartGame::ROMRead(u32 addr) const
{
    addr &= 0x01FFFFFF;

    if (addr >= 0xC4 && addr < 0xCA)
    {
        if (GPIO.control & 0x1)
        {
            switch (addr)
            {
            case 0xC4: return GPIO.data;
            case 0xC6: return GPIO.direction;
            case 0xC8: return GPIO.control;
            }
        }
        else
            return 0;
    }

    // CHECKME: does ROM mirror?
    if (addr < ROMLength)
        return *(u16*)&ROM[addr];

    return 0;
}

void CartGame::ROMWrite(u32 addr, u16 val)
{
    addr &= 0x01FFFFFF;

    switch (addr)
    {
        case 0xC4:
            GPIO.data &= ~GPIO.direction;
            GPIO.data |= val & GPIO.direction;
            ProcessGPIO();
            break;

        case 0xC6:
            GPIO.direction = val;
            break;

        case 0xC8:
            GPIO.control = val;
            break;

        default:
            Log(LogLevel::Warn, "Unknown GBA GPIO write 0x%02X @ 0x%04X\n", val, addr);
            break;
    }
}

u8 CartGame::SRAMRead(u32 addr)
{
    addr &= 0xFFFF;

    switch (SRAMType)
    {
    case S_EEPROM4K:
    case S_EEPROM64K:
        return SRAMRead_EEPROM(addr);

    case S_FLASH512K:
    case S_FLASH1M:
        return SRAMRead_FLASH(addr);

    case S_SRAM256K:
        return SRAMRead_SRAM(addr);
    default:
        break;
    }

    return 0xFF;
}

void CartGame::SRAMWrite(u32 addr, u8 val)
{
    addr &= 0xFFFF;

    switch (SRAMType)
    {
    case S_EEPROM4K:
    case S_EEPROM64K:
        return SRAMWrite_EEPROM(addr, val);

    case S_FLASH512K:
    case S_FLASH1M:
        return SRAMWrite_FLASH(addr, val);

    case S_SRAM256K:
        return SRAMWrite_SRAM(addr, val);
    default:
        break;
    }
}

u8* CartGame::GetSaveMemory() const
{
    return SRAM.get();
}

u32 CartGame::GetSaveMemoryLength() const
{
    return SRAMLength;
}

void CartGame::ProcessGPIO()
{
}

u8 CartGame::SRAMRead_EEPROM(u32 addr)
{
    return 0;
}

void CartGame::SRAMWrite_EEPROM(u32 addr, u8 val)
{
    // TODO: could be used in homebrew?
}

// mostly ported from DeSmuME
u8 CartGame::SRAMRead_FLASH(u32 addr)
{
    if (SRAMFlashState.cmd == 0) // no cmd
    {
        return *(u8*)&SRAM[addr + 0x10000 * SRAMFlashState.bank];
    }

    switch (SRAMFlashState.cmd)
    {
        case 0x90: // chip ID
            if (addr == 0x0000) return SRAMFlashState.manufacturer;
            if (addr == 0x0001) return SRAMFlashState.device;
            break;
        case 0xF0: // terminate command (TODO: break if non-Macronix chip and not at the end of an ID call?)
            SRAMFlashState.state = 0;
            SRAMFlashState.cmd = 0;
            break;
        case 0xA0: // write command
            break; // ignore here, handled in Write_Flash()
        case 0xB0: // bank switching (128K only)
            break; // ignore here, handled in Write_Flash()
        default:
            Log(LogLevel::Warn, "GBACart_SRAM::Read_Flash: unknown command 0x%02X @ 0x%04X\n", SRAMFlashState.cmd, addr);
            break;
    }

    return 0xFF;
}

// mostly ported from DeSmuME
void CartGame::SRAMWrite_FLASH(u32 addr, u8 val)
{
    switch (SRAMFlashState.state)
    {
        case 0x00:
            if (addr == 0x5555)
            {
                if (val == 0xF0)
                {
                    // reset
                    SRAMFlashState.state = 0;
                    SRAMFlashState.cmd = 0;
                    return;
                }
                else if (val == 0xAA)
                {
                    SRAMFlashState.state = 1;
                    return;
                }
            }
            if (addr == 0x0000)
            {
                if (SRAMFlashState.cmd == 0xB0)
                {
                    // bank switching
                    SRAMFlashState.bank = val;
                    SRAMFlashState.cmd = 0;
                    return;
                }
            }
            break;
        case 0x01:
            if (addr == 0x2AAA && val == 0x55)
            {
                SRAMFlashState.state = 2;
                return;
            }
            SRAMFlashState.state = 0;
            break;
        case 0x02:
            if (addr == 0x5555)
            {
                // send command
                switch (val)
                {
                    case 0x80: // erase
                        SRAMFlashState.state = 0x80;
                        break;
                    case 0x90: // chip ID
                        SRAMFlashState.state = 0x90;
                        break;
                    case 0xA0: // write
                        SRAMFlashState.state = 0;
                        break;
                    default:
                        SRAMFlashState.state = 0;
                        break;
                }

                SRAMFlashState.cmd = val;
                return;
            }
            SRAMFlashState.state = 0;
            break;
        // erase
        case 0x80:
            if (addr == 0x5555 && val == 0xAA)
            {
                SRAMFlashState.state = 0x81;
                return;
            }
            SRAMFlashState.state = 0;
            break;
        case 0x81:
            if (addr == 0x2AAA && val == 0x55)
            {
                SRAMFlashState.state = 0x82;
                return;
            }
            SRAMFlashState.state = 0;
            break;
        case 0x82:
            if (val == 0x30)
            {
                u32 start_addr = addr + 0x10000 * SRAMFlashState.bank;
                memset((u8*)&SRAM[start_addr], 0xFF, 0x1000);

                Platform::WriteGBASave(SRAM.get(), SRAMLength, start_addr, 0x1000);
            }
            SRAMFlashState.state = 0;
            SRAMFlashState.cmd = 0;
            return;
        // chip ID
        case 0x90:
            if (addr == 0x5555 && val == 0xAA)
            {
                SRAMFlashState.state = 0x91;
                return;
            }
            SRAMFlashState.state = 0;
            break;
        case 0x91:
            if (addr == 0x2AAA && val == 0x55)
            {
                SRAMFlashState.state = 0x92;
                return;
            }
            SRAMFlashState.state = 0;
            break;
        case 0x92:
            SRAMFlashState.state = 0;
            SRAMFlashState.cmd = 0;
            return;
        default:
            break;
    }

    if (SRAMFlashState.cmd == 0xA0) // write
    {
        SRAMWrite_SRAM(addr + 0x10000 * SRAMFlashState.bank, val);
        SRAMFlashState.state = 0;
        SRAMFlashState.cmd = 0;
        return;
    }

    Log(LogLevel::Debug, "GBACart_SRAM::Write_Flash: unknown write 0x%02X @ 0x%04X (state: 0x%02X)\n",
        val, addr, SRAMFlashState.state);
}

u8 CartGame::SRAMRead_SRAM(u32 addr)
{
    if (addr >= SRAMLength) return 0xFF;

    return SRAM[addr];
}

void CartGame::SRAMWrite_SRAM(u32 addr, u8 val)
{
    if (addr >= SRAMLength) return;

    u8 prev = *(u8*)&SRAM[addr];
    if (prev != val)
    {
        *(u8*)&SRAM[addr] = val;

        // TODO: optimize this!!
        Platform::WriteGBASave(SRAM.get(), SRAMLength, addr, 1);
    }
}


CartGameSolarSensor::CartGameSolarSensor(const u8* rom, u32 len, const u8* sram, u32 sramlen) :
    CartGameSolarSensor(CopyToUnique(rom, len), len, CopyToUnique(sram, sramlen), sramlen)
{
}

CartGameSolarSensor::CartGameSolarSensor(std::unique_ptr<u8[]>&& rom, u32 len, std::unique_ptr<u8[]>&& sram, u32 sramlen) :
    CartGame(std::move(rom), len, std::move(sram), sramlen, CartType::GameSolarSensor)
{
}

const int CartGameSolarSensor::kLuxLevels[11] = {0, 5, 11, 18, 27, 42, 62, 84, 109, 139, 183};

void CartGameSolarSensor::Reset()
{
    CartGame::Reset();
    LightEdge = false;
    LightCounter = 0;
    LightSample = 0xFF;
    LightLevel = 0;
}

void CartGameSolarSensor::DoSavestate(Savestate* file)
{
    CartGame::DoSavestate(file);

    file->Var8((u8*)&LightEdge);
    file->Var8(&LightCounter);
    file->Var8(&LightSample);
    file->Var8(&LightLevel);
}

int CartGameSolarSensor::SetInput(int num, bool pressed)
{
    if (!pressed) return -1;

    if (num == Input_SolarSensorDown)
    {
        if (LightLevel > 0)
            LightLevel--;

        return LightLevel;
    }
    else if (num == Input_SolarSensorUp)
    {
        if (LightLevel < 10)
            LightLevel++;

        return LightLevel;
    }

    return -1;
}

void CartGameSolarSensor::ProcessGPIO()
{
    if (GPIO.data & 4) return; // Boktai chip select
    if (GPIO.data & 2) // Reset
    {
        u8 prev = LightSample;
        LightCounter = 0;
        LightSample = (0xFF - (0x16 + kLuxLevels[LightLevel]));
        Log(LogLevel::Debug, "Solar sensor reset (sample: 0x%02X -> 0x%02X)\n", prev, LightSample);
    }
    if (GPIO.data & 1 && LightEdge) LightCounter++;

    LightEdge = !(GPIO.data & 1);

    bool sendBit = LightCounter >= LightSample;
    if (GPIO.control & 1)
    {
        GPIO.data = (GPIO.data & GPIO.direction) | ((sendBit << 3) & ~GPIO.direction & 0xF);
    }
}


CartRAMExpansion::CartRAMExpansion() : CartCommon(RAMExpansion)
{
}

CartRAMExpansion::~CartRAMExpansion() = default;

void CartRAMExpansion::Reset()
{
    memset(RAM, 0xFF, sizeof(RAM));
    RAMEnable = 1;
}

void CartRAMExpansion::DoSavestate(Savestate* file)
{
    CartCommon::DoSavestate(file);

    file->VarArray(RAM, sizeof(RAM));
    file->Var16(&RAMEnable);
}

u16 CartRAMExpansion::ROMRead(u32 addr) const
{
    addr &= 0x01FFFFFF;

    if (addr < 0x01000000)
    {
        switch (addr)
        {
        case 0xB0: return 0xFFFF;
        case 0xB2: return 0x0000;
        case 0xB4: return 0x2400;
        case 0xB6: return 0x2424;
        case 0xB8: return 0xFFFF;
        case 0xBA: return 0xFFFF;
        case 0xBC: return 0xFFFF;
        case 0xBE: return 0x7FFF;

        case 0x1FFFC: return 0xFFFF;
        case 0x1FFFE: return 0x7FFF;

        case 0x240000: return RAMEnable;
        case 0x240002: return 0x0000;
        }

        return 0xFFFF;
    }
    else if (addr < 0x01800000)
    {
        if (!RAMEnable) return 0xFFFF;

        return *(u16*)&RAM[addr & 0x7FFFFF];
    }

    return 0xFFFF;
}

void CartRAMExpansion::ROMWrite(u32 addr, u16 val)
{
    addr &= 0x01FFFFFF;

    if (addr < 0x01000000)
    {
        switch (addr)
        {
        case 0x240000:
            RAMEnable = val & 0x0001;
            return;
        }
    }
    else if (addr < 0x01800000)
    {
        if (!RAMEnable) return;

        *(u16*)&RAM[addr & 0x7FFFFF] = val;
    }
}

GBACartSlot::GBACartSlot(std::unique_ptr<CartCommon>&& cart) noexcept : Cart(std::move(cart))
{
}

void GBACartSlot::Reset() noexcept
{
    if (Cart) Cart->Reset();
}

void GBACartSlot::DoSavestate(Savestate* file) noexcept
{
    file->Section("GBAC"); // Game Boy Advance Cartridge

    // little state here
    // no need to save OpenBusDecay, it will be set later

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

std::unique_ptr<CartCommon> ParseROM(std::unique_ptr<u8[]>&& romdata, u32 romlen)
{
    return ParseROM(std::move(romdata), romlen, nullptr, 0);
}

std::unique_ptr<CartCommon> ParseROM(const u8* romdata, u32 romlen, const u8* sramdata, u32 sramlen)
{
    auto [romcopy, romcopylen] = PadToPowerOf2(romdata, romlen);

    return ParseROM(std::move(romcopy), romcopylen, CopyToUnique(sramdata, sramlen), sramlen);
}

std::unique_ptr<CartCommon> ParseROM(const u8* romdata, u32 romlen)
{
    return ParseROM(romdata, romlen, nullptr, 0);
}

std::unique_ptr<CartCommon> ParseROM(std::unique_ptr<u8[]>&& romdata, u32 romlen, std::unique_ptr<u8[]>&& sramdata, u32 sramlen)
{
    if (romdata == nullptr)
    {
        Log(LogLevel::Error, "GBACart: romdata is null\n");
        return nullptr;
    }

    if (romlen == 0)
    {
        Log(LogLevel::Error, "GBACart: romlen is zero\n");
        return nullptr;
    }

    auto [cartrom, cartromsize] = PadToPowerOf2(std::move(romdata), romlen);

    char gamecode[5] = { '\0' };
    memcpy(&gamecode, cartrom.get() + 0xAC, 4);

    bool solarsensor = false;
    for (const char* i : SOLAR_SENSOR_GAMECODES)
    {
        if (strcmp(gamecode, i) == 0)
            solarsensor = true;
    }

    if (solarsensor)
    {
        Log(LogLevel::Info, "GBA solar sensor support detected!\n");
    }

    std::unique_ptr<CartCommon> cart;
    if (solarsensor)
        cart = std::make_unique<CartGameSolarSensor>(std::move(cartrom), cartromsize, std::move(sramdata), sramlen);
    else
        cart = std::make_unique<CartGame>(std::move(cartrom), cartromsize, std::move(sramdata), sramlen);

    cart->Reset();

    // save
    //printf("GBA save file: %s\n", sram);

    // TODO: have a list of sorts like in NDSCart? to determine the savemem type
    //if (Cart) Cart->LoadSave(sram, 0);

    return cart;
}

void GBACartSlot::SetCart(std::unique_ptr<CartCommon>&& cart) noexcept
{
    Cart = std::move(cart);

    if (!Cart)
    {
        Log(LogLevel::Info, "Ejected GBA cart");
        return;
    }

    const u8* cartrom = Cart->GetROM();

    if (cartrom)
    {
        char gamecode[5] = { '\0' };
        memcpy(&gamecode, Cart->GetROM() + 0xAC, 4);
        Log(LogLevel::Info, "Inserted GBA cart with game code: %s\n", gamecode);
    }
    else
    {
        Log(LogLevel::Info, "Inserted GBA cart with no game code (it's probably an accessory)\n");
    }
}

void GBACartSlot::SetSaveMemory(const u8* savedata, u32 savelen) noexcept
{
    if (Cart)
    {
        Cart->SetSaveMemory(savedata, savelen);
    }
}

void GBACartSlot::LoadAddon(int type) noexcept
{
    switch (type)
    {
    case GBAAddon_RAMExpansion:
        Cart = std::make_unique<CartRAMExpansion>();
        break;

    default:
        Log(LogLevel::Warn, "GBACart: !! invalid addon type %d\n", type);
        return;
    }
}

std::unique_ptr<CartCommon> GBACartSlot::EjectCart() noexcept
{
    return std::move(Cart);
    // Cart will be nullptr after this function returns, due to the move
}


int GBACartSlot::SetInput(int num, bool pressed) noexcept
{
    if (Cart) return Cart->SetInput(num, pressed);

    return -1;
}


u16 GBACartSlot::ROMRead(u32 addr) const noexcept
{
    if (Cart) return Cart->ROMRead(addr);

    return ((addr >> 1) & 0xFFFF) | OpenBusDecay;
}

void GBACartSlot::ROMWrite(u32 addr, u16 val) noexcept
{
    if (Cart) Cart->ROMWrite(addr, val);
}

u8 GBACartSlot::SRAMRead(u32 addr) noexcept
{
    if (Cart) return Cart->SRAMRead(addr);

    return 0xFF;
}

void GBACartSlot::SRAMWrite(u32 addr, u8 val) noexcept
{
    if (Cart) Cart->SRAMWrite(addr, val);
}

}

}