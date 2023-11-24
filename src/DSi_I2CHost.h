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

#ifndef MELONDS_DSI_I2CHOST_H
#define MELONDS_DSI_I2CHOST_H

#include "types.h"

class Savestate;
class DSi_BPTWL;
class DSi_Camera;
class DSi_I2CDevice;

class DSi_I2CHost
{
public:
    DSi_I2CHost();
    ~DSi_I2CHost();
    void Reset();
    void DoSavestate(Savestate* file);

    DSi_BPTWL* GetBPTWL() { return BPTWL; }
    DSi_Camera* GetOuterCamera() { return Camera0; }
    DSi_Camera* GetInnerCamera() { return Camera1; }

    u8 ReadCnt() { return Cnt; }
    void WriteCnt(u8 val);

    u8 ReadData();
    void WriteData(u8 val);

private:
    u8 Cnt;
    u8 Data;

    DSi_BPTWL* BPTWL;       // 4A / BPTWL IC
    DSi_Camera* Camera0;    // 78 / facing outside
    DSi_Camera* Camera1;    // 7A / selfie cam

    u8 CurDeviceID;
    DSi_I2CDevice* CurDevice;

    void GetCurDevice();
};

#endif //MELONDS_DSI_I2CHOST_H
