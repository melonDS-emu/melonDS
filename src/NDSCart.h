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
#include <variant>

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
    UnlicensedR4 = 0x301
};

class NDSCartSlot;

/// Arguments used to create and populate an NDS cart of unknown type.
/// Different carts take different subsets of these arguments,
/// but we won't know which ones to use
/// until we parse the header at runtime.
struct NDSCartArgs
{
    /// The arguments used to load a homebrew SD card image.
    /// If \c nullopt, then the cart will not have an SD card.
    /// Ignored for retail ROMs.
    std::optional<FATStorageArgs> SDCard = std::nullopt;

    /// Save RAM to load into the cartridge.
    /// If \c nullptr, then the cart's SRAM buffer will be empty.
    /// Ignored for homebrew ROMs.
    std::unique_ptr<u8[]> SRAM = nullptr;

    /// The length of the buffer in SRAM.
    /// If 0, then the cart's SRAM buffer will be empty.
    /// Ignored for homebrew ROMs.
    u32 SRAMLength = 0;
};

// CartCommon -- base code shared by all cart types
class CartCommon
{
public:
    CartCommon(const u8* rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, CartType type);
    CartCommon(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, CartType type);
    virtual ~CartCommon();

    [[nodiscard]] u32 Type() const { return CartType; };
    [[nodiscard]] u32 Checksum() const;

    virtual void Reset();
    virtual void SetupDirectBoot(const std::string& romname, NDS& nds);

    virtual void DoSavestate(Savestate* file);


    virtual int ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd, u8* data, u32 len);
    virtual void ROMCommandFinish(const u8* cmd, u8* data, u32 len);

    virtual u8 SPIWrite(u8 val, u32 pos, bool last);

    virtual u8* GetSaveMemory() { return nullptr; }
    virtual const u8* GetSaveMemory() const { return nullptr; }
    virtual u32 GetSaveMemoryLength() const { return 0; }
    virtual void SetSaveMemory(const u8* savedata, u32 savelen) {};

    [[nodiscard]] const NDSHeader& GetHeader() const { return Header; }
    [[nodiscard]] NDSHeader& GetHeader() { return Header; }

    /// @return The cartridge's banner if available, or \c nullptr if not.
    [[nodiscard]] const NDSBanner* Banner() const;
    [[nodiscard]] const ROMListEntry& GetROMParams() const { return ROMParams; };
    [[nodiscard]] u32 ID() const { return ChipID; }
    [[nodiscard]] const u8* GetROM() const { return ROM.get(); }
    [[nodiscard]] u32 GetROMLength() const { return ROMLength; }
protected:
    void ReadROM(u32 addr, u32 len, u8* data, u32 offset) const;

    std::unique_ptr<u8[]> ROM = nullptr;
    u32 ROMLength = 0;
    u32 ChipID = 0;
    bool IsDSi = false;
    bool DSiMode = false;
    u32 DSiBase = 0;

    u32 CmdEncMode = 0;
    u32 DataEncMode = 0;
    // Kept separate from the ROM data so we can decrypt the modcrypt area
    // without touching the overall ROM data
    NDSHeader Header {};
    ROMListEntry ROMParams {};
    const melonDS::NDSCart::CartType CartType = Default;
};

// CartRetail -- regular retail cart (ROM, SPI SRAM)
class CartRetail : public CartCommon
{
public:
    CartRetail(
        const u8* rom,
        u32 len,
        u32 chipid,
        bool badDSiDump,
        ROMListEntry romparams,
        std::unique_ptr<u8[]>&& sram,
        u32 sramlen,
        melonDS::NDSCart::CartType type = CartType::Retail
    );
    CartRetail(
        std::unique_ptr<u8[]>&& rom,
        u32 len, u32 chipid,
        bool badDSiDump,
        ROMListEntry romparams,
        std::unique_ptr<u8[]>&& sram,
        u32 sramlen,
        melonDS::NDSCart::CartType type = CartType::Retail
    );
    ~CartRetail() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void SetSaveMemory(const u8* savedata, u32 savelen) override;

    int ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd, u8* data, u32 len) override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;

    u8* GetSaveMemory() override { return SRAM.get(); }
    const u8* GetSaveMemory() const override { return SRAM.get(); }
    u32 GetSaveMemoryLength() const override { return SRAMLength; }

protected:
    void ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset) const;

    u8 SRAMWrite_EEPROMTiny(u8 val, u32 pos, bool last);
    u8 SRAMWrite_EEPROM(u8 val, u32 pos, bool last);
    u8 SRAMWrite_FLASH(u8 val, u32 pos, bool last);

    std::unique_ptr<u8[]> SRAM = nullptr;
    u32 SRAMLength = 0;
    u32 SRAMType = 0;

    u8 SRAMCmd = 0;
    u32 SRAMAddr = 0;
    u32 SRAMFirstAddr = 0;
    u8 SRAMStatus = 0;
};

