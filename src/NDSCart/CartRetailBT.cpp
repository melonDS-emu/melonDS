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

#include <cstring>

#include "CartRetailBT.h"
#include "../NDSCart.h"
#include "../Platform.h"
#include "../Utils.h"

// CartRetailBT: NDS cartridge with Bluetooth transceiver (ie. Pokémon Typing Adventure)
// the BT transceiver shares the SPI interface with the save memory

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace NDSCart
{

// 2 seconds at the 33513982 Hz system clock
static constexpr s32 PageTickIntervalCycles = 2 * 33513982;

CartRetailBT::CartRetailBT(const u8* rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata) :
    CartRetailBT(CopyToUnique(rom, len), len, chipid, romparams, std::move(sram), sramlen, userdata)
{
}

CartRetailBT::CartRetailBT(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata) :
    CartRetail(std::move(rom), len, chipid, false, romparams, std::move(sram), sramlen, userdata, CartType::RetailBT)
{
    Log(LogLevel::Info, "POKETYPE CART\n");

    // the game's save lives in the controller's flash; back it with our SRAM
    Keyboard.SetFlash(this);
}

CartRetailBT::~CartRetailBT() = default;

void CartRetailBT::Reset()
{
    CartRetail::Reset();

    RxLen = 0;
    TxLen = 0;
    TxPos = 0;
    SaveActive = false;
    SaveSession = false;
    Keyboard.Reset();

    // Slot is null while the cart is being inserted, so SetSlot() arms the timer too.
    // It runs for the life of the cart: the game can drop the link at any point
    // in play and expects the keyboard to page it back.
    if (Slot)
        Slot->ScheduleCartTimer(PageTickIntervalCycles, 0);
}

void CartRetailBT::DoSavestate(Savestate* file)
{
    CartRetail::DoSavestate(file);

    // States older than 14.1 have none of this, nor a scheduler entry for the
    // timer, so start the link over and the timer again.
    if (!file->Saving && !file->IsAtLeastVersion(14, 1))
    {
        RxLen = 0;
        TxLen = 0;
        TxPos = 0;
        SaveActive = false;
        SaveSession = false;
        Keyboard.Reset();

        if (Slot)
            Slot->ScheduleCartTimer(PageTickIntervalCycles, 0);
        return;
    }

    file->VarArray(RxBuf, sizeof(RxBuf));
    file->Var32(&RxLen);

    file->VarArray(TxBuf, sizeof(TxBuf));
    file->Var32(&TxLen);
    file->Var32(&TxPos);

    // don't trust a loaded state: the buffers are indexed by these unchecked
    if (!file->Saving)
    {
        if (RxLen > BufferSize) RxLen = 0;
        if (TxLen > BufferSize) TxLen = 0;
        if (TxPos > TxLen)      TxPos = TxLen;
    }

    file->VarBool(&SaveActive);
    file->VarBool(&SaveSession);

    Keyboard.DoSavestate(file);
}

void CartRetailBT::SetSlot(NDSCartSlot* slot) noexcept
{
    Slot = slot;

    // Reset() couldn't arm the timer without a slot
    if (Slot)
        Slot->ScheduleCartTimer(PageTickIntervalCycles, 0);
}

void CartRetailBT::OnPageTimer(u32 param)
{
    // page the DS back like a bonded keyboard reconnecting, and time out an
    // Inquiry the keyboard is sitting out until the player presses Fn
    bool sent = Keyboard.PageTick();
    if (Keyboard.InquiryTick())
        sent = true;

    if (sent)
    {
        // don't wait for the driver's next SPI poll
        if (LoadNextPacket())
            RaiseIRQ();
    }

    if (Slot)
        Slot->ScheduleCartTimer(PageTickIntervalCycles, 0);
}

bool CartRetailBT::EnterPairingMode() noexcept
{
    if (!Keyboard.EnterPairingMode()) return false;

    // answer an Inquiry in progress right away
    if (LoadNextPacket())
        RaiseIRQ();
    return true;
}

// BTKeyboard::FlashBackend: the controller's flash, mapped onto SRAM at BTFlashBase.
// Out-of-range reads return 0xFF; out-of-range writes and erases are ignored.
// Writes store bytes verbatim rather than clearing bits like NOR flash: the game
// zero-fills a region, then writes non-zero data over it.

void CartRetailBT::FlushFlash(u32 offset, u32 len)
{
    if (!SRAM || len == 0) return;
    Platform::WriteNDSSave(SRAM.get(), SRAMLength, offset, len, UserData);
}

void CartRetailBT::Read(u32 addr, u8* out, u32 len)
{
    for (u32 i = 0; i < len; i++)
    {
        u32 a = addr + i;
        out[i] = (SRAM && a >= BTFlashBase && (a - BTFlashBase) < SRAMLength)
                 ? SRAM[a - BTFlashBase]
                 : 0xFF;
    }
}

void CartRetailBT::Write(u32 addr, const u8* in, u32 len)
{
    if (!SRAM) return;

    bool any = false;
    u32 lo = 0, hi = 0;
    for (u32 i = 0; i < len; i++)
    {
        u32 a = addr + i;
        if (a < BTFlashBase) continue;
        u32 off = a - BTFlashBase;
        if (off >= SRAMLength) continue;

        SRAM[off] = in[i];
        if (!any) { lo = hi = off; any = true; }
        else { if (off < lo) lo = off; if (off > hi) hi = off; }
    }

    if (any)
        FlushFlash(lo, hi - lo + 1);
}

void CartRetailBT::EraseSector(u32 addr)
{
    if (!SRAM || addr < BTFlashBase) return;

    u32 off = (addr - BTFlashBase) & ~(BTFlashSectorSize - 1);
    if (off >= SRAMLength) return;

    u32 n = BTFlashSectorSize;
    if (off + n > SRAMLength) n = SRAMLength - off;

    memset(&SRAM[off], 0xFF, n);
    FlushFlash(off, n);
}

void CartRetailBT::RaiseIRQ()
{
    if (Slot) Slot->RaiseCardIRQ();
}

void CartRetailBT::SPISelect()
{
    RxLen = 0;
    SaveSession = false;
    CartRetail::SPISelect();
}

void CartRetailBT::SPIRelease()
{
    if (!SaveSession && RxLen > 0)
        HandleFrame();

    // the latch is cleared after the base call, which flushes a pending write
    SPIRoute route = SaveSession ? SPIRoute::SaveMemory : SPIRoute::Bluetooth;
    bool keep = SessionKeepsWriteEnable(route, RxBuf[0], RxLen);

    CartRetail::SPIRelease();

    if (!keep)
        SRAMStatus &= ~(1<<1);
}

u8 CartRetailBT::SPITransmitReceive(u8 val)
{
    // route the session on its first byte (SPISelect() zeroes RxLen)
    if (RxLen == 0)
        SaveSession = ClassifySPISession(val, SaveActive, (SRAMStatus & (1<<1)) != 0) == SPIRoute::SaveMemory;

    if (RxLen < BufferSize)
        RxBuf[RxLen++] = val;

    if (SaveSession)
    {
        SaveActive = true;
        return CartRetail::SPITransmitReceive(val);
    }

    // Sessions are host writes (0x01), the sync byte (0xFF) or host reads (0x02).
    // The driver never reads AUXSPIDATA back during the first two, so a pending
    // packet may only be shifted out during a read.
    if (RxBuf[0] != 0x02)
        return 0x00;

    // the driver clocks dummy bytes to read a pending response out of the controller
    if (TxPos < TxLen)
        return TxBuf[TxPos++];

    return 0;
}

// load the controller's next packet once the driver has read the last one
bool CartRetailBT::LoadNextPacket()
{
    if (TxPos < TxLen) return false;

    std::vector<u8> pkt;
    if (!Keyboard.NextPacket(pkt)) return false;
    if (pkt.size() + 4 > BufferSize) return false;

    // type (2, LE) | payload length (2, BE) | payload
    TxBuf[0] = 0x01;
    TxBuf[1] = 0x00;
    TxBuf[2] = (u8)(pkt.size() >> 8);
    TxBuf[3] = (u8)(pkt.size() & 0xFF);
    memcpy(&TxBuf[4], pkt.data(), pkt.size());

    TxLen = (u32)pkt.size() + 4;
    TxPos = 0;

    return true;
}

void CartRetailBT::HandleFrame()
{
    if (RxLen >= 5 && RxBuf[0] == 0x01)
    {
        u32 len = ((u32)RxBuf[2] << 8) | RxBuf[3];
        if (len >= 1 && RxLen >= 4 + len)
            Keyboard.HostPacket(&RxBuf[4], len);
    }

    // IREQ_MC is raised after the sync byte and while a reply is ready, not on every
    // transaction; raising it more often makes the driver fall back to slow retries
    bool sync = (RxLen == 1 && RxBuf[0] == 0xFF);
    bool replyready = LoadNextPacket();

    // keep raising it until the packet is fully read, or the driver's receive
    // thread can block in OS_ReceiveMessage with nothing to wake it
    if (TxPos < TxLen)
        replyready = true;

    if (sync || replyready)
        RaiseIRQ();
}

}

}
