/*
    Copyright 2016-2021 Arisotura, Raphaël Zumer

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

#include <stdio.h>
#include <string.h>
#include "GBACart.h"
#include "CRC32.h"
#include "Platform.h"


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


bool CartInserted;
u8* CartROM;
u32 CartROMSize;
u32 CartCRC;
u32 CartID;

CartCommon* Cart;

u16 OpenBusDecay;


CartCommon::CartCommon()
{
}

CartCommon::~CartCommon()
{
}

void CartCommon::DoSavestate(Savestate* file)
{
    file->Section("GBCS");
}

void CartCommon::LoadSave(const char* path, u32 type)
{
}

void CartCommon::RelocateSave(const char* path, bool write)
{
}

int CartCommon::SetInput(int num, bool pressed)
{
    return -1;
}

u16 CartCommon::ROMRead(u32 addr)
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


CartGame::CartGame(u8* rom, u32 len) : CartCommon()
{
    ROM = rom;
    ROMLength = len;

    memset(&GPIO, 0, sizeof(GPIO));

    SRAM = nullptr;
    SRAMFile = nullptr;
    SRAMLength = 0;
    SRAMType = S_NULL;
    SRAMFlashState = {};
}

CartGame::~CartGame()
{
    if (SRAMFile) fclose(SRAMFile);
    if (SRAM) delete[] SRAM;
}

void CartGame::DoSavestate(Savestate* file)
{
    CartCommon::DoSavestate(file);

    file->Var16(&GPIO.control);
    file->Var16(&GPIO.data);
    file->Var16(&GPIO.direction);

    // logic mostly copied from NDSCart_SRAM

    u32 oldlen = SRAMLength;

    file->Var32(&SRAMLength);

    if (SRAMLength != oldlen)
    {
        // reallocate save memory
        if (oldlen) delete[] SRAM;
        if (SRAMLength) SRAM = new u8[SRAMLength];
    }
    if (SRAMLength)
    {
        // fill save memory if data is present
        file->VarArray(SRAM, SRAMLength);
    }
    else
    {
        // no save data, clear the current state
        SRAMType = SaveType::S_NULL;
        if (SRAMFile) fclose(SRAMFile);
        SRAM = nullptr;
        SRAMFile = nullptr;
        return;
    }

    // persist some extra state info
    file->Var8(&SRAMFlashState.bank);
    file->Var8(&SRAMFlashState.cmd);
    file->Var8(&SRAMFlashState.device);
    file->Var8(&SRAMFlashState.manufacturer);
    file->Var8(&SRAMFlashState.state);

    file->Var8((u8*)&SRAMType);
}

void CartGame::LoadSave(const char* path, u32 type)
{
    if (SRAM) delete[] SRAM;

    strncpy(SRAMPath, path, 1023);
    SRAMPath[1023] = '\0';
    SRAMLength = 0;

    FILE* f = Platform::OpenFile(SRAMPath, "r+b");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        SRAMLength = (u32)ftell(f);
        SRAM = new u8[SRAMLength];

        fseek(f, 0, SEEK_SET);
        fread(SRAM, SRAMLength, 1, f);

        SRAMFile = f;
    }

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
        SRAMType = S_FLASH1M;
        break;
    case 0:
        SRAMType = S_NULL;
        break;
    default:
        printf("!! BAD GBA SAVE LENGTH %d\n", SRAMLength);
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

void CartGame::RelocateSave(const char* path, bool write)
{
    if (!write)
    {
        LoadSave(path, 0); // lazy
        return;
    }

    strncpy(SRAMPath, path, 1023);
    SRAMPath[1023] = '\0';

    FILE *f = Platform::OpenFile(path, "r+b");
    if (!f)
    {
        printf("GBACart_SRAM::RelocateSave: failed to create new file. fuck\n");
        return;
    }

    SRAMFile = f;
    fwrite(SRAM, SRAMLength, 1, SRAMFile);
}

u16 CartGame::ROMRead(u32 addr)
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
            printf("Unknown GBA GPIO write 0x%02X @ 0x%04X\n", val, addr);
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
            printf("GBACart_SRAM::Read_Flash: unknown command 0x%02X @ 0x%04X\n", SRAMFlashState.cmd, addr);
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

                if (SRAMFile)
                {
                    fseek(SRAMFile, start_addr, SEEK_SET);
                    fwrite((u8*)&SRAM[start_addr], 1, 0x1000, SRAMFile);
                }
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

    printf("GBACart_SRAM::Write_Flash: unknown write 0x%02X @ 0x%04X (state: 0x%02X)\n",
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

        if (SRAMFile)
        {
            fseek(SRAMFile, addr, SEEK_SET);
            fwrite((u8*)&SRAM[addr], 1, 1, SRAMFile);
        }
    }
}


const int CartGameSolarSensor::kLuxLevels[11] = {0, 5, 11, 18, 27, 42, 62, 84, 109, 139, 183};

CartGameSolarSensor::CartGameSolarSensor(u8* rom, u32 len) : CartGame(rom, len)
{
    LightEdge = false;
    LightCounter = 0;
    LightSample = 0xFF;
    LightLevel = 0;
}

CartGameSolarSensor::~CartGameSolarSensor()
{
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
        printf("Solar sensor reset (sample: 0x%02X -> 0x%02X)\n", prev, LightSample);
    }
    if (GPIO.data & 1 && LightEdge) LightCounter++;

    LightEdge = !(GPIO.data & 1);

    bool sendBit = LightCounter >= LightSample;
    if (GPIO.control & 1)
    {
        GPIO.data = (GPIO.data & GPIO.direction) | ((sendBit << 3) & ~GPIO.direction & 0xF);
    }
}


bool Init()
{
    CartROM = nullptr;

    Cart = nullptr;

    return true;
}

void DeInit()
{
    if (CartROM) delete[] CartROM;

    if (Cart) delete Cart;
}

void Reset()
{
    // Do not reset cartridge ROM.
    // Prefer keeping the inserted cartridge on reset.
    // This allows resetting a DS game without losing GBA state,
    // and resetting to firmware without the slot being emptied.
    // The Stop function will clear the cartridge state via Eject().

    // OpenBusDecay doesn't need to be reset, either, as it will be set
    // through NDS::SetGBASlotTimings().
}

void Eject()
{
    if (CartROM) delete[] CartROM;

    CartInserted = false;
    CartROM = NULL;
    CartROMSize = 0;
    CartCRC = 0;
    CartID = 0;

    if (Cart) delete Cart;
    Cart = nullptr;

    Reset();
}

void DoSavestate(Savestate* file)
{
    file->Section("GBAC"); // Game Boy Advance Cartridge

    // logic mostly copied from NDSCart

    // first we need to reload the cart itself,
    // since unlike with DS, it's not loaded in advance

    file->Var32(&CartROMSize);
    if (!CartROMSize) // no GBA cartridge state? nothing to do here
    {
        // do eject the cartridge if something is inserted
        Eject();
        return;
    }

    u32 oldCRC = CartCRC;
    file->Var32(&CartCRC);

    if (CartCRC != oldCRC)
    {
        // delete and reallocate ROM so that it is zero-padded to its full length
        if (CartROM) delete[] CartROM;
        CartROM = new u8[CartROMSize];
    }

    // only save/load the cartridge header
    //
    // GBA connectivity on DS mainly involves identifying the title currently
    // inserted, reading save data, and issuing commands intercepted here
    // (e.g. solar sensor signals). we don't know of any case where GBA ROM is
    // read directly from DS software. therefore, it is more practical, both
    // from the development and user experience perspectives, to avoid dealing
    // with file dependencies, and store a small portion of ROM data that should
    // satisfy the needs of all known software that reads from the GBA slot.
    //
    // note: in case of a state load, only the cartridge header is restored, but
    // the rest of the ROM data is only cleared (zero-initialized) if the CRC
    // differs. Therefore, loading the GBA cartridge associated with the save state
    // in advance will maintain access to the full ROM contents.
    file->VarArray(CartROM, 192);

    CartInserted = true; // known, because CartROMSize > 0
    file->Var32(&CartCRC);
    file->Var32(&CartID);

    // now do the rest

    if (Cart) Cart->DoSavestate(file);
}

void LoadROMCommon(const char *sram)
{
    char gamecode[5] = { '\0' };
    memcpy(&gamecode, CartROM + 0xAC, 4);
    printf("GBA game code: %s\n", gamecode);

    bool solarsensor = false;
    for (size_t i = 0; i < sizeof(SOLAR_SENSOR_GAMECODES)/sizeof(SOLAR_SENSOR_GAMECODES[0]); i++)
    {
        if (strcmp(gamecode, SOLAR_SENSOR_GAMECODES[i]) == 0)
            solarsensor = true;
    }

    if (solarsensor)
    {
        printf("GBA solar sensor support detected!\n");
    }

    CartCRC = CRC32(CartROM, CartROMSize);
    printf("GBA ROM CRC32: %08X\n", CartCRC);

    CartInserted = true;

    if (solarsensor)
        Cart = new CartGameSolarSensor(CartROM, CartROMSize);
    else
        Cart = new CartGame(CartROM, CartROMSize);

    // save
    printf("GBA save file: %s\n", sram);

    // TODO: have a list of sorts like in NDSCart? to determine the savemem type
    if (Cart) Cart->LoadSave(sram, 0);
}

bool LoadROM(const char* path, const char* sram)
{
    FILE* f = Platform::OpenFile(path, "rb");
    if (!f)
    {
        return false;
    }

    if (CartInserted)
    {
        Reset();
    }

    fseek(f, 0, SEEK_END);
    u32 len = (u32)ftell(f);

    CartROMSize = 0x200;
    while (CartROMSize < len)
        CartROMSize <<= 1;

    CartROM = new u8[CartROMSize];
    memset(CartROM, 0, CartROMSize);
    fseek(f, 0, SEEK_SET);
    fread(CartROM, 1, len, f);
    fclose(f);

    LoadROMCommon(sram);

    return true;
}

bool LoadROM(const u8* romdata, u32 filelength, const char *sram)
{
    CartROMSize = 0x200;
    while (CartROMSize < filelength)
        CartROMSize <<= 1;

    CartROM = new u8[CartROMSize];
    memcpy(CartROM, romdata, filelength);

    LoadROMCommon(sram);

    return true;
}

void RelocateSave(const char* path, bool write)
{
    if (Cart) Cart->RelocateSave(path, write);
}


int SetInput(int num, bool pressed)
{
    if (Cart) return Cart->SetInput(num, pressed);

    return -1;
}


void SetOpenBusDecay(u16 val)
{
    OpenBusDecay = val;
}


u16 ROMRead(u32 addr)
{
    if (Cart) return Cart->ROMRead(addr);

    return ((addr >> 1) & 0xFFFF) | OpenBusDecay;
}

void ROMWrite(u32 addr, u16 val)
{
    if (Cart) Cart->ROMWrite(addr, val);
}

u8 SRAMRead(u32 addr)
{
    if (Cart) return Cart->SRAMRead(addr);

    return 0xFF;
}

void SRAMWrite(u32 addr, u8 val)
{
    if (Cart) Cart->SRAMWrite(addr, val);
}

}
