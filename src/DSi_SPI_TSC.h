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

#ifndef DSI_SPI_TSC
#define DSI_SPI_TSC

#include "types.h"
#include "Savestate.h"
#include "SPI.h"

namespace melonDS
{
class DSi;
class DSi_TSC : public TSC
{
public:
    DSi_TSC(melonDS::DSi& dsi);
    ~DSi_TSC() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    // 00=DS-mode 01=normal
    void SetMode(u8 mode);

    void SetTouchCoords(u16 x, u16 y) override;
    void MicInputFrame(const s16* data, int samples) override;

    void Write(u8 val) override;
    void Release() override;

private:
    u8 Index;
    u8 Bank;

    u8 Bank3Regs[0x80];
    u8 TSCMode;
};

}
#endif // DSI_SPI_TSC
