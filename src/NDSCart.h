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

#ifndef NDSCART_H
#define NDSCART_H

#include <string>

#include "types.h"
#include "Savestate.h"
#include "NDS_Header.h"
#include "FATStorage.h"
#include "ROMList.h"

namespace NDSCart
{

enum CartType
{
    Default = 0x001,
    Retail = 0x101,
    RetailNAND = 0x102,
    RetailIR = 0x103,
    RetailBT = 0x104,
    Homebrew = 0x201,
};

// CartCommon -- base code shared by all cart types
class CartCommon
{
public:
    CartCommon(u8* rom, u32 len, u32 chipid, bool badDSiDump);
    virtual ~CartCommon();

    virtual u32 Type() { return CartType::Default; }
    virtual u32 Checksum();

    virtual void Reset();
    virtual void SetupDirectBoot(const std::string& romname);

    virtual void DoSavestate(Savestate* file);

    virtual void SetupSave(u32 type);
    virtual void LoadSave(const u8* savedata, u32 savelen);

    virtual int ROMCommandStart(u8* cmd, u8* data, u32 len);
    virtual void ROMCommandFinish(u8* cmd, u8* data, u32 len);

    virtual u8 SPIWrite(u8 val, u32 pos, bool last);

    virtual u8* GetSaveMemory() const;
    virtual u32 GetSaveMemoryLength() const;

protected:
    void ReadROM(u32 addr, u32 len, u8* data, u32 offset);

    void SetIRQ();

    u8* ROM;
    u32 ROMLength;
    u32 ChipID;
    bool IsDSi;
    bool DSiMode;
    u32 DSiBase;

    u32 CmdEncMode;
    u32 DataEncMode;
};

// CartRetail -- regular retail cart (ROM, SPI SRAM)
class CartRetail : public CartCommon
{
public:
    CartRetail(u8* rom, u32 len, u32 chipid, bool badDSiDump);
    virtual ~CartRetail() override;

    virtual u32 Type() override { return CartType::Retail; }

    virtual void Reset() override;

    virtual void DoSavestate(Savestate* file) override;

    virtual void SetupSave(u32 type) override;
    virtual void LoadSave(const u8* savedata, u32 savelen) override;

    virtual int ROMCommandStart(u8* cmd, u8* data, u32 len) override;

    virtual u8 SPIWrite(u8 val, u32 pos, bool last) override;

    virtual u8* GetSaveMemory() const override;
    virtual u32 GetSaveMemoryLength() const override;

protected:
    void ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset);

    u8 SRAMWrite_EEPROMTiny(u8 val, u32 pos, bool last);
    u8 SRAMWrite_EEPROM(u8 val, u32 pos, bool last);
    u8 SRAMWrite_FLASH(u8 val, u32 pos, bool last);

    u8* SRAM;
    u32 SRAMLength;
    u32 SRAMType;

    u8 SRAMCmd;
    u32 SRAMAddr;
    u32 SRAMFirstAddr;
    u8 SRAMStatus;
};

// CartRetailNAND -- retail cart with NAND SRAM (WarioWare DIY, Jam with the Band, ...)
class CartRetailNAND : public CartRetail
{
public:
    CartRetailNAND(u8* rom, u32 len, u32 chipid);
    ~CartRetailNAND() override;

    virtual u32 Type() override { return CartType::RetailNAND; }

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void LoadSave(const u8* savedata, u32 savelen) override;

    int ROMCommandStart(u8* cmd, u8* data, u32 len) override;
    void ROMCommandFinish(u8* cmd, u8* data, u32 len) override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;

private:
    void BuildSRAMID();

    u32 SRAMBase;
    u32 SRAMWindow;

    u8 SRAMWriteBuffer[0x800];
    u32 SRAMWritePos;
};

// CartRetailIR -- SPI IR device and SRAM
class CartRetailIR : public CartRetail
{
public:
    CartRetailIR(u8* rom, u32 len, u32 chipid, u32 irversion, bool badDSiDump);
    ~CartRetailIR() override;

    virtual u32 Type() override { return CartType::RetailIR; }

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;

private:
    u32 IRVersion;
    u8 IRCmd;
};

// CartRetailBT - Pok�mon Typing Adventure (SPI BT controller)
class CartRetailBT : public CartRetail
{
public:
    CartRetailBT(u8* rom, u32 len, u32 chipid);
    ~CartRetailBT() override;

    virtual u32 Type() override { return CartType::RetailBT; }

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;
};

// CartHomebrew -- homebrew 'cart' (no SRAM, DLDI)
class CartHomebrew : public CartCommon
{
public:
    CartHomebrew(u8* rom, u32 len, u32 chipid);
    ~CartHomebrew() override;

    virtual u32 Type() override { return CartType::Homebrew; }

    void Reset() override;
    void SetupDirectBoot(const std::string& romname) override;

    void DoSavestate(Savestate* file) override;

    int ROMCommandStart(u8* cmd, u8* data, u32 len) override;
    void ROMCommandFinish(u8* cmd, u8* data, u32 len) override;

private:
    void ApplyDLDIPatchAt(u8* binary, u32 dldioffset, const u8* patch, u32 patchlen, bool readonly);
    void ApplyDLDIPatch(const u8* patch, u32 patchlen, bool readonly);
    void ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset);

    FATStorage* SD;
    bool ReadOnly;
};

