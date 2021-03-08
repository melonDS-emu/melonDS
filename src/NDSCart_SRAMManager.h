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

#ifndef NDSCART_SRAMMANAGER_H
#define NDSCART_SRAMMANAGER_H

#include "types.h"

namespace NDSCart_SRAMManager
{
    extern u32 SecondaryBufferLength;

    bool Init();
    void DeInit();

    void Setup(const char* path, u8* buffer, u32 length);
    void RequestFlush();

    bool NeedsFlush();
    void FlushSecondaryBuffer(u8* dst = NULL, s32 dstLength = 0);
    void UpdateBuffer(u8* src, s32 srcLength);
}

#endif // NDSCART_SRAMMANAGER_H