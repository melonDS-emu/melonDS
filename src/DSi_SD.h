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

#ifndef DSI_SD_H
#define DSI_SD_H

#include <cstring>
#include <variant>
#include "FIFO.h"
#include "FATStorage.h"
#include "DSi_NAND.h"
#include "Savestate.h"

namespace melonDS
{
class DSi_SDDevice;
class DSi;

using Nothing = std::monostate;
using DSiStorage = std::variant<std::monostate, FATStorage, DSi_NAND::NANDImage>;

class DSi_SDHost
{
public:
    /// Creates an SDMMC host.
    DSi_SDHost(melonDS::DSi& dsi, DSi_NAND::NANDImage&& nand, std::optional<FATStorage>&& sdcard = std::nullopt) noexcept;

    /// Creates an SDIO host
    explicit DSi_SDHost(melonDS::DSi& dsi) noexcept;
    ~DSi_SDHost();

    void Reset();

    void DoSavestate(Savestate* file);

    void FinishRX(u32 param);
    void FinishTX(u32 param);
    void SendResponse(u32 val, bool last);
    u32 DataRX(const u8* data, u32 len);
    u32 DataTX(u8* data, u32 len);
    u32 GetTransferrableLen(u32 len) const;

    void CheckRX();
    void CheckTX();
    bool TXReq;

    void SetCardIRQ();

    [[nodiscard]] FATStorage* GetSDCard() noexcept;
    [[nodiscard]] const FATStorage* GetSDCard() const noexcept;
    [[nodiscard]] DSi_NAND::NANDImage* GetNAND() noexcept;
    [[nodiscard]] const DSi_NAND::NANDImage* GetNAND() const noexcept;

    void SetSDCard(FATStorage&& sdcard) noexcept;
    void SetSDCard(std::optional<FATStorage>&& sdcard) noexcept;
    void SetNAND(DSi_NAND::NANDImage&& nand) noexcept;

    u16 Read(u32 addr);
    void Write(u32 addr, u16 val);
    u16 ReadFIFO16();
    void WriteFIFO16(u16 val);
    u32 ReadFIFO32();
    void WriteFIFO32(u32 val);

    void UpdateFIFO32();
    void CheckSwapFIFO();

private:
    melonDS::DSi& DSi;
    u32 Num;

    u16 PortSelect;
    u16 SoftReset;
    u16 SDClock;
    u16 SDOption;

    u32 IRQStatus;  // IF
    u32 IRQMask;    // ~IE

    u16 CardIRQStatus;
    u16 CardIRQMask;
    u16 CardIRQCtl;

    u16 DataCtl;
    u16 Data32IRQ;
    u32 DataMode; // 0=16bit 1=32bit
    u16 BlockCount16, BlockCount32, BlockCountInternal;
    u16 BlockLen16, BlockLen32;
    u16 StopAction;

    u16 Command;
    u32 Param;
    u16 ResponseBuffer[8];

    std::array<std::unique_ptr<DSi_SDDevice>, 2> Ports {};

    u32 CurFIFO; // FIFO accessible for read/write
    FIFO<u16, 0x100> DataFIFO[2];
    FIFO<u32, 0x80> DataFIFO32;

    void UpdateData32IRQ();
    void ClearIRQ(u32 irq);
    void SetIRQ(u32 irq);
    void UpdateIRQ(u32 oldmask);
    void UpdateCardIRQ(u16 oldmask);
};


class DSi_SDDevice
{
public:
    DSi_SDDevice(DSi_SDHost* host) { Host = host; IRQ = false; ReadOnly = false; }
    virtual ~DSi_SDDevice() {}

    virtual void Reset() = 0;

    virtual void DoSavestate(Savestate* file) = 0;

    virtual void SendCMD(u8 cmd, u32 param) = 0;
    virtual void ContinueTransfer() = 0;

    bool IRQ;
    bool ReadOnly;

protected:
    DSi_SDHost* Host;
};


class DSi_MMCStorage : public DSi_SDDevice
{
public:
    DSi_MMCStorage(melonDS::DSi& dsi, DSi_SDHost* host, DSi_NAND::NANDImage&& nand) noexcept;
    DSi_MMCStorage(melonDS::DSi& dsi, DSi_SDHost* host, FATStorage&& sdcard) noexcept;
    ~DSi_MMCStorage() override;

    [[nodiscard]] FATStorage* GetSDCard() noexcept { return std::get_if<FATStorage>(&Storage); }
    [[nodiscard]] const FATStorage* GetSDCard() const noexcept { return std::get_if<FATStorage>(&Storage); }
    [[nodiscard]] DSi_NAND::NANDImage* GetNAND() noexcept { return std::get_if<DSi_NAND::NANDImage>(&Storage); }
    [[nodiscard]] const DSi_NAND::NANDImage* GetNAND() const noexcept { return std::get_if<DSi_NAND::NANDImage>(&Storage); }

    void SetNAND(DSi_NAND::NANDImage&& nand) noexcept { Storage = std::move(nand); }
    void SetSDCard(FATStorage&& sdcard) noexcept { Storage = std::move(sdcard); }
    void SetSDCard(std::optional<FATStorage>&& sdcard) noexcept
    {
        if (sdcard)
        { // If we're setting a new SD card...
            Storage = std::move(*sdcard);
            sdcard = std::nullopt;
        }
        else
        {
            Storage = Nothing();
        }
    }

    void SetStorage(DSiStorage&& storage) noexcept
    {
        Storage = std::move(storage);
        storage = Nothing();
        // not sure if a moved-from variant is empty or contains a moved-from object;
        // better to be safe than sorry
    }

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void SetCID(const u8* cid) { memcpy(CID, cid, sizeof(CID)); }

    void SendCMD(u8 cmd, u32 param) override;
    void SendACMD(u8 cmd, u32 param);

    void ContinueTransfer() override;

private:
    static constexpr u8 DSiSDCardCID[16] = {0xBD, 0x12, 0x34, 0x56, 0x78, 0x03, 0x4D, 0x30, 0x30, 0x46, 0x50, 0x41, 0x00, 0x00, 0x15, 0x00};
    melonDS::DSi& DSi;
    DSiStorage Storage;

    u8 CID[16];
    u8 CSD[16];

    u32 CSR;
    u32 OCR;
    u32 RCA;
    u8 SCR[8];
    u8 SSR[64];

    u32 BlockSize;
    u64 RWAddress;
    u32 RWCommand;

    void SetState(u32 state) { CSR &= ~(0xF << 9); CSR |= (state << 9); }

    u32 ReadBlock(u64 addr);
    u32 WriteBlock(u64 addr);
};

}
#endif // DSI_SD_H
