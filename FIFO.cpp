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

#include "FIFO.h"


FIFO::FIFO(u32 num)
{
    NumEntries = num;
    Entries = new u32[num];
    Clear();
}

FIFO::~FIFO()
{
    delete[] Entries;
}

void FIFO::Clear()
{
    NumOccupied = 0;
    ReadPos = 0;
    WritePos = 0;
    Entries[ReadPos] = 0;
}

void FIFO::Write(u32 val)
{
    if (IsFull()) return;

    Entries[WritePos] = val;

    WritePos++;
    if (WritePos >= NumEntries)
        WritePos = 0;

    NumOccupied++;
}

u32 FIFO::Read()
{
    u32 ret = Entries[ReadPos];
    if (IsEmpty())
        return ret;

    ReadPos++;
    if (ReadPos >= NumEntries)
        ReadPos = 0;

    NumOccupied--;
    return ret;
}

u32 FIFO::Peek()
{
    return Entries[ReadPos];
}
