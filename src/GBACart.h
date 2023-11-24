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

#ifndef GBACART_H
#define GBACART_H

#include <memory>
#include "types.h"
#include "Savestate.h"

namespace melonDS::GBACart
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
    explicit CartCommon(CartType type);
    virtual ~CartCommon();

    [[nodiscard]] u32 Type() const noexcept { return CartType; }
    virtual u32 Checksum() const { return 0; }

    virtual void Reset();

    virtual void DoSavestate(Savestate* file);

    virtual void SetupSave(u32 type);
    virtual void LoadSave(const u8* savedata, u32 savelen);

    virtual int SetInput(int num, bool pressed);

    [[nodiscard]] virtual u16 ROMRead(u32 addr) const;
    virtual void ROMWrite(u32 addr, u16 val);

    virtual u8 SRAMRead(u32 addr);
    virtual void SRAMWrite(u32 addr, u8 val);

    [[nodiscard]] virtual const u8* GetROM() const { return nullptr; }
    [[nodiscard]] virtual u32 GetROMLength() const { return 0; }

    [[nodiscard]] virtual u8* GetSaveMemory() const;
    [[nodiscard]] virtual u32 GetSaveMemoryLength() const;
private:
    const melonDS::GBACart::CartType CartType;
};

// CartGame -- regular retail game cart (ROM, SRAM)
class CartGame : public CartCommon
{
public:
    CartGame(u8* rom, u32 len);
    ~CartGame() override;

    [[nodiscard]] u32 Checksum() const override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void SetupSave(u32 type) override;
    void LoadSave(const u8* savedata, u32 savelen) override;

    [[nodiscard]] u16 ROMRead(u32 addr) const override;
    void ROMWrite(u32 addr, u16 val) override;

    u8 SRAMRead(u32 addr) override;
    void SRAMWrite(u32 addr, u8 val) override;

    [[nodiscard]] const u8* GetROM() const override { return ROM.get(); }
    [[nodiscard]] u32 GetROMLength() const override { return ROMLength; }

    [[nodiscard]] u8* GetSaveMemory() const override;
    [[nodiscard]] u32 GetSaveMemoryLength() const override;
protected:
    CartGame(GBACart::CartType type, u8* rom, u32 len);
    virtual void ProcessGPIO();

    u8 SRAMRead_EEPROM(u32 addr);
    void SRAMWrite_EEPROM(u32 addr, u8 val);
    u8 SRAMRead_FLASH(u32 addr);
    void SRAMWrite_FLASH(u32 addr, u8 val);
    u8 SRAMRead_SRAM(u32 addr);
    void SRAMWrite_SRAM(u32 addr, u8 val);

    std::unique_ptr<u8[]> ROM;
    u32 ROMLength;

    struct
    {
        u16 data = 0;
        u16 direction = 0;
        u16 control = 0;

    } GPIO {};

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
        u8 state = 0;
        u8 cmd = 0;
        u8 device = 0;
        u8 manufacturer = 0;
        u8 bank = 0;

    } SRAMFlashState {};

    std::unique_ptr<u8[]> SRAM = nullptr;
    u32 SRAMLength = 0;
    SaveType SRAMType = S_NULL;
};

// CartGameSolarSensor -- Boktai game cart
class CartGameSolarSensor : public CartGame
{
public:
    CartGameSolarSensor(u8* rom, u32 len);
    ~CartGameSolarSensor() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    int SetInput(int num, bool pressed) override;

private:
    void ProcessGPIO() override;

    static const int kLuxLevels[11];

    bool LightEdge = false;
    u8 LightCounter = 0;
    u8 LightSample = 0xFF;
    u8 LightLevel = 0;
};

// CartRAMExpansion -- RAM expansion cart (DS browser, ...)
class CartRAMExpansion : public CartCommon
{
public:
    CartRAMExpansion();
    ~CartRAMExpansion() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    [[nodiscard]] u16 ROMRead(u32 addr) const override;
    void ROMWrite(u32 addr, u16 val) override;

private:
    std::array<u8, 0x800000> RAM {};
    u16 RAMEnable = 1;
};

// possible inputs for GBA carts that might accept user input
enum
{
    Input_SolarSensorDown = 0,
    Input_SolarSensorUp,
};

class GBACartSlot
{
public:
    GBACartSlot(std::unique_ptr<CartCommon>&& cart = nullptr, const u8* sram = nullptr, u32 sramlength = 0) noexcept;
    ~GBACartSlot() noexcept = default;
    void Reset() noexcept;
    void DoSavestate(Savestate* file) noexcept;
    /// Applies the GBACartData to the emulator state and unloads an existing ROM if any.
    /// Upon successful insertion, \c cart will be nullptr and the global GBACart state
    /// (\c CartROM, CartInserted, etc.) will be updated.
    bool InsertROM(std::unique_ptr<CartCommon>&& cart) noexcept;
    bool LoadROM(const u8* romdata, u32 romlen) noexcept;
    void LoadSave(const u8* savedata, u32 savelen) noexcept;

    void LoadAddon(int type) noexcept;

    void EjectCart() noexcept;

    // TODO: make more flexible, support nonbinary inputs
    int SetInput(int num, bool pressed) noexcept;

    void SetOpenBusDecay(u16 val) noexcept { OpenBusDecay = val; }

    [[nodiscard]] u16 ROMRead(u32 addr) const noexcept;
    void ROMWrite(u32 addr, u16 val) noexcept;

    u8 SRAMRead(u32 addr) noexcept;
    void SRAMWrite(u32 addr, u8 val) noexcept;

    /// This function is intended to allow frontends to save and load SRAM
    /// without using melonDS APIs.
    /// Modifying the emulated SRAM for any other reason is strongly discouraged.
    /// The returned pointer may be invalidated if the emulator is reset,
    /// or when a new game is loaded.
    /// Consequently, don't store the returned pointer for any longer than necessary.
    /// @returns Pointer to this cart's SRAM if a cart is loaded and supports SRAM, otherwise \c nullptr.
    [[nodiscard]] u8* GetSaveMemory() noexcept { return Cart ? Cart->GetSaveMemory() : nullptr; }
    [[nodiscard]] const u8* GetSaveMemory() const noexcept { return Cart ? Cart->GetSaveMemory() : nullptr; }

    /// @returns The length of the buffer returned by ::GetSaveMemory()
    /// if a cart is loaded and supports SRAM, otherwise zero.
    [[nodiscard]] u32 GetSaveMemoryLength() const noexcept { return Cart ? Cart->GetSaveMemoryLength() : 0; }
private:
    std::unique_ptr<CartCommon> Cart = nullptr;
    u16 OpenBusDecay = 0;
};

/// Parses the given ROM data and constructs a \c GBACart::CartCommon subclass
/// that can be inserted into the emulator or used to extract information about the cart beforehand.
/// @param romdata The ROM data to parse.
/// The returned cartridge will contain a copy of this data,
/// so the caller may deallocate \c romdata after this function returns.
/// @param romlen The length of the ROM data in bytes.
/// @returns A \c GBACart::CartCommon object representing the parsed ROM,
/// or \c nullptr if the ROM data couldn't be parsed.
std::unique_ptr<CartCommon> ParseROM(const u8* romdata, u32 romlen);

}

#endif // GBACART_H
