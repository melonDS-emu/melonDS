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

#include "../DSi.h"
#include "UcodeBase.h"
#include "../Platform.h"


namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace DSP_HLE
{


const u32 UcodeBase::kPipeMonitorAddr = 0x0800;
const u32 UcodeBase::kPipeBufferAddr  = 0x1000;
const u32 UcodeBase::kMicBufferAddr   = 0x2000;


UcodeBase::UcodeBase(melonDS::DSi& dsi) : DSi(dsi)
{
}

UcodeBase::~UcodeBase()
{
}

void UcodeBase::Reset()
{
    Exit = false;

    memset(CmdReg, 0, sizeof(CmdReg));
    memset(CmdWritten, 0, sizeof(CmdWritten));
    memset(ReplyReg, 0, sizeof(ReplyReg));
    memset(ReplyWritten, 0, sizeof(ReplyWritten));
    ReplyReadCb[0] = 0;
    ReplyReadCb[1] = 0;
    ReplyReadCb[2] = 0;
    ReplyReadCbParam[0] = 0;
    ReplyReadCbParam[1] = 0;
    ReplyReadCbParam[2] = 0;

    SemaphoreIn = 0;
    SemaphoreOut = 0;
    SemaphoreMask = 0;

    AudioPlaying = false;
    AudioOutHalve = false;
    AudioOutAddr = 0;
    AudioOutLength = 0;
    AudioOutFIFO.Clear();

    MicSampling = false;
    MicInFIFO.Clear();
}

void UcodeBase::DoSavestate(Savestate *file)
{
    file->Section("DSPH");

    file->Bool32(&Exit);

    file->VarArray(CmdReg, sizeof(CmdReg));
    file->VarArray(CmdWritten, sizeof(CmdWritten));
    file->VarArray(ReplyReg, sizeof(ReplyReg));
    file->VarArray(ReplyWritten, sizeof(ReplyWritten));
    file->VarArray(ReplyReadCb, sizeof(ReplyReadCb));
    file->VarArray(ReplyReadCbParam, sizeof(ReplyReadCbParam));

    file->Var16(&SemaphoreIn);
    file->Var16(&SemaphoreOut);
    file->Var16(&SemaphoreMask);

    file->Bool32(&AudioPlaying);
    file->Bool32(&AudioOutHalve);
    file->Var32(&AudioOutAddr);
    file->Var32(&AudioOutLength);
    AudioOutFIFO.DoSavestate(file);

    file->Bool32(&MicSampling);
    MicInFIFO.DoSavestate(file);
}


bool UcodeBase::RecvDataIsReady(u8 index) const
{
    return ReplyWritten[index];
}

bool UcodeBase::SendDataIsEmpty(u8 index) const
{
    return !CmdWritten[index];
}

u16 UcodeBase::RecvData(u8 index)
{
    if (!ReplyWritten[index]) return 0; // CHECKME

    u16 ret = ReplyReg[index];
    ReplyWritten[index] = false;

    if (ReplyReadCb[index])
    {
        OnReplyRead(index);
        ReplyReadCb[index] = 0;
    }

    return ret;
}

void UcodeBase::SendData(u8 index, u16 val)
{
    // TODO less ambiguous naming for those functions
    if (CmdWritten[index])
    {
        printf("??? trying to write cmd but there's already one\n");
        return; // CHECKME
    }

    CmdReg[index] = val;
    CmdWritten[index] = true;

    if (Exit) return;

    if (index == 2)
    {
        if (val == 0x8000)
        {
            // stop DSP
            SendReply(2, 0x8000);
            Exit = true;
        }
        else if (val == 5)
        {
            // received data on audio pipe, try to run an audio command
            TryStartAudioCmd();
        }

        CmdWritten[2] = false;
    }
}


void UcodeBase::SendReply(u8 index, u16 val)
{
    if (ReplyWritten[index])
    {
        printf("??? trying to write reply but there's already one\n");
        return;
    }

    ReplyReg[index] = val;
    ReplyWritten[index] = true;

    switch (index)
    {
    case 0: DSi.DSP.IrqRep0(); break;
    case 1: DSi.DSP.IrqRep1(); break;
    case 2: DSi.DSP.IrqRep2(); break;
    }
}

void UcodeBase::SetReplyReadCallback(u8 index, u8 callback, u32 param)
{
    ReplyReadCb[index] = callback;
    ReplyReadCbParam[index] = param;

    if (!ReplyWritten[index])
    {
        OnReplyRead(index);
        ReplyReadCb[index] = 0;
    }
}

void UcodeBase::OnReplyRead(u8 index)
{
    if (index == 2)
    {
        switch (ReplyReadCb[2])
        {
        case 1:
            {
                // finish init
                // after sync replies were read, send pipe monitor addr
                SendReply(2, kPipeMonitorAddr);
                SetSemaphoreOut(0x8000);
            }
            break;

        case 2:
            {
                // response to "play sound" command
                u16* pipe = LoadPipe(4);
                u32 outlen = ReplyReadCbParam[2];
                u16 resp[4] = {0x0000, 0x1200, (u16)(outlen >> 16), (u16)(outlen & 0xFFFF)};
                WritePipe(pipe, resp, 4);
            }
            break;

        case 3:
            {
                // response to mic commands
                u16* rpipe = LoadPipe(4);
                u32 cmd = ReplyReadCbParam[2];
                u16 resp[4] = {(u16)(cmd >> 16), (u16)(cmd & 0xFFFF),
                               (u16)(kMicBufferAddr >> 16), (u16)(kMicBufferAddr & 0xFFFF)};
                WritePipe(rpipe, resp, 4);
            }
            break;
        }
    }
}


u16 UcodeBase::DMAChan0GetSrcHigh()
{
    // TODO?
    return 0;
}

u16 UcodeBase::DMAChan0GetDstHigh()
{
    // TODO?
    return 0;
}

u16 UcodeBase::AHBMGetDmaChannel(u16 index) const
{
    return 0;
}

u16 UcodeBase::AHBMGetDirection(u16 index) const
{
    return 0;
}

u16 UcodeBase::AHBMGetUnitSize(u16 index) const
{
    return 0;
}


u16 UcodeBase::DataReadA32(u32 addr) const
{
    addr <<= 1;
    u8* ptr = DSi.NWRAMMap_C[2][(addr >> 15) & 0x7];
    return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
}

void UcodeBase::DataWriteA32(u32 addr, u16 val)
{
    addr <<= 1;
    u8* ptr = DSi.NWRAMMap_C[2][(addr >> 15) & 0x7];
    if (ptr) *(u16*)&ptr[addr & 0x7FFF] = val;
}

u16 UcodeBase::MMIORead(u16 addr)
{
    return 0;
}

void UcodeBase::MMIOWrite(u16 addr, u16 val)
{
}

u16 UcodeBase::ProgramRead(u32 addr) const
{
    addr <<= 1;
    u8* ptr = DSi.NWRAMMap_B[2][(addr >> 15) & 0x7];
    return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
}

void UcodeBase::ProgramWrite(u32 addr, u16 val)
{
    addr <<= 1;
    u8* ptr = DSi.NWRAMMap_B[2][(addr >> 15) & 0x7];
    if (ptr) *(u16*)&ptr[addr & 0x7FFF] = val;
}

u16 UcodeBase::AHBMRead16(u32 addr)
{
    return 0;
}

u16 UcodeBase::AHBMRead32(u32 addr)
{
    return 0;
}

void UcodeBase::AHBMWrite16(u32 addr, u16 val)
{
}

void UcodeBase::AHBMWrite32(u32 addr, u32 val)
{
}


u16 UcodeBase::GetSemaphore() const
{
    return SemaphoreOut;
}

void UcodeBase::SetSemaphore(u16 val)
{
    SemaphoreIn |= val;
}

void UcodeBase::ClearSemaphore(u16 val)
{
    SemaphoreOut &= ~val;
}

void UcodeBase::MaskSemaphore(u16 val)
{
    SemaphoreMask = val;
}

void UcodeBase::SetSemaphoreOut(u16 val)
{
    SemaphoreOut |= val;
    if (SemaphoreOut & (~SemaphoreMask))
        DSi.DSP.IrqSem();
}


u16* UcodeBase::GetDataMemPointer(u32 addr)
{
    // pointer only valid within 32K page
    u16* mem = (u16*)DSi.NWRAMMap_C[2][(addr >> 14) & 0x7];
    return &mem[addr & 0x3FFF];
}


void UcodeBase::Start()
{
    // initialize pipe structure
    u16* pipe = GetDataMemPointer(kPipeMonitorAddr);
    for (int i = 0; i < 16; i++)
    {
        *pipe++ = kPipeBufferAddr + (0x100 * i);    // buffer address
        *pipe++ = 0x200;                            // length in bytes
        *pipe++ = 0;                                // read pointer
        *pipe++ = 0;                                // write pointer
        *pipe++ = i;                                // pipe index
    }

    SendReply(0, 1);
    SendReply(1, 1);
    SendReply(2, 1);
    SetReplyReadCallback(2, 1, 0);
}


u16* UcodeBase::LoadPipe(u8 index)
{
    return GetDataMemPointer(kPipeMonitorAddr + (index * 5));
}

u32 UcodeBase::GetPipeLength(u16* pipe)
{
    u32 ret;
    u16 rdptr = pipe[2] & 0x7FFF;
    u16 wrptr = pipe[3] & 0x7FFF;
    if ((pipe[2] ^ pipe[3]) & 0x8000)
    {
        u16 len = pipe[1];
        ret = wrptr + len - rdptr;
    }
    else
    {
        ret = wrptr - rdptr;
    }

    if (ret & 1) printf("HUH? pipe length is odd? (%d)\n", ret);
    return ret >> 1;
}

u32 UcodeBase::ReadPipe(u16* pipe, u16* data, u32 len)
{
    u16* pipebuf = GetDataMemPointer(pipe[0]);
    u16 pipelen = pipe[1] >> 1;
    u16 rdptr = (pipe[2] & 0x7FFF) >> 1;
    u16 rdphase = pipe[2] & 0x8000;
    u16 wrptr = (pipe[3] & 0x7FFF) >> 1;

    u32 rdlen = 0;
    for (int i = 0; i < len; i++)
    {
        data[i] = pipebuf[rdptr++];
        rdlen++;
        if (rdptr >= pipelen)
        {
            rdptr = 0;
            rdphase ^= 0x8000;
        }
        if (rdptr == wrptr) break;
    }

    pipe[2] = (rdptr << 1) | rdphase;
    SendReply(2, pipe[4]);
    SetSemaphoreOut(0x8000);

    return rdlen;
}

u32 UcodeBase::WritePipe(u16* pipe, const u16* data, u32 len)
{
    u16* pipebuf = GetDataMemPointer(pipe[0]);
    u16 pipelen = pipe[1] >> 1;
    u16 rdptr = (pipe[2] & 0x7FFF) >> 1;
    u16 wrptr = (pipe[3] & 0x7FFF) >> 1;
    u16 wrphase = pipe[3] & 0x8000;

    u32 wrlen = 0;
    for (int i = 0; i < len; i++)
    {
        pipebuf[wrptr++] = data[i];
        wrlen++;
        if (wrptr >= pipelen)
        {
            wrptr = 0;
            wrphase ^= 0x8000;
        }
        if (wrptr == rdptr)
        {
            Log(LogLevel::Error, "DSP_HLE: PIPE %d IS FULL!!\n", pipe[4]);
            break;
        }
    }

    pipe[3] = (wrptr << 1) | wrphase;
    SendReply(2, pipe[4]);
    SetSemaphoreOut(0x8000);

    return wrlen;
}


// TODO: those could be accelerated eventually?
// by providing a way to read/write blocks of memory in NDS
// rather than having to decode the address for every word

void UcodeBase::ReadARM9Mem(u16* mem, u32 addr, u32 len)
{
    if (addr & 2)
    {
        *mem = DSi.ARM9Read16(addr);
        mem++;
        addr += 2;
        len -= 2;
    }
    while (len >= 4)
    {
        *(u32*)mem = DSi.ARM9Read32(addr);
        mem += 2;
        addr += 4;
        len -= 4;
    }
    if (len)
    {
        *mem = DSi.ARM9Read16(addr);
        len -= 2;
    }
}

void UcodeBase::WriteARM9Mem(const u16* mem, u32 addr, u32 len)
{
    if (addr & 2)
    {
        DSi.ARM9Write16(addr, *mem);
        mem++;
        addr += 2;
        len -= 2;
    }
    while (len >= 4)
    {
        DSi.ARM9Write32(addr, *(u32*)mem);
        mem += 2;
        addr += 4;
        len -= 4;
    }
    if (len)
    {
        DSi.ARM9Write16(addr, *mem);
        len -= 2;
    }
}


void UcodeBase::TryStartAudioCmd()
{
    u16* pipe = LoadPipe(5);
    u32 pipelen = GetPipeLength(pipe);
    if (pipelen < 8) return;

    u16 cmdparams[8];
    ReadPipe(pipe, cmdparams, 8);
    u32 cmd = (cmdparams[0] << 16) | cmdparams[1];
    u32 addr = (cmdparams[2] << 16) | cmdparams[3];
    u32 len = (cmdparams[4] << 16) | cmdparams[5];

    u32 cmdtype = (cmd >> 12) & 0xF;
    u32 cmdaction = (cmd >> 8) & 0xF;
    if ((cmdtype == 1) && (cmdaction == 1))
    {
        // play sound

        AudioOutHalve = !!(cmd & (1<<1));
        AudioOutAddr = addr;
        AudioOutLength = len;
        AudioPlaying = true;

        if (AudioOutFIFO.IsEmpty())
            AudioOutAdvance();
    }
    else if (cmdtype == 2)
    {
        if (cmdaction == 1)
        {
            // start mic sampling

            MicSampling = true;
            MicInFIFO.Clear();
            DSi.Mic.Start(Mic_DSi_DSP);
        }
        else if (cmdaction == 2)
        {
            // stop mic sampling

            DSi.Mic.Stop(Mic_DSi_DSP);
            MicSampling = false;
        }

        if ((cmdaction == 1) || (cmdaction == 2))
        {
            // initialize mic buffer
            u16* micbuf = GetDataMemPointer(kMicBufferAddr);
            *micbuf++ = kMicBufferAddr + 3; // pointer to mic buffer
            *micbuf++ = 0x1000;             // buffer length
            *micbuf++ = 0;                  // write pointer
            for (int i = 0; i < 0x1000; i++)
                *micbuf++ = 0;
        }

        // send response to tell the ARM9 where the mic buffer is
        SetReplyReadCallback(2, 3, cmd);
    }
}

void UcodeBase::AudioOutAdvance()
{
    while (!AudioOutFIFO.IsFull())
    {
        s16 sample = (s16)DSi.ARM9Read16(AudioOutAddr);

        // halve bit isn't supported by early AAC ucode
        if (AudioOutHalve && (UcodeVersion != -1))
        {
            sample += (sample >> 15);
            sample >>= 1;
        }

        // sounds are always mono, so each sample is duplicated to make it stereo
        AudioOutFIFO.Write(sample);
        AudioOutFIFO.Write(sample);

        AudioOutAddr += 2;
        AudioOutLength--;
        if (!AudioOutLength)
        {
            AudioPlaying = false;

            // send completion message
            SetReplyReadCallback(2, 2, AudioOutLength);

            break;
        }
    }
}

void UcodeBase::MicInAdvance()
{
    u16* micbuf = GetDataMemPointer(kMicBufferAddr);
    u16 buflen = micbuf[1];
    u16 wrpos = micbuf[2];
    u16* micdata = &micbuf[3];

    while (!MicInFIFO.IsEmpty())
    {
        // mic input is written to a circular buffer in DSP RAM

        s16 val = MicInFIFO.Read();
        micdata[wrpos & 0x3FFF] = val;

        wrpos++;
        if (wrpos >= buflen)
            wrpos = 0;
    }

    micbuf[2] = wrpos;
}


void UcodeBase::SampleClock(s16 output[2], s16 input)
{
    if (MicSampling && (!MicInFIFO.IsFull()))
    {
        MicInFIFO.Write(input);
        if (MicInFIFO.IsFull())
            MicInAdvance();
    }

    if (AudioOutFIFO.IsEmpty() && AudioPlaying)
        AudioOutAdvance();

    if (AudioOutFIFO.IsEmpty())
    {
        output[0] = 0;
        output[1] = 0;
    }
    else
    {
        output[0] = AudioOutFIFO.Read();
        output[1] = AudioOutFIFO.Read();
    }
}


}
}
