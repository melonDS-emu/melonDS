/*
    Copyright 2016-2025 melonDS team

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

#ifndef UCODEBASE_H
#define UCODEBASE_H

#include <functional>

#include "../DSi_DSP.h"
#include "../Savestate.h"

namespace melonDS
{
namespace DSP_HLE
{

class UcodeBase: public DSPInterface
{
public:
    UcodeBase(melonDS::DSi& dsi);
    virtual ~UcodeBase();
    virtual void Reset();
    virtual void DoSavestate(Savestate* file);

    u32 GetID() { return (UcodeClass << 16) | (UcodeVersion & 0xFFFF); }

    enum {Class_AAC, Class_Graphics, Class_G711};

    bool RecvDataIsReady(u8 index) const;
    bool SendDataIsEmpty(u8 index) const;
    u16 RecvData(u8 index);
    virtual void SendData(u8 index, u16 val);

    u16 DMAChan0GetSrcHigh();
    u16 DMAChan0GetDstHigh();
    u16 AHBMGetDmaChannel(u16 index) const;
    u16 AHBMGetDirection(u16 index) const;
    u16 AHBMGetUnitSize(u16 index) const;

    u16 DataReadA32(u32 addr) const;
    void DataWriteA32(u32 addr, u16 val);
    u16 MMIORead(u16 addr);
    void MMIOWrite(u16 addr, u16 val);
    u16 ProgramRead(u32 addr) const;
    void ProgramWrite(u32 addr, u16 val);
    u16 AHBMRead16(u32 addr);
    u16 AHBMRead32(u32 addr);
    void AHBMWrite16(u32 addr, u16 val);
    void AHBMWrite32(u32 addr, u32 val);

    u16 GetSemaphore() const;
    void SetSemaphore(u16 val);
    void ClearSemaphore(u16 val);
    void MaskSemaphore(u16 val);

    void Start();

    void SampleClock(s16 output[2], s16 input);

protected:
    melonDS::DSi& DSi;
    int UcodeClass;
    int UcodeVersion;

    static const u32 kPipeMonitorAddr;
    static const u32 kPipeBufferAddr;
    static const u32 kMicBufferAddr;

    bool Exit;

    u16 CmdReg[3];
    bool CmdWritten[3];
    u16 ReplyReg[3];
    bool ReplyWritten[3];
    u8 ReplyReadCb[3];
    u32 ReplyReadCbParam[3];

    u16 SemaphoreIn;        // ARM9 -> DSP
    u16 SemaphoreOut;       // DSP -> ARM9
    u16 SemaphoreMask;      // DSP -> ARM9

    bool AudioPlaying;
    bool AudioOutHalve;
    u32 AudioOutAddr;
    u32 AudioOutLength;
    FIFO<s16, 16> AudioOutFIFO;

    bool MicSampling;
    FIFO<s16, 8> MicInFIFO;

    u16* GetDataMemPointer(u32 addr);

    void SendReply(u8 index, u16 val);
    void SetReplyReadCallback(u8 index, u8 callback, u32 param);
    void OnReplyRead(u8 index);

    void SetSemaphoreOut(u16 val);

    u16* LoadPipe(u8 index);
    u32 GetPipeLength(u16* pipe);
    u32 ReadPipe(u16* pipe, u16* data, u32 len);
    u32 WritePipe(u16* pipe, const u16* data, u32 len);

    void ReadARM9Mem(u16* mem, u32 addr, u32 len);
    void WriteARM9Mem(const u16* mem, u32 addr, u32 len);

    void TryStartAudioCmd();
    void AudioOutAdvance();
    void MicInAdvance();
};

}
}

#endif // UCODEBASE_H
