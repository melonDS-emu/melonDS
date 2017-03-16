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

template<typename T>
class FIFO
{
public:
    FIFO(u32 num)
    {
        NumEntries = num;
        Entries = new T[num];
        Clear();
    }

    ~FIFO()
    {
        delete[] Entries;
    }


    void Clear()
    {
        NumOccupied = 0;
        ReadPos = 0;
        WritePos = 0;
        memset(&Entries[ReadPos], 0, sizeof(T));
    }


    void Write(T val)
    {
        if (IsFull()) return;

        Entries[WritePos] = val;

        WritePos++;
        if (WritePos >= NumEntries)
            WritePos = 0;

        NumOccupied++;
    }

    T Read()
    {
        T ret = Entries[ReadPos];
        if (IsEmpty())
            return ret;

        ReadPos++;
        if (ReadPos >= NumEntries)
            ReadPos = 0;

        NumOccupied--;
        return ret;
    }

    T Peek()
    {
        return Entries[ReadPos];
    }

    u32 Level() { return NumOccupied; }
    bool IsEmpty() { return NumOccupied == 0; }
    bool IsFull() { return NumOccupied >= NumEntries; }

private:
    u32 NumEntries;
    T* Entries;
    u32 NumOccupied;
    u32 ReadPos, WritePos;
};

#endif
