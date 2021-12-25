/*
    Copyright 2016-2021 Arisotura

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

#include <stdio.h>
#include <string.h>

#include "SaveManager.h"
#include "Platform.h"


SaveManager::SaveManager()
{
    SecondaryBuffer = nullptr;
    SecondaryBufferLock = new QMutex();

    Running = false;
}

SaveManager::~SaveManager()
{
    if (Running)
    {
        Running = false;
        wait();
        FlushSecondaryBuffer();
    }

    if (SecondaryBuffer) delete[] SecondaryBuffer;

    delete SecondaryBufferLock;
}

void SaveManager::Setup(std::string path, u8* buffer, u32 length)
{
    // Flush SRAM in case there is unflushed data from previous state.
    FlushSecondaryBuffer();

    SecondaryBufferLock->lock();

    Path = path;

    Buffer = buffer;
    Length = length;

    if(SecondaryBuffer) delete[] SecondaryBuffer; // Delete secondary buffer, there might be previous state.

    SecondaryBuffer = new u8[length];
    SecondaryBufferLength = length;

    FlushVersion = 0;
    PreviousFlushVersion = 0;
    TimeAtLastFlushRequest = 0;

    SecondaryBufferLock->unlock();

    if ((!path.empty()) && (!Running))
    {
        Running = true;
        start();
    }
    else if (path.empty() && Running)
    {
        Running = false;
        wait();
    }
}

void SaveManager::RequestFlush()
{
    SecondaryBufferLock->lock();

    printf("SaveManager: Flush requested\n");
    memcpy(SecondaryBuffer, Buffer, Length);
    FlushVersion++;
    TimeAtLastFlushRequest = time(nullptr);

    SecondaryBufferLock->unlock();
}

void SaveManager::run()
{
    for (;;)
    {
        QThread::msleep(100);

        if (!Running) return;

        // We debounce for two seconds after last flush request to ensure that writing has finished.
        if (TimeAtLastFlushRequest == 0 || difftime(time(nullptr), TimeAtLastFlushRequest) < 2)
        {
            continue;
        }

        FlushSecondaryBuffer();
    }
}

void SaveManager::FlushSecondaryBuffer(u8* dst, u32 dstLength)
{
    // When flushing to a file, there's no point in re-writing the exact same data.
    if (!dst && !NeedsFlush()) return;
    // When flushing to memory, we don't know if dst already has any data so we only check that we CAN flush.
    if (dst && dstLength < SecondaryBufferLength) return;

    SecondaryBufferLock->lock();
    if (dst)
    {
        memcpy(dst, SecondaryBuffer, SecondaryBufferLength);
    }
    else
    {
        FILE* f = Platform::OpenFile(Path, "wb");
        if (f)
        {
            printf("SaveManager: Written\n");
            fwrite(SecondaryBuffer, SecondaryBufferLength, 1, f);
            fclose(f);
        }
    }
    PreviousFlushVersion = FlushVersion;
    TimeAtLastFlushRequest = 0;
    SecondaryBufferLock->unlock();
}

bool SaveManager::NeedsFlush()
{
    return FlushVersion != PreviousFlushVersion;
}

void SaveManager::UpdateBuffer(u8* src, u32 srcLength)
{
    if (!src || srcLength != Length) return;

    // should we create a lock for the primary buffer? this method is not intended to be called from a secondary thread in the way Flush is
    memcpy(Buffer, src, srcLength);
    SecondaryBufferLock->lock();
    memcpy(SecondaryBuffer, src, srcLength);
    SecondaryBufferLock->unlock();

    PreviousFlushVersion = FlushVersion;
}