// CartRetailNAND -- retail cart with NAND SRAM (WarioWare DIY, Jam with the Band, ...)
class CartRetailNAND : public CartRetail
{
public:
    CartRetailNAND(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen);
    CartRetailNAND(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen);
    ~CartRetailNAND() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void SetSaveMemory(const u8* savedata, u32 savelen) override;

    int ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd, u8* data, u32 len) override;
    void ROMCommandFinish(const u8* cmd, u8* data, u32 len) override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;

private:
    void BuildSRAMID();

    u32 SRAMBase = 0;
    u32 SRAMWindow = 0;

    u8 SRAMWriteBuffer[0x800] {};
    u32 SRAMWritePos = 0;
};

// CartRetailIR -- SPI IR device and SRAM
class CartRetailIR : public CartRetail
{
public:
    CartRetailIR(const u8* rom, u32 len, u32 chipid, u32 irversion, bool badDSiDump, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen);
    CartRetailIR(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, u32 irversion, bool badDSiDump, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen);
    ~CartRetailIR() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;

private:
    u32 IRVersion = 0;
    u8 IRCmd = 0;
};

// CartRetailBT - Pok�mon Typing Adventure (SPI BT controller)
class CartRetailBT : public CartRetail
{
public:
    CartRetailBT(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen);
    CartRetailBT(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen);
    ~CartRetailBT() override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;
};

// CartSD -- any 'cart' with an SD card slot
class CartSD : public CartCommon
{
public:
    CartSD(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, std::optional<FATStorage>&& sdcard = std::nullopt);
    CartSD(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, std::optional<FATStorage>&& sdcard = std::nullopt);
    ~CartSD() override;

    [[nodiscard]] const std::optional<FATStorage>& GetSDCard() const noexcept { return SD; }
    void SetSDCard(FATStorage&& sdcard) noexcept { SD = std::move(sdcard); }
    void SetSDCard(std::optional<FATStorage>&& sdcard) noexcept
    {
        SD = std::move(sdcard);
        sdcard = std::nullopt;
        // moving from an optional doesn't set it to nullopt,
        // it just leaves behind an optional with a moved-from value
    }

    void SetSDCard(std::optional<FATStorageArgs>&& args) noexcept
    {
        // Close the open SD card (if any) so that its contents are flushed to disk.
        // Also, if args refers to the same image file that SD is currently using,
        // this will ensure that we don't have two open read-write handles
        // to the same file.
        SD = std::nullopt;

        if (args)
            SD = FATStorage(std::move(*args));

        args = std::nullopt;
        // moving from an optional doesn't set it to nullopt,
        // it just leaves behind an optional with a moved-from value
    }

protected:
    void ApplyDLDIPatchAt(u8* binary, u32 dldioffset, const u8* patch, u32 patchlen, bool readonly) const;
    void ApplyDLDIPatch(const u8* patch, u32 patchlen, bool readonly);
    void ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset) const;

    std::optional<FATStorage> SD {};
};

// CartHomebrew -- homebrew 'cart' (no SRAM, DLDI)
class CartHomebrew : public CartSD
{
public:
    CartHomebrew(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, std::optional<FATStorage>&& sdcard = std::nullopt);
    CartHomebrew(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, std::optional<FATStorage>&& sdcard = std::nullopt);
    ~CartHomebrew() override;

    void Reset() override;
    void SetupDirectBoot(const std::string& romname, NDS& nds) override;

    int ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd, u8* data, u32 len) override;
    void ROMCommandFinish(const u8* cmd, u8* data, u32 len) override;
};

// CartR4 -- unlicensed R4 'cart' (NDSCartR4.cpp)
enum CartR4Type
{
    /* non-SDHC carts */
    CartR4TypeM3Simply = 0,
    CartR4TypeR4 = 1,
    /* SDHC carts */
    CartR4TypeAce3DS = 2
};

enum CartR4Language
{
    CartR4LanguageJapanese = (7 << 3) | 1,
    CartR4LanguageEnglish = (7 << 3) | 2,
    CartR4LanguageFrench = (2 << 3) | 2,
    CartR4LanguageKorean = (4 << 3) | 2,
    CartR4LanguageSimplifiedChinese = (6 << 3) | 3,
    CartR4LanguageTraditionalChinese = (7 << 3) | 3
};

class CartR4 : public CartSD
{
public:
    CartR4(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, CartR4Type ctype, CartR4Language clanguage,
        std::optional<FATStorage>&& sdcard = std::nullopt);
    ~CartR4() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    int ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd, u8* data, u32 len) override;
    void ROMCommandFinish(const u8* cmd, u8* data, u32 len) override;

