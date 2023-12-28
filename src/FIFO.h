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

#ifndef FIFO_H
#define FIFO_H

#include "types.h"
#include "Savestate.h"

namespace melonDS
{
template<typename T, u32 NumEntries>
class FIFO
{
public:
    void Clear()
    {
        NumOccupied = 0;
        ReadPos = 0;
        WritePos = 0;
        memset(&Entries[ReadPos], 0, sizeof(T));
    }


    void DoSavestate(Savestate* file)
    {
        file->Var32(&NumOccupied);
        file->Var32(&ReadPos);
        file->Var32(&WritePos);

        file->VarArray(Entries, sizeof(T)*NumEntries);
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

    T Peek() const
    {
        return Entries[ReadPos];
    }

    T Peek(u32 offset) const
    {
        u32 pos = ReadPos + offset;
        if (pos >= NumEntries)
            pos -= NumEntries;

        return Entries[pos];
    }

    u32 Level() const { return NumOccupied; }
    bool IsEmpty() const { return NumOccupied == 0; }
    bool IsFull() const { return NumOccupied >= NumEntries; }

    bool CanFit(u32 num) const { return ((NumOccupied + num) <= NumEntries); }

private:
    T Entries[NumEntries] = {0};
    u32 NumOccupied = 0;
    u32 ReadPos = 0, WritePos = 0;
};


template<typename T>
class DynamicFIFO
{
public:
    DynamicFIFO(u32 num)
    {
        NumEntries = num;
        Entries = new T[num];
        Clear();
    }

    ~DynamicFIFO()
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


    void DoSavestate(Savestate* file)
    {
        file->Var32(&NumOccupied);
        file->Var32(&ReadPos);
        file->Var32(&WritePos);

        file->VarArray(Entries, sizeof(T)*NumEntries);
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

    T Peek() const
    {
        return Entries[ReadPos];
    }

    T Peek(u32 offset) const
    {
        u32 pos = ReadPos + offset;
        if (pos >= NumEntries)
            pos -= NumEntries;

        return Entries[pos];
    }

    u32 Level() const { return NumOccupied; }
    bool IsEmpty() const { return NumOccupied == 0; }
    bool IsFull() const { return NumOccupied >= NumEntries; }

    bool CanFit(u32 num) const { return ((NumOccupied + num) <= NumEntries); }

private:
    u32 NumEntries;
    T* Entries;
    u32 NumOccupied;
    u32 ReadPos, WritePos;
};

}
#endif
