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

#ifndef NDSCART_CARTRETAILBT_H
#define NDSCART_CARTRETAILBT_H

#include "CartRetail.h"
#include "BTKeyboard.h"

namespace melonDS::NDSCart
{

// which device on the cart's AUX SPI bus a session belongs to
enum class SPIRoute
{
    Bluetooth,
    SaveMemory,
};

// Route an AUX SPI session on its first byte. The Bluetooth framing and the
// EEPROM command set overlap on 0x01 (WRSR / host write frame) and 0x02
// (WRITE / host read), so those go to save memory only once the game has used
// an unambiguous save command (saveactive) and a WREN is latched (writeenabled).
constexpr SPIRoute ClassifySPISession(u8 firstbyte, bool saveactive, bool writeenabled) noexcept
{
    switch (firstbyte)
    {
    case 0x03: // read
    case 0x04: // write disable
    case 0x05: // read status register
    case 0x06: // write enable
    case 0x9F: // read JEDEC ID
        return SPIRoute::SaveMemory;

    case 0x01: // write status register, or a host write frame
    case 0x02: // write, or the host reading a pending packet
        return (saveactive && writeenabled) ? SPIRoute::SaveMemory : SPIRoute::Bluetooth;

    default:
        return SPIRoute::Bluetooth;
    }
}

// Whether a finished session leaves the EEPROM's write-enable latch armed. CartRetail
// only clears it after a WRITE that stored data, and left armed it would route the
// Bluetooth driver's 0x01/0x02 traffic into save memory. So only the WREN itself, a
// save command other than WRSR/WRITE, or an empty chipselect pulse keep it.
constexpr bool SessionKeepsWriteEnable(SPIRoute route, u8 firstbyte, u32 rxlen) noexcept
{
    if (rxlen == 0) return true;
    if (route != SPIRoute::SaveMemory) return false;
    return firstbyte != 0x01 && firstbyte != 0x02;
}

// CartRetailBT - Pokémon Typing Adventure (SPI BT controller)
//
// A Broadcom BCM2070 on the AUX SPI bus, alongside save memory. The game speaks
// HCI to it in a small framing layer; the cart signals back by raising IREQ_MC.
//   FF                           (alone: wake/sync byte)
//   01 00 | 00 04 | 01 03 0C 00  (type | BE length | HCI payload, here HCI_Reset)
class CartRetailBT : public CartRetail, public BTKeyboard::FlashBackend
{
public:
    CartRetailBT(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata);
    CartRetailBT(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata);
    ~CartRetailBT() override;

    void Reset() override;
    void DoSavestate(Savestate* file) override;

    void SPISelect() override;
    void SPIRelease() override;
    u8 SPITransmitReceive(u8 val) override;

    /// Needed to raise IREQ_MC. Set when the cart is inserted, cleared when it is ejected.
    void SetSlot(NDSCartSlot* slot) noexcept;

    /// The slot's cart timer, every 2 seconds while the cart is inserted.
    void OnPageTimer(u32 param);

    void SetAutoPair(bool enable) noexcept { Keyboard.SetAutoPair(enable); }

    /// The player pressing Fn at the registration prompt: makes the keyboard
    /// discoverable. Returns false once the game has already connected.
    bool EnterPairingMode() noexcept;

    [[nodiscard]] bool GameHasKeyboard() const noexcept { return Keyboard.GameHasKeyboard(); }

private:
    // BTKeyboard::FlashBackend: the controller's on-board flash, where the game
    // keeps its save. The game never uses the cart's SPI EEPROM, so the flash is
    // mapped onto the SRAM buffer instead and persisted like a normal save.
    void Read(u32 addr, u8* out, u32 len) override;
    void Write(u32 addr, const u8* in, u32 len) override;
    void EraseSector(u32 addr) override;

    // the game addresses 0xFF00Bxxx..0xFF014xxx; this base fits that in a 64 KB save
    static constexpr u32 BTFlashBase = 0xFF008000;
    static constexpr u32 BTFlashSectorSize = 0x1000;

    void FlushFlash(u32 offset, u32 len);

private:
    void HandleFrame();
    bool LoadNextPacket();
    void RaiseIRQ();

    static constexpr u32 BufferSize = 1024;

    NDSCartSlot* Slot = nullptr;

    u8 RxBuf[BufferSize] {};
    u32 RxLen = 0;

    u8 TxBuf[BufferSize] {};
    u32 TxLen = 0;
    u32 TxPos = 0;

    // set once the game issues a command that can only mean save memory
    bool SaveActive = false;

    // whether the current session was routed to save memory (decided on its first byte)
    bool SaveSession = false;

    BTKeyboard Keyboard;
};

}

#endif
