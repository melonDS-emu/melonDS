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

#ifndef DSI_SPI_TSC
#define DSI_SPI_TSC

#include "types.h"
#include "Savestate.h"

namespace DSi_SPI_TSC
{

extern u32 DataPos;

bool Init();
void DeInit();
void Reset();
void DoSavestate(Savestate* file);

// 00=DS-mode 01=normal
void SetMode(u8 mode);

void SetTouchCoords(u16 x, u16 y);
void MicInputFrame(s16* data, int samples);

u8 Read();
void Write(u8 val, u32 hold);

}

#endif // DSI_SPI_TSC
