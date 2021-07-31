/*
    Copyright 2016-2021 Arisotura

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

#include <string.h>
#include "FIFO.h"


class DSi_SDDevice;


class DSi_SDHost
{
public:
    DSi_SDHost(u32 num);
    ~DSi_SDHost();

    void Reset();

    void DoSavestate(Savestate* file);

    static void FinishRX(u32 param);
    static void FinishTX(u32 param);
    void SendResponse(u32 val, bool last);
    u32 DataRX(u8* data, u32 len);
    u32 DataTX(u8* data, u32 len);
    u32 GetTransferrableLen(u32 len);

    void CheckRX();
    void CheckTX();
    bool TXReq;

    void SetCardIRQ();

    u16 Read(u32 addr);
    void Write(u32 addr, u16 val);
    u16 ReadFIFO16();
    void WriteFIFO16(u16 val);
    u32 ReadFIFO32();
    void WriteFIFO32(u32 val);

    void UpdateFIFO32();
    void CheckSwapFIFO();

private:
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

    DSi_SDDevice* Ports[2];

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
    DSi_SDDevice(DSi_SDHost* host) { Host = host; IRQ = false; }
    virtual ~DSi_SDDevice() {}

    virtual void Reset() = 0;

    virtual void SendCMD(u8 cmd, u32 param) = 0;
    virtual void ContinueTransfer() = 0;

    bool IRQ;

protected:
    DSi_SDHost* Host;
};


class DSi_MMCStorage : public DSi_SDDevice
{
public:
    DSi_MMCStorage(DSi_SDHost* host, bool internal, FILE* file);
    ~DSi_MMCStorage();

    void Reset();

    void SetCID(u8* cid) { memcpy(CID, cid, 16); }

    void SendCMD(u8 cmd, u32 param);
    void SendACMD(u8 cmd, u32 param);

    void ContinueTransfer();

private:
    bool Internal;
    FILE* File;

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

#endif // DSI_SD_H