/// Represents a NDS cart, including header, banner, and ROM data.
/// Can be used to retrieve information about a cart before loading it.
class NDSCartData {
public:
    /// Parses an NDS cart from the given ROM data,
    /// resets its emulated state,
    /// and initializes SRAM if applicable.
    /// If the ROM data is invalid, IsValid() will return false.
    /// @param romdata The ROM data to parse.
    /// The constructed object makes a copy of this buffer,
    /// so the caller may free it after this constructor returns.
    /// @param romlen The length of the ROM data in bytes.
    /// @note This constructor does \em not modify emulator state,
    /// so it can be called at any time.
    /// Use this if you need to extract information from a ROM image
    /// before loading it into the emulator.
    NDSCartData(const u8* romdata, u32 romlen);
    NDSCartData(const NDSCartData&) = delete;
    NDSCartData& operator=(const NDSCartData&) = delete;
    NDSCartData(NDSCartData&&) = delete;
    ~NDSCartData();

    /// @returns The NDS header for this cart.
    /// @note If \c IsValid() returns \c false,
    /// the header is invalid and all values will be zero.
    [[nodiscard]] const NDSHeader& Header() const { return _header; }

    /// @returns The NDS banner for this cart.
    /// @note If \c IsValid() returns \c false,
    /// the banner is invalid and all values will be zero.
    [[nodiscard]] const NDSBanner& Banner() const { return _banner; }

    /// @returns The ROM parameters for this cart
    /// @note If \c IsValid() returns \c false,
    /// all values will be zero.
    [[nodiscard]] const ROMListEntry& ROMParams() const { return _romparams; }

    /// @returns A pointer to the ROM data for this cart.
    /// @note If \c IsValid() returns \c false,
    /// the pointer will be \c nullptr.
    [[nodiscard]] const u8* CartROM() const { return _cart_rom; }

    /// @returns The size of the ROM data for this cart in bytes.
    /// @note If \c IsValid() returns \c false,
    /// the size will be zero.
    [[nodiscard]] u32 CartROMSize() const { return _cart_rom_size; }

    /// @returns The cart ID for this cart.
    /// @note If \c IsValid() returns \c false,
    /// the ID will be zero.
    [[nodiscard]] u32 CartID() const { return _cart_id; }

    /// @returns A pointer to the \c CartCommon object for this cart, or nullptr if the cart is invalid.
    [[nodiscard]] const CartCommon* Cart() const { return _cart; }

    /// @returns A pointer to the \c CartCommon object for this cart, or nullptr if the cart is invalid.
    [[nodiscard]] CartCommon* Cart() { return _cart; }

    /// @returns \c true if the cart data is valid, \c false otherwise.
    [[nodiscard]] bool IsValid() const { return _cart != nullptr; }
private:
    // This function installs the cart data globally
    // but invalidates the provided object to prevent memory access errors,
    // so it needs to access this class's private fields.
    friend bool InsertROM(NDSCartData&& cart);
    NDSHeader _header;
    NDSBanner _banner;
    ROMListEntry _romparams;
    u8* _cart_rom;
    u32 _cart_rom_size;
    u32 _cart_id;
    CartCommon* _cart;
};

extern u16 SPICnt;
extern u32 ROMCnt;

extern u8 ROMCommand[8];

extern bool CartInserted;
extern u8* CartROM;
extern u32 CartROMSize;

extern u32 CartID;

extern NDSHeader Header;
extern NDSBanner Banner;

bool Init();
void DeInit();
void Reset();

void DoSavestate(Savestate* file);

void DecryptSecureArea(u8* out);

/// Loads a Nintendo DS cart object into the emulator.
/// The emulator takes ownership of the cart object and its underlying resources.
/// If a cartridge is already inserted, it is first ejected
/// and its state is discarded.
/// If the provided cart is not valid,
/// then the currently-loaded ROM will not be ejected.
///
/// @param cart Movable reference to cart data.
/// @returns \c true if the cart was successfully loaded,
/// \c false otherwise.
/// @post If the cart was successfully loaded,
/// then \c cart.IsValid() is will be false.
bool InsertROM(NDSCartData&& cart);

/// Loads a Nintendo DS cart object into the emulator.
/// The emulator takes ownership of the cart object and its underlying resources.
/// If a cartridge is already inserted, it is first ejected
/// and its state is discarded.
/// If the provided cart is not valid,
/// then the currently-loaded ROM will not be ejected.
///
/// @param cart Movable reference to cart data.
/// @returns \c true if the cart was successfully loaded,
/// \c false otherwise.
/// @post If the cart was successfully loaded,
/// then \c cart.IsValid() is will be false.
bool InsertROM(std::unique_ptr<NDSCartData>&& cart);

/// Parses a ROM image and loads it into the emulator.
/// This function is equivalent to calling ::ParseROM() and ::InsertROM() in sequence.
/// @param romdata Pointer to the ROM image.
/// The cart emulator maintains its own copy of this data,
/// so the caller is free to discard this data after calling this function.
/// @param romlen The length of the ROM image, in bytes.
/// @returns \c true if the ROM image was successfully loaded,
/// \c false if not.
bool LoadROM(const u8* romdata, u32 romlen);
void LoadSave(const u8* savedata, u32 savelen);
void SetupDirectBoot(const std::string& romname);

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

void EjectCart();

void ResetCart();

void WriteROMCnt(u32 val);
u32 ReadROMData();
void WriteROMData(u32 val);

void WriteSPICnt(u16 val);
u8 ReadSPIData();
void WriteSPIData(u8 val);

void ROMPrepareData(u32 param);
void ROMEndTransfer(u32 param);
void SPITransferDone(u32 param);

}

#endif
