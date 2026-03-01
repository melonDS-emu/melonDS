/*
    Copyright 2016-2026 melonDS team

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

#include "NDSCart/CartCommon.h"

namespace melonDS
{
class NDS;
}
namespace melonDS::NDSCart
{

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
    //u32 SPIDataPos = 0;
    //bool SPIHold = false;
    bool SPISelected = false;

    u32 ROMData[2] {};
    u32 ROMDataPosCPU;
    u32 ROMDataPosCart;
    u32 ROMDataCount;
    bool ROMDataLate;

    //std::array<u8, 0x4000> TransferData {};
    u32 TransferPos = 0;
    u32 TransferLen = 0;
    u32 TransferDir = 0; // TODO remove me?
    std::array<u8, 8> TransferCmd {};

    std::unique_ptr<CartCommon> Cart = nullptr;

    std::array<u32, 0x412> Key1_KeyBuf {};

    u64 Key2_X = 0;
    u64 Key2_Y = 0;

    void Key1_Encrypt(u32* data) const noexcept;
    void Key1_Decrypt(u32* data) const noexcept;
    void Key1_ApplyKeycode(u32* keycode, u32 mod) noexcept;
    void Key1_LoadKeyBuf(bool dsi) noexcept;
    void Key1_InitKeycode(bool dsi, u32 idcode, u32 level, u32 mod) noexcept;

    void Key2_Encrypt(const u8* data, u32 len) noexcept;

    void RaiseDRQ() noexcept;
    void ROMEndTransfer(u32 param) noexcept;
    void ROMPrepareData(u32 param) noexcept;
    void ROMSendData(u32 param) noexcept;
    void ROMAdvanceData() noexcept;
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
std::unique_ptr<CartCommon> ParseROM(const u8* romdata, u32 romlen, void* userdata = nullptr, std::optional<NDSCartArgs>&& args = std::nullopt);
std::unique_ptr<CartCommon> ParseROM(std::unique_ptr<u8[]>&& romdata, u32 romlen, void* userdata = nullptr, std::optional<NDSCartArgs>&& args = std::nullopt);
}

#endif
