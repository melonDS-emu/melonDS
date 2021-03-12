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

#ifndef DSI_CAMERA_H
#define DSI_CAMERA_H

#include "types.h"

class DSi_Camera
{
public:
    static bool Init();
    static void DeInit();
    static void Reset();

    static void IRQ(u32 param);
    static void RequestFrame(u32 cam);

    static void Transfer(u32 pos);

    DSi_Camera(u32 num);
    ~DSi_Camera();

    void ResetCam();
    bool IsActivated();

    void I2C_Start();
    u8 I2C_Read(bool last);
    void I2C_Write(u8 val, bool last);

    static u8 Read8(u32 addr);
    static u16 Read16(u32 addr);
    static u32 Read32(u32 addr);
    static void Write8(u32 addr, u8 val);
    static void Write16(u32 addr, u16 val);
    static void Write32(u32 addr, u32 val);

    u32 Num;

private:
    u32 DataPos;
    u32 RegAddr;
    u16 RegData;

    u16 I2C_ReadReg(u16 addr);
    void I2C_WriteReg(u16 addr, u16 val);

    u16 PLLDiv;
    u16 PLLPDiv;
    u16 PLLCnt;
    u16 ClocksCnt;
    u16 StandbyCnt;
    u16 MiscCnt;

    u16 MCUAddr;
    u16* MCUData;

    u8 MCURegs[0x8000];

    static u16 ModuleCnt;
    static u16 Cnt;

    static u8 FrameBuffer[640*480*4];
    static u32 TransferPos;
    static u32 FrameLength;
};


extern DSi_Camera* DSi_Camera0;
extern DSi_Camera* DSi_Camera1;

#endif // DSI_CAMERA_H