private:
    inline u32 GetAdjustedSector(u32 sector) const
    {
        return R4CartType >= CartR4TypeAce3DS ? sector : sector >> 9;
    }

    u16 GetEncryptionKey(u16 sector);
    void ReadSDToBuffer(u32 sector, bool rom);
    u64 SDFATEntrySectorGet(u32 entry, u32 addr);

    s32 EncryptionKey;
    u32 FATEntryOffset[2];
    u8 Buffer[512];
    u8 InitStatus;
    CartR4Type R4CartType;
    CartR4Language CartLanguage;
    bool BufferInitialized;
};

class NDSCartSlot
{
public:
    explicit NDSCartSlot(melonDS::NDS& nds, std::unique_ptr<CartCommon>&& rom = nullptr) noexcept;
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
    /// @param cart Movable reference to the cart,
    /// or \c nullptr to eject the cart.
    /// @post If the cart was successfully loaded,
    /// then \c cart will be \c nullptr
    /// and \c Cart will contain the object that \c cart previously pointed to.
    /// Otherwise, \c cart and \c Cart will be both be unchanged.
    void SetCart(std::unique_ptr<CartCommon>&& cart) noexcept;
    [[nodiscard]] CartCommon* GetCart() noexcept { return Cart.get(); }
    [[nodiscard]] const CartCommon* GetCart() const noexcept { return Cart.get(); }

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
    void SetSaveMemory(const u8* savedata, u32 savelen) noexcept;

    /// @returns The length of the buffer returned by ::GetSaveMemory()
    /// if a cart is loaded and supports SRAM, otherwise zero.
    [[nodiscard]] u32 GetSaveMemoryLength() const noexcept { return Cart ? Cart->GetSaveMemoryLength() : 0; }

    /// @return The cart that was in the slot before it was ejected,
    /// or \c nullptr if the slot was already empty.
    std::unique_ptr<CartCommon> EjectCart() noexcept;
    u32 ReadROMData() noexcept;
    void WriteROMData(u32 val) noexcept;
    void WriteSPICnt(u16 val) noexcept;
    void WriteROMCnt(u32 val) noexcept;
    [[nodiscard]] u8 ReadSPIData() const noexcept;
    void WriteSPIData(u8 val) noexcept;

    [[nodiscard]] u8 GetROMCommand(u8 index) const noexcept { return ROMCommand[index]; }
    void SetROMCommand(u8 index, u8 val) noexcept { ROMCommand[index] = val; }

    [[nodiscard]] u32 GetROMCnt() const noexcept { return ROMCnt; }
    [[nodiscard]] u16 GetSPICnt() const noexcept { return SPICnt; }
    void SetSPICnt(u16 val) noexcept { SPICnt = val; }
private:
    friend class CartCommon;
    melonDS::NDS& NDS;
    u16 SPICnt = 0;
    u32 ROMCnt = 0;
    std::array<u8, 8> ROMCommand {};
    u8 SPIData = 0;
    u32 SPIDataPos = 0;
    bool SPIHold = false;

    u32 ROMData = 0;

    std::array<u8, 0x4000> TransferData {};
    u32 TransferPos = 0;
    u32 TransferLen = 0;
    u32 TransferDir = 0;
    std::array<u8, 8> TransferCmd {};

    std::unique_ptr<CartCommon> Cart = nullptr;

    std::array<u32, 0x412> Key1_KeyBuf {};

    u64 Key2_X = 0;
    u64 Key2_Y = 0;

    void Key1_Encrypt(u32* data) const noexcept;
    void Key1_Decrypt(u32* data) const noexcept;
    void Key1_ApplyKeycode(u32* keycode, u32 mod) noexcept;
    void Key1_LoadKeyBuf(bool dsi, const u8 *bios, u32 biosLength) noexcept;
    void Key1_InitKeycode(bool dsi, u32 idcode, u32 level, u32 mod, const u8 *bios, u32 biosLength) noexcept;
    void Key2_Encrypt(const u8* data, u32 len) noexcept;
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
/// @param sdcard The arguments to use for initializing the SD card.
/// Ignored if the parsed ROM is not homebrew.
/// If not given, the cart will not have an SD card.
/// @returns A \c NDSCart::CartCommon object representing the parsed ROM,
/// or \c nullptr if the ROM data couldn't be parsed.
std::unique_ptr<CartCommon> ParseROM(const u8* romdata, u32 romlen, std::optional<NDSCartArgs>&& args = std::nullopt);
std::unique_ptr<CartCommon> ParseROM(std::unique_ptr<u8[]>&& romdata, u32 romlen, std::optional<NDSCartArgs>&& args = std::nullopt);
}

#endif
