/*
    Copyright 2016-2025 melonDS team

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

#ifndef MIC_H
#define MIC_H

#include "Savestate.h"
#include "Platform.h"

namespace melonDS
{
class NDS;

class Mic
{
public:
    explicit Mic(melonDS::NDS& nds);
    ~Mic();
    void Reset();
    void DoSavestate(Savestate* file);

private:
    melonDS::NDS& NDS;

    static const u32 InputBufferSize = 2*1024;
    s16 InputBuffer[2 * InputBufferSize] {};
    u32 InputBufferWritePos = 0;
    u32 InputBufferReadPos = 0;
};

}

#endif // MIC_H
