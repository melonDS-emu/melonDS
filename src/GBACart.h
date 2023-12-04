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
    virtual ~CartCommon() = default;

    [[nodiscard]] u32 Type() const { return CartType; }
    virtual u32 Checksum() const { return 0; }

    virtual void Reset();

    virtual void DoSavestate(Savestate* file);

    virtual int SetInput(int num, bool pressed);

    virtual u16 ROMRead(u32 addr) const;
    virtual void ROMWrite(u32 addr, u16 val);

    virtual u8 SRAMRead(u32 addr);
    virtual void SRAMWrite(u32 addr, u8 val);

    [[nodiscard]] virtual const u8* GetROM() const { return nullptr; }
    [[nodiscard]] virtual u32 GetROMLength() const { return 0; }

    virtual u8* GetSaveMemory() const;
    virtual u32 GetSaveMemoryLength() const;
    virtual void SetSaveMemory(const u8* savedata, u32 savelen);
protected:
    CartCommon(GBACart::CartType type);
    friend class GBACartSlot;
private:
    GBACart::CartType CartType;
};

// CartGame -- regular retail game cart (ROM, SRAM)
class CartGame : public CartCommon
{
public:
    CartGame(const u8* rom, u32 len, const u8* sram, u32 sramlen, GBACart::CartType type = GBACart::CartType::Game);
    CartGame(std::unique_ptr<u8[]>&& rom, u32 len, std::unique_ptr<u8[]>&& sram, u32 sramlen, GBACart::CartType type = GBACart::CartType::Game);
    ~CartGame() override;

    u32 Checksum() const override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    u16 ROMRead(u32 addr) const override;
    void ROMWrite(u32 addr, u16 val) override;

    u8 SRAMRead(u32 addr) override;
    void SRAMWrite(u32 addr, u8 val) override;

    [[nodiscard]] const u8* GetROM() const override { return ROM.get(); }
    [[nodiscard]] u32 GetROMLength() const override { return ROMLength; }

    u8* GetSaveMemory() const override;
    u32 GetSaveMemoryLength() const override;
    void SetSaveMemory(const u8* savedata, u32 savelen) override;
protected:
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
        u16 data;
        u16 direction;
        u16 control;

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
        u8 state;
        u8 cmd;
        u8 device;
        u8 manufacturer;
        u8 bank;

    } SRAMFlashState {};

    std::unique_ptr<u8[]> SRAM = nullptr;
    u32 SRAMLength = 0;
    SaveType SRAMType = S_NULL;
private:
    void SetupSave(u32 type);
};

// CartGameSolarSensor -- Boktai game cart
class CartGameSolarSensor : public CartGame
{
public:
    CartGameSolarSensor(const u8* rom, u32 len, const u8* sram, u32 sramlen);
    CartGameSolarSensor(std::unique_ptr<u8[]>&& rom, u32 len, std::unique_ptr<u8[]>&& sram, u32 sramlen);

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    int SetInput(int num, bool pressed) override;

protected:
    void ProcessGPIO() override;

private:
    static const int kLuxLevels[11];

    bool LightEdge = false;
    u8 LightCounter = 0;
    u8 LightSample = 0;
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

    u16 ROMRead(u32 addr) const override;
    void ROMWrite(u32 addr, u16 val) override;

private:
    u8 RAM[0x800000] {};
    u16 RAMEnable = 0;
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
    GBACartSlot(std::unique_ptr<CartCommon>&& cart = nullptr) noexcept;
    ~GBACartSlot() noexcept = default;
    void Reset() noexcept;
    void DoSavestate(Savestate* file) noexcept;

    /// Ejects the cart in the GBA slot (if any)
    /// and inserts the given one.
    ///
    /// To insert a cart that does not require ROM data
    /// (such as the RAM expansion pack),
    /// create it manually with std::make_unique and pass it here.
    ///
    /// @param cart Movable \c unique_ptr to the GBA cart object.
    /// May be \c nullptr, in which case the cart slot remains empty.
    /// @post \c cart is \c nullptr and the underlying object
    /// is moved into the cart slot.
    void SetCart(std::unique_ptr<CartCommon>&& cart) noexcept;
    [[nodiscard]] CartCommon* GetCart() noexcept { return Cart.get(); }
    [[nodiscard]] const CartCommon* GetCart() const noexcept { return Cart.get(); }

    void LoadAddon(int type) noexcept;

    /// @return The cart that was in the cart slot if any,
    /// or \c nullptr if the cart slot was empty.
    std::unique_ptr<CartCommon> EjectCart() noexcept;

    // TODO: make more flexible, support nonbinary inputs
    int SetInput(int num, bool pressed) noexcept;

    void SetOpenBusDecay(u16 val) noexcept { OpenBusDecay = val; }

    u16 ROMRead(u32 addr) const noexcept;
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

    /// Sets the loaded cart's SRAM.
    /// Does nothing if no cart is inserted
    /// or the inserted cart doesn't support SRAM.
    ///
    /// @param savedata Buffer containing the raw contents of the SRAM.
    /// The contents of this buffer are copied into the cart slot,
    /// so the caller may dispose of it after this method returns.
    /// @param savelen The length of the buffer in \c savedata, in bytes.
    void SetSaveMemory(const u8* savedata, u32 savelen) noexcept;

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
std::unique_ptr<CartCommon> ParseROM(std::unique_ptr<u8[]>&& romdata, u32 romlen);
std::unique_ptr<CartCommon> ParseROM(const u8* romdata, u32 romlen, const u8* sramdata, u32 sramlen);

/// @param romdata The ROM data to parse. Will be moved-from.
/// @param romlen Length of romdata in bytes.
/// @param sramdata The save data to add to the cart.
/// May be \c nullptr, in which case the cart will have no save data.
/// @param sramlen Length of sramdata in bytes.
/// May be zero, in which case the cart will have no save data.
/// @return Unique pointer to the parsed GBA cart,
/// or \c nullptr if there was an error.
std::unique_ptr<CartCommon> ParseROM(std::unique_ptr<u8[]>&& romdata, u32 romlen, std::unique_ptr<u8[]>&& sramdata, u32 sramlen);

}

#endif // GBACART_H
