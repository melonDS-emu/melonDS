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

#ifndef GBACART_H
#define GBACART_H

#include "types.h"
#include "Savestate.h"

namespace GBACart
{

// CartCommon -- base code shared by all cart types
class CartCommon
{
public:
    CartCommon();
    virtual ~CartCommon();

    virtual void DoSavestate(Savestate* file);

    virtual void LoadSave(const char* path, u32 type);
    virtual void RelocateSave(const char* path, bool write);

    virtual int SetInput(int num, bool pressed);

    virtual u16 ROMRead(u32 addr);
    virtual void ROMWrite(u32 addr, u16 val);

    virtual u8 SRAMRead(u32 addr);
    virtual void SRAMWrite(u32 addr, u8 val);
};

// CartGame -- regular retail game cart (ROM, SRAM)
class CartGame : public CartCommon
{
public:
    CartGame(u8* rom, u32 len);
    virtual ~CartGame() override;

    virtual void DoSavestate(Savestate* file) override;

    virtual void LoadSave(const char* path, u32 type) override;
    virtual void RelocateSave(const char* path, bool write) override;

    virtual u16 ROMRead(u32 addr) override;
    virtual void ROMWrite(u32 addr, u16 val) override;

    virtual u8 SRAMRead(u32 addr) override;
    virtual void SRAMWrite(u32 addr, u8 val) override;

protected:
    virtual void ProcessGPIO();

    u8 SRAMRead_EEPROM(u32 addr);
    void SRAMWrite_EEPROM(u32 addr, u8 val);
    u8 SRAMRead_FLASH(u32 addr);
    void SRAMWrite_FLASH(u32 addr, u8 val);
    u8 SRAMRead_SRAM(u32 addr);
    void SRAMWrite_SRAM(u32 addr, u8 val);

    u8* ROM;
    u32 ROMLength;

    struct
    {
        u16 data;
        u16 direction;
        u16 control;

    } GPIO;

    enum SaveType
    {
        S_NULL,
        S_EEPROM4K,
        S_EEPROM64K,
        S_SRAM256K,
        S_FLASH512K,
        S_FLASH1M
    };

    // from DeSmuME
    struct
    {
        u8 state;
        u8 cmd;
        u8 device;
        u8 manufacturer;
        u8 bank;

    } SRAMFlashState;

    u8* SRAM;
    FILE* SRAMFile;
    u32 SRAMLength;
    SaveType SRAMType;

    char SRAMPath[1024];
};

// CartGameSolarSensor -- Boktai game cart
class CartGameSolarSensor : public CartGame
{
public:
    CartGameSolarSensor(u8* rom, u32 len);
    virtual ~CartGameSolarSensor() override;

    virtual void DoSavestate(Savestate* file) override;

    virtual int SetInput(int num, bool pressed) override;

private:
    virtual void ProcessGPIO() override;

    static const int kLuxLevels[11];

    bool LightEdge;
    u8 LightCounter;
    u8 LightSample;
    u8 LightLevel;
};

// possible inputs for GBA carts that might accept user input
enum
{
    Input_SolarSensorDown = 0,
    Input_SolarSensorUp,
};

extern bool CartInserted;
extern u8* CartROM;
extern u32 CartROMSize;
extern u32 CartCRC;

bool Init();
void DeInit();
void Reset();
void Eject();

void DoSavestate(Savestate* file);
bool LoadROM(const char* path, const char* sram);
bool LoadROM(const u8* romdata, u32 filelength, const char *sram);
void RelocateSave(const char* path, bool write);

// TODO: make more flexible, support nonbinary inputs
int SetInput(int num, bool pressed);

void SetOpenBusDecay(u16 val);

u16 ROMRead(u32 addr);
void ROMWrite(u32 addr, u16 val);

u8 SRAMRead(u32 addr);
void SRAMWrite(u32 addr, u8 val);

}

#endif // GBACART_H
