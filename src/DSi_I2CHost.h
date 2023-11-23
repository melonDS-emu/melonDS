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

#ifndef DSI_I2CHOST_H
#define DSI_I2CHOST_H

#include "DSi_I2C.h"
#include "DSi_Camera.h"

namespace melonDS
{
class DSi_I2CHost
{
public:
    explicit DSi_I2CHost(melonDS::DSi& dsi);
    void Reset();
    void DoSavestate(Savestate* file);

    [[nodiscard]] u8 ReadCnt() const noexcept { return Cnt; }
    void WriteCnt(u8 val);

    [[nodiscard]] u8 ReadData() const noexcept { return Data; }
    void WriteData(u8 val);

    DSi_BPTWL BPTWL;       // 4A / BPTWL IC
    DSi_Camera Camera0;    // 78 / facing outside
    DSi_Camera Camera1;    // 7A / selfie cam
private:
    melonDS::DSi& DSi;
    u8 Cnt = 0;
    u8 Data = 0;
    u8 CurDeviceID = 0;
    DSi_I2CDevice* CurDevice = nullptr;

    void GetCurDevice();
};
}

#endif //DSI_I2CHOST_H
