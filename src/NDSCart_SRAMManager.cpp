/*
    Copyright 2016-2020 Arisotura

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
#include <unistd.h>
#include <time.h>
#include "NDSCart_SRAMManager.h"
#include "Platform.h"

namespace NDSCart_SRAMManager
{
    Platform::Thread* FlushThread;
    bool FlushThreadRunning;
    Platform::Mutex* SecondaryBufferLock;

    char Path[1024];

    u8* Buffer;
    u32 Length;

    u8* SecondaryBuffer;
    u32 SecondaryBufferLength;
    
    time_t TimeAtLastFlushRequest;

    // We keep versions in case the user closes the application before
    // a flush cycle is finished.
    u32 PreviousFlushVersion;
    u32 FlushVersion;

    void FlushThreadFunc();

    bool Init()
    {
        SecondaryBufferLock = Platform::Mutex_Create();

        return true;
    }

    void DeInit()
    {
        if (FlushThreadRunning)
        {
            FlushThreadRunning = false;
            Platform::Thread_Wait(FlushThread);
            Platform::Thread_Free(FlushThread);
            FlushSecondaryBuffer();
        }

        if (SecondaryBuffer) delete SecondaryBuffer;
        SecondaryBuffer = NULL;

        Platform::Mutex_Free(SecondaryBufferLock);
    }
    
    void Setup(const char* path, u8* buffer, u32 length)
    {
        // Flush SRAM in case there is unflushed data from previous state.
        FlushSecondaryBuffer();

        Platform::Mutex_Lock(SecondaryBufferLock);

        strncpy(Path, path, 1023);
        Path[1023] = '\0';

        Buffer = buffer;
        Length = length;

        if(SecondaryBuffer) delete SecondaryBuffer; // Delete secondary buffer, there might be previous state.

        SecondaryBuffer = new u8[length];
        SecondaryBufferLength = length;

        FlushVersion = 0;
        PreviousFlushVersion = 0;
        TimeAtLastFlushRequest = 0;

        Platform::Mutex_Unlock(SecondaryBufferLock);

        if (path[0] != '\0')
        {
            FlushThread = Platform::Thread_Create(FlushThreadFunc);
            FlushThreadRunning = true;
        }
    }

    void RequestFlush()
    {
        Platform::Mutex_Lock(SecondaryBufferLock);
        printf("NDS SRAM: Flush requested\n");
        memcpy(SecondaryBuffer, Buffer, Length);
        FlushVersion++;
        TimeAtLastFlushRequest = time(NULL);
        Platform::Mutex_Unlock(SecondaryBufferLock);
    }
    
    void FlushThreadFunc()
    {
        for (;;)
        {
            Platform::Sleep(100 * 1000); // 100ms

            if (!FlushThreadRunning) return;
            
            // We debounce for two seconds after last flush request to ensure that writing has finished.
            if (TimeAtLastFlushRequest == 0 || difftime(time(NULL), TimeAtLastFlushRequest) < 2)
            {
                continue;
            }

            FlushSecondaryBuffer();
        }
    }
    
    void FlushSecondaryBuffer(u8* dst, s32 dstLength)
    {
        // When flushing to a file, there's no point in re-writing the exact same data.
        if (!dst && !NeedsFlush()) return;
        // When flushing to memory, we don't know if dst already has any data so we only check that we CAN flush.
        if (dst && dstLength < SecondaryBufferLength) return;

        Platform::Mutex_Lock(SecondaryBufferLock);
        if (dst)
        {
            memcpy(dst, SecondaryBuffer, SecondaryBufferLength);
        }
        else
        {
            FILE* f = Platform::OpenFile(Path, "wb");
            if (f)
            {
                printf("NDS SRAM: Written\n");
                fwrite(SecondaryBuffer, SecondaryBufferLength, 1, f);
                fclose(f);
            }
        }
        PreviousFlushVersion = FlushVersion;
        TimeAtLastFlushRequest = 0;
        Platform::Mutex_Unlock(SecondaryBufferLock);
    }

    bool NeedsFlush()
    {
        return FlushVersion != PreviousFlushVersion;
    }

    void UpdateBuffer(u8* src, s32 srcLength)
    {
        if (!src || srcLength != Length) return;

        // should we create a lock for the primary buffer? this method is not intended to be called from a secondary thread in the way Flush is
        memcpy(Buffer, src, srcLength);
        Platform::Mutex_Lock(SecondaryBufferLock);
        memcpy(SecondaryBuffer, src, srcLength);
        Platform::Mutex_Unlock(SecondaryBufferLock);

        PreviousFlushVersion = FlushVersion;
    }
}
