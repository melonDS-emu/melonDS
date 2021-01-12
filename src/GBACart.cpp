/*
    Copyright 2019 Arisotura, Raphaël Zumer

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


namespace GBACart_SRAM
{

enum SaveType {
    S_NULL,
    S_EEPROM4K,
    S_EEPROM64K,
    S_SRAM256K,
    S_FLASH512K,
    S_FLASH1M
};

// from DeSmuME
struct FlashProperties
{
    u8 state;
    u8 cmd;
    u8 device;
    u8 manufacturer;
    u8 bank;
};

u8* SRAM;
FILE* SRAMFile;
u32 SRAMLength;
SaveType SRAMType;
FlashProperties SRAMFlashState;

char SRAMPath[1024];

void (*WriteFunc)(u32 addr, u8 val);


void Write_Null(u32 addr, u8 val);
void Write_EEPROM(u32 addr, u8 val);
void Write_SRAM(u32 addr, u8 val);
void Write_Flash(u32 addr, u8 val);


bool Init()
{
    SRAM = NULL;
    SRAMFile = NULL;
    return true;
}

void DeInit()
{
    if (SRAMFile) fclose(SRAMFile);
    if (SRAM) delete[] SRAM;
}

void Reset()
{
    // do nothing, we don't want to clear GBA SRAM on reset
}

void Eject()
{
    if (SRAMFile) fclose(SRAMFile);
    if (SRAM) delete[] SRAM;
    SRAM = NULL;
    SRAMFile = NULL;
    SRAMLength = 0;
    SRAMType = S_NULL;
    SRAMFlashState = {};
}

void DoSavestate(Savestate* file)
{
    file->Section("GBCS"); // Game Boy [Advance] Cart Save

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
        SRAM = NULL;
        SRAMFile = NULL;
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

void LoadSave(const char* path)
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
        WriteFunc = Write_EEPROM;
        break;
    case 8192:
        SRAMType = S_EEPROM64K;
        WriteFunc = Write_EEPROM;
        break;
    case 32768:
        SRAMType = S_SRAM256K;
        WriteFunc = Write_SRAM;
        break;
    case 65536:
        SRAMType = S_FLASH512K;
        WriteFunc = Write_Flash;
        break;
    case 128*1024:
        SRAMType = S_FLASH1M;
        WriteFunc = Write_Flash;
        break;
    default:
        printf("!! BAD SAVE LENGTH %d\n", SRAMLength);
    case 0:
        SRAMType = S_NULL;
        WriteFunc = Write_Null;
        break;
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

void RelocateSave(const char* path, bool write)
{
    if (!write)
    {
        LoadSave(path); // lazy
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

// mostly ported from DeSmuME
u8 Read_Flash(u32 addr)
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

void Write_Null(u32 addr, u8 val) {}

void Write_EEPROM(u32 addr, u8 val)
{
    // TODO: could be used in homebrew?
}

// mostly ported from DeSmuME
void Write_Flash(u32 addr, u8 val)
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
        Write_SRAM(addr + 0x10000 * SRAMFlashState.bank, val);
        SRAMFlashState.state = 0;
        SRAMFlashState.cmd = 0;
        return;
    }

    printf("GBACart_SRAM::Write_Flash: unknown write 0x%02X @ 0x%04X (state: 0x%02X)\n",
        val, addr, SRAMFlashState.state);
}

void Write_SRAM(u32 addr, u8 val)
{
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

u8 Read8(u32 addr)
{
    if (SRAMType == S_NULL)
    {
        return 0xFF;
    }

    if (SRAMType == S_FLASH512K || SRAMType == S_FLASH1M)
    {
        return Read_Flash(addr);
    }

    return *(u8*)&SRAM[addr];
}

u16 Read16(u32 addr)
{
    if (SRAMType == S_NULL)
    {
        return 0xFFFF;
    }

    if (SRAMType == S_FLASH512K || SRAMType == S_FLASH1M)
    {
        u16 val = Read_Flash(addr + 0) |
            (Read_Flash(addr + 1) << 8);
        return val;
    }

    return *(u16*)&SRAM[addr];
}

u32 Read32(u32 addr)
{
    if (SRAMType == S_NULL)
    {
        return 0xFFFFFFFF;
    }

    if (SRAMType == S_FLASH512K || SRAMType == S_FLASH1M)
    {
        u32 val = Read_Flash(addr + 0) |
            (Read_Flash(addr + 1) << 8) |
            (Read_Flash(addr + 2) << 16) |
            (Read_Flash(addr + 3) << 24);
        return val;
    }

    return *(u32*)&SRAM[addr];
}

void Write8(u32 addr, u8 val)
{
    u8 prev = *(u8*)&SRAM[addr];

    WriteFunc(addr, val);
}

void Write16(u32 addr, u16 val)
{
    u16 prev = *(u16*)&SRAM[addr];

    WriteFunc(addr + 0, val & 0xFF);
    WriteFunc(addr + 1, val >> 8 & 0xFF);
}

void Write32(u32 addr, u32 val)
{
    u32 prev = *(u32*)&SRAM[addr];

    WriteFunc(addr + 0, val & 0xFF);
    WriteFunc(addr + 1, val >> 8 & 0xFF);
    WriteFunc(addr + 2, val >> 16 & 0xFF);
    WriteFunc(addr + 3, val >> 24 & 0xFF);
}

}


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
bool HasSolarSensor;
u8* CartROM;
u32 CartROMSize;
u32 CartCRC;
u32 CartID;
GPIO CartGPIO; // overridden GPIO parameters


bool Init()
{
    if (!GBACart_SRAM::Init()) return false;

    CartROM = NULL;

    return true;
}

void DeInit()
{
    if (CartROM) delete[] CartROM;

    GBACart_SRAM::DeInit();
}

void Reset()
{
    // Do not reset cartridge ROM.
    // Prefer keeping the inserted cartridge on reset.
    // This allows resetting a DS game without losing GBA state,
    // and resetting to firmware without the slot being emptied.
    // The Stop function will clear the cartridge state via Eject().

    GBACart_SRAM::Reset();
    GBACart_SolarSensor::Reset();
}

void Eject()
{
    if (CartROM) delete[] CartROM;

    CartInserted = false;
    HasSolarSensor = false;
    CartROM = NULL;
    CartROMSize = 0;
    CartCRC = 0;
    CartID = 0;
    CartGPIO = {};

    GBACart_SRAM::Eject();
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

        // clear the SRAM file handle; writes will not be committed
        if (GBACart_SRAM::SRAMFile)
        {
            fclose(GBACart_SRAM::SRAMFile);
            GBACart_SRAM::SRAMFile = NULL;
        }
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

    file->Var8((u8*)&HasSolarSensor);

    file->Var16(&CartGPIO.control);
    file->Var16(&CartGPIO.data);
    file->Var16(&CartGPIO.direction);

    // now do the rest

    GBACart_SRAM::DoSavestate(file);
    if (HasSolarSensor) GBACart_SolarSensor::DoSavestate(file);
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

    char gamecode[5] = { '\0' };
    fseek(f, 0xAC, SEEK_SET);
    fread(&gamecode, 1, 4, f);
    printf("Game code: %s\n", gamecode);

    for (int i = 0; i < sizeof(SOLAR_SENSOR_GAMECODES)/sizeof(SOLAR_SENSOR_GAMECODES[0]); i++)
    {
        if (strcmp(gamecode, SOLAR_SENSOR_GAMECODES[i]) == 0) HasSolarSensor = true;
    }

    if (HasSolarSensor)
    {
        printf("GBA solar sensor support detected!\n");
    }

    CartROM = new u8[CartROMSize];
    memset(CartROM, 0, CartROMSize);
    fseek(f, 0, SEEK_SET);
    fread(CartROM, 1, len, f);

    fclose(f);

    CartCRC = CRC32(CartROM, CartROMSize);
    printf("ROM CRC32: %08X\n", CartCRC);

    CartInserted = true;

    // save
    printf("Save file: %s\n", sram);
    GBACart_SRAM::LoadSave(sram);

    return true;
}

void RelocateSave(const char* path, bool write)
{
    // derp herp
    GBACart_SRAM::RelocateSave(path, write);
}

// referenced from mGBA
void WriteGPIO(u32 addr, u16 val)
{
    switch (addr)
    {
        case 0xC4:
            CartGPIO.data &= ~CartGPIO.direction;
            CartGPIO.data |= val & CartGPIO.direction;
            if (HasSolarSensor) GBACart_SolarSensor::Process(&CartGPIO);
            break;
        case 0xC6:
            CartGPIO.direction = val;
            break;
        case 0xC8:
            CartGPIO.control = val;
            break;
        default:
            printf("Unknown GBA GPIO write 0x%02X @ 0x%04X\n", val, addr);
    }

    // write the GPIO values in the ROM (if writable)
    if (CartGPIO.control & 1)
    {
        *(u16*)&CartROM[0xC4] = CartGPIO.data;
        *(u16*)&CartROM[0xC6] = CartGPIO.direction;
        *(u16*)&CartROM[0xC8] = CartGPIO.control;
    }
    else
    {
        // GBATEK: "in write-only mode, reads return 00h (or [possibly] other data (...))"
        // ambiguous, but mGBA sets ROM to 00h when switching to write-only, so do the same
        *(u16*)&CartROM[0xC4] = 0;
        *(u16*)&CartROM[0xC6] = 0;
        *(u16*)&CartROM[0xC8] = 0;
    }
}

}


namespace GBACart_SolarSensor
{

bool LightEdge;
u8 LightCounter;
u8 LightSample;
u8 LightLevel; // 0-10 range

// levels from mGBA
const int GBA_LUX_LEVELS[11] = { 0, 5, 11, 18, 27, 42, 62, 84, 109, 139, 183 };
#define LIGHT_VALUE (0xFF - (0x16 + GBA_LUX_LEVELS[LightLevel]))


void Reset()
{
    LightEdge = false;
    LightCounter = 0;
    LightSample = 0xFF;
    LightLevel = 0;
}

void DoSavestate(Savestate* file)
{
    file->Var8((u8*)&LightEdge);
    file->Var8(&LightCounter);
    file->Var8(&LightSample);
    file->Var8(&LightLevel);
}

void Process(GBACart::GPIO* gpio)
{
    if (gpio->data & 4) return; // Boktai chip select
    if (gpio->data & 2) // Reset
    {
        u8 prev = LightSample;
        LightCounter = 0;
        LightSample = LIGHT_VALUE;
        printf("Solar sensor reset (sample: 0x%02X -> 0x%02X)\n", prev, LightSample);
    }
    if (gpio->data & 1 && LightEdge) LightCounter++;

    LightEdge = !(gpio->data & 1);

    bool sendBit = LightCounter >= LightSample;
    if (gpio->control & 1)
    {
        gpio->data = (gpio->data & gpio->direction) | ((sendBit << 3) & ~gpio->direction & 0xF);
    }
}

}
