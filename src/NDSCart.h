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

#ifndef NDSCART_H
#define NDSCART_H

#include <array>
#include <string>
#include <memory>
#include <array>

#include "types.h"
#include "Savestate.h"
#include "NDS_Header.h"
#include "FATStorage.h"
#include "ROMList.h"

namespace melonDS
{
class NDS;
}
namespace melonDS::NDSCart
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

class NDSCartSlot;

// CartCommon -- base code shared by all cart types
class CartCommon
{
public:
    CartCommon(u8* rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams);
    virtual ~CartCommon();

    virtual u32 Type() const = 0;
    [[nodiscard]] u32 Checksum() const;

    virtual void Reset();
    virtual void SetupDirectBoot(const std::string& romname, NDS& nds);

    virtual void DoSavestate(Savestate* file);

    virtual void SetupSave(u32 type);
    virtual void LoadSave(const u8* savedata, u32 savelen);

    virtual int ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, u8* cmd, u8* data, u32 len);
    virtual void ROMCommandFinish(u8* cmd, u8* data, u32 len);

    virtual u8 SPIWrite(u8 val, u32 pos, bool last);

    virtual u8* GetSaveMemory() const;
    virtual u32 GetSaveMemoryLength() const;

    [[nodiscard]] const NDSHeader& GetHeader() const { return Header; }
    [[nodiscard]] NDSHeader& GetHeader() { return Header; }

    /// @return The cartridge's banner if available, or \c nullptr if not.
    [[nodiscard]] const NDSBanner* Banner() const;
    [[nodiscard]] const ROMListEntry& GetROMParams() const { return ROMParams; };
    [[nodiscard]] u32 ID() const { return ChipID; }
    [[nodiscard]] const u8* GetROM() const { return ROM; }
    [[nodiscard]] u32 GetROMLength() const { return ROMLength; }
protected:
    void ReadROM(u32 addr, u32 len, u8* data, u32 offset);

    u8* ROM;
    u32 ROMLength;
    u32 ChipID;
    bool IsDSi;
    bool DSiMode;
    u32 DSiBase;

    u32 CmdEncMode;
    u32 DataEncMode;
    // Kept separate from the ROM data so we can decrypt the modcrypt area
    // without touching the overall ROM data
    NDSHeader Header;
    ROMListEntry ROMParams;
};

// CartRetail -- regular retail cart (ROM, SPI SRAM)
class CartRetail : public CartCommon
{
public:
    CartRetail(u8* rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams);
    virtual ~CartRetail() override;

    virtual u32 Type() const override { return CartType::Retail; }

    virtual void Reset() override;

    virtual void DoSavestate(Savestate* file) override;

    virtual void SetupSave(u32 type) override;
    virtual void LoadSave(const u8* savedata, u32 savelen) override;

    virtual int ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, u8* cmd, u8* data, u32 len) override;

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
    CartRetailNAND(u8* rom, u32 len, u32 chipid, ROMListEntry romparams);
    ~CartRetailNAND() override;

    virtual u32 Type() const override { return CartType::RetailNAND; }

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void LoadSave(const u8* savedata, u32 savelen) override;

    int ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, u8* cmd, u8* data, u32 len) override;
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
    CartRetailIR(u8* rom, u32 len, u32 chipid, u32 irversion, bool badDSiDump, ROMListEntry romparams);
    ~CartRetailIR() override;

    virtual u32 Type() const override { return CartType::RetailIR; }

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
    CartRetailBT(u8* rom, u32 len, u32 chipid, ROMListEntry romparams);
    ~CartRetailBT() override;

    virtual u32 Type() const override { return CartType::RetailBT; }

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;
};

// CartHomebrew -- homebrew 'cart' (no SRAM, DLDI)
class CartHomebrew : public CartCommon
{
public:
    CartHomebrew(u8* rom, u32 len, u32 chipid, ROMListEntry romparams);
    ~CartHomebrew() override;

    virtual u32 Type() const override { return CartType::Homebrew; }

    void Reset() override;
    void SetupDirectBoot(const std::string& romname, NDS& nds) override;

    void DoSavestate(Savestate* file) override;

    int ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, u8* cmd, u8* data, u32 len) override;
    void ROMCommandFinish(u8* cmd, u8* data, u32 len) override;

private:
    void ApplyDLDIPatchAt(u8* binary, u32 dldioffset, const u8* patch, u32 patchlen, bool readonly);
    void ApplyDLDIPatch(const u8* patch, u32 patchlen, bool readonly);
    void ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset);

    FATStorage* SD;
    bool ReadOnly;
};

class NDSCartSlot
{
public:
    NDSCartSlot(melonDS::NDS& nds) noexcept;
    ~NDSCartSlot() noexcept;
    void Reset() noexcept;
    void ResetCart() noexcept;
    void DoSavestate(Savestate* file) noexcept;
    void DecryptSecureArea(u8* out) noexcept;

