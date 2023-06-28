/*
    Copyright 2016-2022 melonDS team

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

#include <memory>
#include "types.h"
#include "Savestate.h"

namespace GBACart
{

enum CartType
{
    Default = 0x001,
    Game = 0x101,
    GameSolarSensor = 0x102,
    RAMExpansion = 0x201,
};

// CartCommon -- base code shared by all cart types
class CartCommon
{
public:
    CartCommon();
    virtual ~CartCommon();

    virtual u32 Type() const = 0;
    virtual u32 Checksum() const { return 0; }

    virtual void Reset();

    virtual void DoSavestate(Savestate* file);

    virtual void SetupSave(u32 type);
    virtual void LoadSave(const u8* savedata, u32 savelen);

    virtual int SetInput(int num, bool pressed);

    virtual u16 ROMRead(u32 addr);
    virtual void ROMWrite(u32 addr, u16 val);

    virtual u8 SRAMRead(u32 addr);
    virtual void SRAMWrite(u32 addr, u8 val);

    [[nodiscard]] virtual const u8* GetROM() const { return nullptr; }
    [[nodiscard]] virtual u32 GetROMLength() const { return 0; }

    virtual u8* GetSaveMemory() const;
    virtual u32 GetSaveMemoryLength() const;
};

// CartGame -- regular retail game cart (ROM, SRAM)
class CartGame : public CartCommon
{
public:
    CartGame(u8* rom, u32 len);
    virtual ~CartGame() override;

    virtual u32 Type() const override { return CartType::Game; }
    virtual u32 Checksum() const override;

    virtual void Reset() override;

    virtual void DoSavestate(Savestate* file) override;

    virtual void SetupSave(u32 type) override;
    virtual void LoadSave(const u8* savedata, u32 savelen) override;

    virtual u16 ROMRead(u32 addr) override;
    virtual void ROMWrite(u32 addr, u16 val) override;

    virtual u8 SRAMRead(u32 addr) override;
    virtual void SRAMWrite(u32 addr, u8 val) override;

    [[nodiscard]] const u8* GetROM() const override { return ROM; }
    [[nodiscard]] u32 GetROMLength() const override { return ROMLength; }

    virtual u8* GetSaveMemory() const override;
    virtual u32 GetSaveMemoryLength() const override;
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
    u32 SRAMLength;
    SaveType SRAMType;
};

// CartGameSolarSensor -- Boktai game cart
class CartGameSolarSensor : public CartGame
{
public:
    CartGameSolarSensor(u8* rom, u32 len);
    virtual ~CartGameSolarSensor() override;

    virtual u32 Type() const override { return CartType::GameSolarSensor; }

    virtual void Reset() override;

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

// CartRAMExpansion -- RAM expansion cart (DS browser, ...)
class CartRAMExpansion : public CartCommon
{
public:
    CartRAMExpansion();
    ~CartRAMExpansion() override;

    virtual u32 Type() const override { return CartType::RAMExpansion; }

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    u16 ROMRead(u32 addr) override;
    void ROMWrite(u32 addr, u16 val) override;

private:
    u8 RAM[0x800000];
    u16 RAMEnable;
};

// possible inputs for GBA carts that might accept user input
enum
{
    Input_SolarSensorDown = 0,
    Input_SolarSensorUp,
};

extern std::unique_ptr<CartCommon> Cart;

bool Init();
void DeInit();
void Reset();

void DoSavestate(Savestate* file);

/// Parses the given ROM data and constructs a \c GBACart::CartCommon subclass
/// that can be inserted into the emulator or used to extract information about the cart beforehand.
/// @param romdata The ROM data to parse.
/// The returned cartridge will contain a copy of this data,
/// so the caller may deallocate \c romdata after this function returns.
/// @param romlen The length of the ROM data in bytes.
/// @returns A \c GBACart::CartCommon object representing the parsed ROM,
/// or \c nullptr if the ROM data couldn't be parsed.
std::unique_ptr<CartCommon> ParseROM(const u8* romdata, u32 romlen);

/// Applies the GBACartData to the emulator state and unloads an existing ROM if any.
/// Upon successful insertion, \c cart will be nullptr and the global GBACart state
/// (\c CartROM, CartInserted, etc.) will be updated.
bool InsertROM(std::unique_ptr<CartCommon>&& cart);
bool LoadROM(const u8* romdata, u32 romlen);
void LoadSave(const u8* savedata, u32 savelen);

void LoadAddon(int type);

void EjectCart();

//bool LoadROM(const char* path, const char* sram);
//bool LoadROM(const u8* romdata, u32 filelength, const char *sram);
//void RelocateSave(const char* path, bool write);

// TODO: make more flexible, support nonbinary inputs
int SetInput(int num, bool pressed);

void SetOpenBusDecay(u16 val);

u16 ROMRead(u32 addr);
void ROMWrite(u32 addr, u16 val);

u8 SRAMRead(u32 addr);
void SRAMWrite(u32 addr, u8 val);

/// This function is intended to allow frontends to save and load SRAM
/// without using melonDS APIs.
/// Modifying the emulated SRAM for any other reason is strongly discouraged.
/// The returned pointer may be invalidated if the emulator is reset,
/// or when a new game is loaded.
/// Consequently, don't store the returned pointer for any longer than necessary.
/// @returns Pointer to this cart's SRAM if a cart is loaded and supports SRAM, otherwise \c nullptr.
u8* GetSaveMemory();

/// @returns The length of the buffer returned by ::GetSaveMemory()
/// if a cart is loaded and supports SRAM, otherwise zero.
u32 GetSaveMemoryLength();

}

#endif // GBACART_H
