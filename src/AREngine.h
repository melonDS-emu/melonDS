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

#ifndef ARENGINE_H
#define ARENGINE_H

#include "ARCodeFile.h"

namespace melonDS
{
class AREngine
{
public:
    AREngine();
    ~AREngine();
    void Reset();

    ARCodeFile* GetCodeFile() { return CodeFile; }
    void SetCodeFile(ARCodeFile* file) { CodeFile = file; }

    void RunCheats();
    void RunCheat(ARCode& arcode);
private:
    ARCodeFile* CodeFile; // AR code file - frontend is responsible for managing this

    // TEMPORARY
    u8 (*BusRead8)(u32 addr);
    u16 (*BusRead16)(u32 addr);
    u32 (*BusRead32)(u32 addr);
    void (*BusWrite8)(u32 addr, u8 val);
    void (*BusWrite16)(u32 addr, u16 val);
    void (*BusWrite32)(u32 addr, u32 val);
};

}
#endif // ARENGINE_H
