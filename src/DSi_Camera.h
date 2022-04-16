/*
    Copyright 2016-2022 melonDS team

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

namespace DSi_CamModule
{

class Camera;

extern Camera* Camera0;
extern Camera* Camera1;

bool Init();
void DeInit();
void Reset();

void DoSavestate(Savestate* file);

void IRQ(u32 param);
void RequestFrame(u32 cam);

void Transfer(u32 pos);

u8 Read8(u32 addr);
u16 Read16(u32 addr);
u32 Read32(u32 addr);
void Write8(u32 addr, u8 val);
void Write16(u32 addr, u16 val);
void Write32(u32 addr, u32 val);

class Camera
{
public:
    Camera(u32 num);
    ~Camera();

    void DoSavestate(Savestate* file);

    void Reset();
    bool IsActivated();

    void I2C_Start();
    u8 I2C_Read(bool last);
    void I2C_Write(u8 val, bool last);

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

    u8 MCU_Read(u16 addr);
    void MCU_Write(u16 addr, u8 val);
};

}

#endif // DSI_CAMERA_H
