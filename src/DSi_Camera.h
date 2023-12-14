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
class DSi;
class DSi_CamModule;

class DSi_Camera : public DSi_I2CDevice
{
public:
    DSi_Camera(melonDS::DSi& dsi, DSi_I2CHost* host, u32 num);
    ~DSi_Camera();

    void DoSavestate(Savestate* file) override;

    void Reset() override;
    void Stop();
    bool IsActivated() const;

    void StartTransfer();
    bool TransferDone() const;

    // lengths in words
    int TransferScanline(u32* buffer, int maxlen);

    void Acquire() override;
    u8 Read(bool last) override;
    void Write(u8 val, bool last) override;

    void InputFrame(const u32* data, int width, int height, bool rgb);

    u32 Num;

private:
    u32 DataPos;
    u32 RegAddr;
    u16 RegData;

    u16 I2C_ReadReg(u16 addr) const;
    void I2C_WriteReg(u16 addr, u16 val);

    u16 PLLDiv;
    u16 PLLPDiv;
    u16 PLLCnt;
    u16 ClocksCnt;
    u16 StandbyCnt;
    u16 MiscCnt;

    u16 MCUAddr;
    u8 MCURegs[0x8000];

    u8 MCU_Read(u16 addr) const;
    void MCU_Write(u16 addr, u8 val);

    u16 FrameWidth, FrameHeight;
    u16 FrameReadMode, FrameFormat;
    int TransferY;
    u32 FrameBuffer[640*480/2]; // YUYV framebuffer, two pixels per word
};


class DSi_CamModule
{
public:
    DSi_CamModule(melonDS::DSi& dsi);
    ~DSi_CamModule();
    void Reset();
    void Stop();
    void DoSavestate(Savestate* file);

    const DSi_Camera* GetOuterCamera() const { return Camera0; }
    DSi_Camera* GetOuterCamera() { return Camera0; }
    const DSi_Camera* GetInnerCamera() const { return Camera1; }
    DSi_Camera* GetInnerCamera() { return Camera1; }

    void IRQ(u32 param);

    void TransferScanline(u32 line);

    u8 Read8(u32 addr);
    u16 Read16(u32 addr);
    u32 Read32(u32 addr);
    void Write8(u32 addr, u8 val);
    void Write16(u32 addr, u16 val);
    void Write32(u32 addr, u32 val);

private:
    melonDS::DSi& DSi;
    DSi_Camera* Camera0; // 78 / facing outside
    DSi_Camera* Camera1; // 7A / selfie cam

    u16 ModuleCnt;
    u16 Cnt;

    u32 CropStart, CropEnd;

    // pixel data buffer holds a maximum of 512 words, regardless of how long scanlines are
    u32 DataBuffer[512];
    u32 BufferReadPos, BufferWritePos;
    u32 BufferNumLines;
    DSi_Camera* CurCamera;

    static const u32 kIRQInterval;
    static const u32 kTransferStart;
};

}
#endif // DSI_CAMERA_H
