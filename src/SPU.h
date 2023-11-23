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

#ifndef SPU_H
#define SPU_H

#include "Savestate.h"
#include "Platform.h"

namespace melonDS
{
class NDS;
class SPU;

enum class AudioBitDepth
{
    _10Bit,
    _16Bit,
};

enum class Interpolation
{
    None,
    Linear,
    Cosine,
    Cubic,
};

class SPUChannel
{
public:
    SPUChannel(u32 num, melonDS::NDS& nds);
    ~SPUChannel();
    void Reset();
    void DoSavestate(Savestate* file);

    void SetSrcAddr(u32 val) { SrcAddr = val & 0x07FFFFFC; }
    void SetTimerReload(u32 val) { TimerReload = val & 0xFFFF; }
    void SetLoopPos(u32 val) { LoopPos = (val & 0xFFFF) << 2; }
    void SetLength(u32 val) { Length = (val & 0x001FFFFF) << 2; }
    void SetInterpolation(Interpolation type) noexcept { InterpType = type; }

    void SetCnt(u32 val);
    [[nodiscard]] u32 GetCnt() const noexcept { return Cnt; }
    [[nodiscard]] u8 GetPan() const noexcept { return Pan; }
    [[nodiscard]] u32 GetLength() const noexcept { return Length; }
    void Start();


    s32 DoRun();

    void PanOutput(s32 in, s32& left, s32& right) const noexcept;

private:
    static const s8 ADPCMIndexTable[8];
    static const u16 ADPCMTable[89];
    static const s16 PSGTable[8][8];

    // audio interpolation is an improvement upon the original hardware
    // (which performs no interpolation)
    Interpolation InterpType = Interpolation::None;

    u32 Num;

    u32 Cnt = 0;
    u32 SrcAddr = 0;
    u16 TimerReload = 0;
    u32 LoopPos = 0;
    u32 Length = 0;

    u8 Volume = 0;
    u8 VolumeShift = 0;
    u8 Pan = 0;

    bool KeyOn = false;
    u32 Timer = 0;
    s32 Pos = 0;
    std::array<s16, 3> PrevSample {};
    s16 CurSample = 0;
    u16 NoiseVal = 0;

    s32 ADPCMVal = 0;
    s32 ADPCMIndex = 0;
    s32 ADPCMValLoop = 0;
    s32 ADPCMIndexLoop = 0;
    u8 ADPCMCurByte = 0;

    std::array<u32, 8> FIFO {};
    u32 FIFOReadPos = 0;
    u32 FIFOWritePos = 0;
    u32 FIFOReadOffset = 0;
    u32 FIFOLevel = 0;

    void FIFO_BufferData();
    template<typename T> T FIFO_ReadData();

    template<u32 type> s32 Run();
    void NextSample_PCM8();
    void NextSample_PCM16();
    void NextSample_ADPCM();
    void NextSample_PSG();
    void NextSample_Noise();

    melonDS::NDS& NDS;
};

class SPUCaptureUnit
{
public:
    SPUCaptureUnit(u32 num, melonDS::NDS&);
    void Reset();
    void DoSavestate(Savestate* file);
    void Run(s32 sample);

    [[nodiscard]] u8 GetCnt() const noexcept { return Cnt; }
    void SetCnt(u8 val)
    {
        if ((val & 0x80) && !(Cnt & 0x80))
            Start();

        val &= 0x8F;
        if (!(val & 0x80)) val &= ~0x01;
        Cnt = val;
    }

    [[nodiscard]] u32 GetDstAddr() const noexcept { return DstAddr; }
    void SetDstAddr(u32 val) { DstAddr = val & 0x07FFFFFC; }

    void SetTimerReload(u32 val) { TimerReload = val & 0xFFFF; }
    void SetLength(u32 val) { Length = val << 2; if (Length == 0) Length = 4; }

private:
    melonDS::NDS& NDS;
    u32 Num;

    u8 Cnt = 0;
    u32 DstAddr = 0;
    u16 TimerReload = 0;
    u32 Length = 0;

    u32 Timer = 0;
    s32 Pos = 0;

    std::array<u32, 4> FIFO {};
    u32 FIFOReadPos = 0;
    u32 FIFOWritePos = 0;
    u32 FIFOWriteOffset = 0;
    u32 FIFOLevel = 0;

    void FIFO_FlushData();
    template<typename T> void FIFO_WriteData(T val);


    void Start()
    {
        Timer = TimerReload;
        Pos = 0;
        FIFOReadPos = 0;
        FIFOWritePos = 0;
        FIFOWriteOffset = 0;
        FIFOLevel = 0;
    }

};

class SPU
{
public:
    explicit SPU(melonDS::NDS& nds) noexcept;
    ~SPU();
    void Reset();
    void DoSavestate(Savestate* file);

    void Stop();

    void SetPowerCnt(u32 val);

    // 0=none 1=linear 2=cosine 3=cubic
    void SetInterpolation(Interpolation type);

    void SetBias(u16 bias);
    void SetDegrade10Bit(bool enable);
    void SetApplyBias(bool enable);

    void Mix(u32 dummy);

    int GetOutputSize();
    int ReadOutput(s16* data, int samples);
    void TransferOutput();

    [[nodiscard]] u8 Read8(u32 addr) const;
    [[nodiscard]] u16 Read16(u32 addr) const;
    [[nodiscard]] u32 Read32(u32 addr) const;
    void Write8(u32 addr, u8 val);
    void Write16(u32 addr, u16 val);
    void Write32(u32 addr, u32 val);

private:
    static constexpr u32 OutputBufferSize = 2*2048;
    void InitOutput();
    void TrimOutput();
    void DrainOutput();
    void Sync(bool wait);

    melonDS::NDS& NDS;
    s16 OutputBackbuffer[2 * OutputBufferSize] {};
    u32 OutputBackbufferWritePosition = 0;

    s16 OutputFrontBuffer[2 * OutputBufferSize] {};
    u32 OutputFrontBufferWritePosition = 0;
    u32 OutputFrontBufferReadPosition = 0;

    Platform::Mutex* AudioLock;

    u16 Cnt = 0;
    u8 MasterVolume = 0;
    u16 Bias = 0;
    bool ApplyBias = true;
    bool Degrade10Bit = false;

    std::array<SPUChannel, 16> Channels;
    std::array<SPUCaptureUnit, 2> Capture;
};

}
#endif // SPU_H
