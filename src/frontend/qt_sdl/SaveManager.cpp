/*
    Copyright 2016-2022 melonDS team

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

using Platform::Log;
using Platform::LogLevel;

SaveManager::SaveManager(std::string path) : QThread()
{
    SecondaryBuffer = nullptr;
    SecondaryBufferLength = 0;
    SecondaryBufferLock = new QMutex();

    Running = false;

    Path = path;

    Buffer = nullptr;
    Length = 0;
    FlushRequested = false;

    FlushVersion = 0;
    PreviousFlushVersion = 0;
    TimeAtLastFlushRequest = 0;

    if (!path.empty())
    {
        Running = true;
        start();
    }
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

    if (Buffer) delete[] Buffer;
}

std::string SaveManager::GetPath()
{
    return Path;
}

void SaveManager::SetPath(std::string path, bool reload)
{
    Path = path;

    if (reload)
    {
        FILE* f = Platform::OpenFile(Path, "rb", true);
        if (f)
        {
            fread(Buffer, 1, Length, f);
            fclose(f);
        }
    }
    else
        FlushRequested = true;
}

void SaveManager::RequestFlush(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen)
{
    if (Length != savelen)
    {
        if (Buffer) delete[] Buffer;

        Length = savelen;
        Buffer = new u8[Length];

        memcpy(Buffer, savedata, Length);
    }
    else
    {
        if ((writeoffset+writelen) > savelen)
        {
            u32 len = savelen - writeoffset;
            memcpy(&Buffer[writeoffset], &savedata[writeoffset], len);
            len = writelen - len;
            if (len > savelen) len = savelen;
            memcpy(&Buffer[0], &savedata[0], len);
        }
        else
        {
            memcpy(&Buffer[writeoffset], &savedata[writeoffset], writelen);
        }
    }

    FlushRequested = true;
}

void SaveManager::CheckFlush()
{
    if (!FlushRequested) return;

    SecondaryBufferLock->lock();

    Log(LogLevel::Info, "SaveManager: Flush requested\n");

    if (SecondaryBufferLength != Length)
    {
        if (SecondaryBuffer) delete[] SecondaryBuffer;

        SecondaryBufferLength = Length;
        SecondaryBuffer = new u8[SecondaryBufferLength];
    }

    memcpy(SecondaryBuffer, Buffer, Length);

    FlushRequested = false;
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
    if (!SecondaryBuffer) return;

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
            Log(LogLevel::Info, "SaveManager: Written\n");
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
