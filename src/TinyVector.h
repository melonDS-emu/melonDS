/*
    Copyright 2016-2023 melonDS team, RSDuck

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

#ifndef MELONDS_TINYVECTOR_H
#define MELONDS_TINYVECTOR_H

#include <assert.h>
#include <string.h>
#include "types.h"

namespace melonDS
{
/*
    TinyVector
        - because reinventing the wheel is the best!

    - meant to be used very often, with not so many elements
    max 1 << 16 elements
    - doesn't allocate while no elements are inserted
    - not stl confirmant of course
    - probably only works with POD types
    - remove operations don't preserve order, but O(1)!
*/
template<typename T>
struct __attribute__((packed)) TinyVector
{
    T* Data = NULL;
    u16 Capacity = 0;
    u16 Length = 0;

    ~TinyVector()
    {
        delete[] Data;
    }

    void MakeCapacity(u32 capacity)
    {
        assert(capacity <= UINT16_MAX);
        assert(capacity > Capacity);
        T* newMem = new T[capacity];
        if (Data != NULL)
            memcpy(newMem, Data, sizeof(T) * Length);

        T* oldData = Data;
        Data = newMem;
        if (oldData != NULL)
            delete[] oldData;

        Capacity = capacity;
    }

    void SetLength(u16 length)
    {
        if (Capacity < length)
            MakeCapacity(length);

        Length = length;
    }

    void Clear()
    {
        Length = 0;
    }

    void Add(T element)
    {
        assert(Length + 1 <= UINT16_MAX);
        if (Length + 1 > Capacity)
            MakeCapacity(((Capacity + 4) * 3) / 2);

        Data[Length++] = element;
    }

    void Remove(int index)
    {
        assert(Length > 0);
        assert(index >= 0 && index < Length);

        Length--;
        Data[index] = Data[Length];
        /*for (int i = index; i < Length; i++)
            Data[i] = Data[i + 1];*/
    }

    int Find(T needle) const
    {
        for (int i = 0; i < Length; i++)
        {
            if (Data[i] == needle)
                return i;
        }
        return -1;
    }

    bool RemoveByValue(T needle)
    {
        for (int i = 0; i < Length; i++)
        {
            if (Data[i] == needle)
            {
                Remove(i);
                return true;
            }
        }
        return false;
    }

    T& operator[](int index)
    {
        assert(index >= 0 && index < Length);
        return Data[index];
    }

    const T& operator[](int index) const
    {
        assert(index >= 0 && index < Length);
        return Data[index];
    }
};
}

#endif //MELONDS_TINYVECTOR_H