    /// Loads a Nintendo DS cart object into the cart slot.
    /// The cart slot takes ownership of the cart object and its underlying resources
    /// and re-encrypts the ROM's secure area if necessary.
    /// If a cartridge is already inserted, it is first ejected
    /// and its state is discarded.
    /// If the provided cart is not valid,
    /// then the currently-loaded ROM will not be ejected.
    ///
    /// @param cart Movable reference to the cart.
    /// @returns \c true if the cart was successfully loaded,
    /// \c false otherwise.
    /// @post If the cart was successfully loaded,
    /// then \c cart will be \c nullptr
    /// and \c Cart will contain the object that \c cart previously pointed to.
    /// Otherwise, \c cart and \c Cart will be both be unchanged.
    bool InsertROM(std::unique_ptr<CartCommon>&& cart) noexcept;

    /// Parses a ROM image and loads it into the emulator.
    /// This function is equivalent to calling ::ParseROM() and ::InsertROM() in sequence.
    /// @param romdata Pointer to the ROM image.
    /// The cart emulator maintains its own copy of this data,
    /// so the caller is free to discard this data after calling this function.
    /// @param romlen The length of the ROM image, in bytes.
    /// @returns \c true if the ROM image was successfully loaded,
    /// \c false if not.
    bool LoadROM(const u8* romdata, u32 romlen) noexcept;
    void LoadSave(const u8* savedata, u32 savelen) noexcept;
    void SetupDirectBoot(const std::string& romname) noexcept;

    /// This function is intended to allow frontends to save and load SRAM
    /// without using melonDS APIs.
    /// Modifying the emulated SRAM for any other reason is strongly discouraged.
    /// The returned pointer may be invalidated if the emulator is reset,
    /// or when a new game is loaded.
    /// Consequently, don't store the returned pointer for any longer than necessary.
    /// @returns Pointer to this cart's SRAM if a cart is loaded and supports SRAM, otherwise \c nullptr.
    [[nodiscard]] const u8* GetSaveMemory() const noexcept { return Cart ? Cart->GetSaveMemory() : nullptr; }
    [[nodiscard]] u8* GetSaveMemory() noexcept { return Cart ? Cart->GetSaveMemory() : nullptr; }

    /// @returns The length of the buffer returned by ::GetSaveMemory()
    /// if a cart is loaded and supports SRAM, otherwise zero.
    [[nodiscard]] u32 GetSaveMemoryLength() const noexcept { return Cart ? Cart->GetSaveMemoryLength() : 0; }
    void EjectCart() noexcept;
    u32 ReadROMData() noexcept;
    void WriteROMData(u32 val) noexcept;
    void WriteSPICnt(u16 val) noexcept;
    void WriteROMCnt(u32 val) noexcept;
    [[nodiscard]] u8 ReadSPIData() const noexcept;
    void WriteSPIData(u8 val) noexcept;

    [[nodiscard]] CartCommon* GetCart() noexcept { return Cart.get(); }
    [[nodiscard]] const CartCommon* GetCart() const noexcept { return Cart.get(); }

    [[nodiscard]] u8 GetROMCommand(u8 index) const noexcept { return ROMCommand[index]; }
    void SetROMCommand(u8 index, u8 val) noexcept { ROMCommand[index] = val; }

    [[nodiscard]] u32 GetROMCnt() const noexcept { return ROMCnt; }
    [[nodiscard]] u16 GetSPICnt() const noexcept { return SPICnt; }
    void SetSPICnt(u16 val) noexcept { SPICnt = val; }
private:
    friend class CartCommon;
    melonDS::NDS& NDS;
    u16 SPICnt {};
    u32 ROMCnt {};
    std::array<u8, 8> ROMCommand {};
    u8 SPIData;
    u32 SPIDataPos;
    bool SPIHold;

    u32 ROMData;

    std::array<u8, 0x4000> TransferData;
    u32 TransferPos;
    u32 TransferLen;
    u32 TransferDir;
    std::array<u8, 8> TransferCmd;

    std::unique_ptr<CartCommon> Cart;

    std::array<u32, 0x412> Key1_KeyBuf;

    u64 Key2_X;
    u64 Key2_Y;

    void Key1_Encrypt(u32* data) noexcept;
    void Key1_Decrypt(u32* data) noexcept;
    void Key1_ApplyKeycode(u32* keycode, u32 mod) noexcept;
    void Key1_LoadKeyBuf(bool dsi, bool externalBios, u8 *bios, u32 biosLength) noexcept;
    void Key1_InitKeycode(bool dsi, u32 idcode, u32 level, u32 mod, u8 *bios, u32 biosLength) noexcept;
    void Key2_Encrypt(u8* data, u32 len) noexcept;
    void ROMEndTransfer(u32 param) noexcept;
    void ROMPrepareData(u32 param) noexcept;
    void AdvanceROMTransfer() noexcept;
    void SPITransferDone(u32 param) noexcept;
};

/// Parses the given ROM data and constructs a \c NDSCart::CartCommon subclass
/// that can be inserted into the emulator or used to extract information about the cart beforehand.
/// @param romdata The ROM data to parse.
/// The returned cartridge will contain a copy of this data,
/// so the caller may deallocate \c romdata after this function returns.
/// @param romlen The length of the ROM data in bytes.
/// @returns A \c NDSCart::CartCommon object representing the parsed ROM,
/// or \c nullptr if the ROM data couldn't be parsed.
std::unique_ptr<CartCommon> ParseROM(const u8* romdata, u32 romlen);
}

#endif
