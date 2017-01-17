/*
    Copyright 2016-2017 StapleButter

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

#ifndef FIFO_H
#define FIFO_H

#include "types.h"

class FIFO
{
public:
    FIFO(u32 num);
    ~FIFO();

    void Clear();

    void Write(u32 val);
    u32 Read();
    u32 Peek();

    u32 Level() { return NumOccupied; }
    bool IsEmpty() { return NumOccupied == 0; }
    bool IsFull() { return NumOccupied >= NumEntries; }

private:
    u32 NumEntries;
    u32* Entries;
    u32 NumOccupied;
    u32 ReadPos, WritePos;
};

#endif
