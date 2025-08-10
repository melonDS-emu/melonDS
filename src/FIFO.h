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
        if (IsEmpty())
            return Entries[((ReadPos==0) ? NumEntries : ReadPos) - 1];

        T ret = Entries[ReadPos];

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

template<u32 Size>
class RingBuffer
{
public:
    void Clear()
    {
        NumOccupied = 0;
        ReadPos = 0;
        WritePos = 0;
        memset(Buffer, 0, Size);
    }


    void DoSavestate(Savestate* file)
    {
        file->Var32(&NumOccupied);
        file->Var32(&ReadPos);
        file->Var32(&WritePos);

        file->VarArray(Buffer, Size);
    }


    bool Write(const void* data, u32 len)
    {
        if (!CanFit(len)) return false;

        if ((WritePos + len) >= Size)
        {
            u32 part1 = Size - WritePos;
            memcpy(&Buffer[WritePos], data, part1);
            if (len > part1)
                memcpy(Buffer, &((u8*)data)[part1], len - part1);
            WritePos = len - part1;
        }
        else
        {
            memcpy(&Buffer[WritePos], data, len);
            WritePos += len;
        }

        NumOccupied += len;

        return true;
    }

    bool Read(void* data, u32 len)
    {
        if (NumOccupied < len) return false;

        u32 readpos = ReadPos;
        if ((readpos + len) >= Size)
        {
            u32 part1 = Size - readpos;
            memcpy(data, &Buffer[readpos], part1);
            if (len > part1)
                memcpy(&((u8*)data)[part1], Buffer, len - part1);
            ReadPos = len - part1;
        }
        else
        {
            memcpy(data, &Buffer[readpos], len);
            ReadPos += len;
        }

        NumOccupied -= len;
        return true;
    }

    bool Peek(void* data, u32 offset, u32 len)
    {
        if (NumOccupied < len) return false;

        u32 readpos = ReadPos + offset;
        if (readpos >= Size) readpos -= Size;

        if ((readpos + len) >= Size)
        {
            u32 part1 = Size - readpos;
            memcpy(data, &Buffer[readpos], part1);
            if (len > part1)
                memcpy(&((u8*)data)[part1], Buffer, len - part1);
        }
        else
        {
            memcpy(data, &Buffer[readpos], len);
        }

        return true;
    }

    bool Skip(u32 len)
    {
        if (NumOccupied < len) return false;

        ReadPos += len;
        if (ReadPos >= Size)
            ReadPos -= Size;

        NumOccupied -= len;
        return true;
    }

    u32 Level() const { return NumOccupied; }
    bool IsEmpty() const { return NumOccupied == 0; }
    bool IsFull() const { return NumOccupied >= Size; }

    bool CanFit(u32 num) const { return ((NumOccupied + num) <= Size); }

private:
    u8 Buffer[Size] = {0};
    u32 NumOccupied = 0;
    u32 ReadPos = 0, WritePos = 0;
};

}

#endif
