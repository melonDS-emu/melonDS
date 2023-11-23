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

#ifndef DSI_CAMERA_H
#define DSI_CAMERA_H

#include "types.h"
#include "Savestate.h"
#include "DSi_I2C.h"

namespace melonDS
{
class DSi_CamModule;

class DSi_Camera : public DSi_I2CDevice
{
public:
    DSi_Camera(melonDS::DSi& dsi, DSi_I2CHost* host, u32 num);

    void DoSavestate(Savestate* file) override;

    void Reset() override;
    void Stop();
    [[nodiscard]] bool IsActivated() const noexcept;

    void StartTransfer();
    [[nodiscard]] bool TransferDone() const noexcept;

    // lengths in words
    int TransferScanline(u32* buffer, int maxlen);

    void Acquire() override;
    u8 Read(bool last) override;
    void Write(u8 val, bool last) override;

    void InputFrame(const u32* data, int width, int height, bool rgb);

private:
    u32 Num;
    u32 DataPos = 0;
    u32 RegAddr = 0;
    u16 RegData = 0;

    [[nodiscard]] u16 I2C_ReadReg(u16 addr) const noexcept;
    void I2C_WriteReg(u16 addr, u16 val);

    u16 PLLDiv = 0x0366;
    u16 PLLPDiv = 0x00F5;
    u16 PLLCnt = 0x21F9;
    u16 ClocksCnt = 0;
    u16 StandbyCnt = 0x4029; // checkme
    u16 MiscCnt = 0;

    u16 MCUAddr = 0;
    u8 MCURegs[0x8000] {};

    [[nodiscard]] u8 MCU_Read(u16 addr) const noexcept;
    void MCU_Write(u16 addr, u8 val);

    u16 FrameWidth = 0, FrameHeight = 0;
    u16 FrameReadMode = 0, FrameFormat = 0;
    int TransferY = 0;
    u32 FrameBuffer[640*480/2] {}; // YUYV framebuffer, two pixels per word
};


class DSi_CamModule
{
public:
    DSi_CamModule(melonDS::DSi& dsi);
    ~DSi_CamModule();
    void Reset();
    void Stop();
    void DoSavestate(Savestate* file);


    u8 Read8(u32 addr);
    u16 Read16(u32 addr);
    u32 Read32(u32 addr);
    void Write8(u32 addr, u8 val);
    void Write16(u32 addr, u16 val);
    void Write32(u32 addr, u32 val);

private:
    void IRQ(u32 param);
    void TransferScanline(u32 line);

    melonDS::DSi& DSi;
    DSi_Camera* Camera0; // 78 / facing outside
    DSi_Camera* Camera1; // 7A / selfie cam

    u16 ModuleCnt = 0;
    u16 Cnt = 0;

    u32 CropStart = 0, CropEnd = 0;

    // pixel data buffer holds a maximum of 512 words, regardless of how long scanlines are
    u32 DataBuffer[512] {};
    u32 BufferReadPos = 0, BufferWritePos = 0;
    u32 BufferNumLines = 0;
    DSi_Camera* CurCamera = nullptr;

    static const u32 kIRQInterval;
    static const u32 kTransferStart;
};

}
#endif // DSI_CAMERA_H
