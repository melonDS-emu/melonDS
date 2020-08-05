/*
    Copyright 2016-2020 Arisotura

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

    DSi_Camera(u32 num);
    ~DSi_Camera();

    void ResetCam();

    void Start();
    u8 Read(bool last);
    void Write(u8 val, bool last);

private:
    u32 Num;

    u32 DataPos;
    u32 RegAddr;
    u16 RegData;

    u16 ReadReg(u16 addr);
    void WriteReg(u16 addr, u16 val);

    u16 PLLCnt;
    u16 StandbyCnt;
};


extern DSi_Camera* DSi_Camera0;
extern DSi_Camera* DSi_Camera1;

#endif // DSI_CAMERA_H
