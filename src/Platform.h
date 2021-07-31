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

#ifndef PLATFORM_H
#define PLATFORM_H

#include "types.h"

#include <functional>

namespace Platform
{

void Init(int argc, char** argv);
void DeInit();

void StopEmu();

// fopen() wrappers
// * OpenFile():
//     simple fopen() wrapper that supports UTF8.
//     can be optionally restricted to only opening a file that already exists.
// * OpenLocalFile():
//     opens files local to the emulator (melonDS.ini, BIOS, firmware, ...)
//     For Windows builds, or portable UNIX builds it checks, by order of priority:
//     * current working directory
//     * emulator directory (essentially where the melonDS executable is) if supported
//     * any platform-specific application data directories
//     in create mode, if the file doesn't exist, it will be created in the emulator
//     directory if supported, or in the current directory otherwise
//     For regular UNIX builds, the user's configuration directory is always used.
// * OpenDataFile():
//     Opens a file that was installed alongside melonDS on UNIX systems in /usr/share, etc.
//     Looks in the user's data directory first, then the system's.
//     If on Windows or a portable UNIX build, this simply calls OpenLocalFile().

FILE* OpenFile(const char* path, const char* mode, bool mustexist=false);
FILE* OpenLocalFile(const char* path, const char* mode);
FILE* OpenDataFile(const char* path);

inline bool FileExists(const char* name)
{
    FILE* f = OpenFile(name, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

inline bool LocalFileExists(const char* name)
{
    FILE* f = OpenLocalFile(name, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

struct Thread;
Thread* Thread_Create(std::function<void()> func);
void Thread_Free(Thread* thread);
void Thread_Wait(Thread* thread);

struct Semaphore;
Semaphore* Semaphore_Create();
void Semaphore_Free(Semaphore* sema);
void Semaphore_Reset(Semaphore* sema);
void Semaphore_Wait(Semaphore* sema);
void Semaphore_Post(Semaphore* sema, int count = 1);

struct Mutex;
Mutex* Mutex_Create();
void Mutex_Free(Mutex* mutex);
void Mutex_Lock(Mutex* mutex);
void Mutex_Unlock(Mutex* mutex);
bool Mutex_TryLock(Mutex* mutex);


// local multiplayer comm interface
// packet type: DS-style TX header (12 bytes) + original 802.11 frame
bool MP_Init();
void MP_DeInit();
int MP_SendPacket(u8* data, int len);
int MP_RecvPacket(u8* data, bool block);

// LAN comm interface
// packet type: Ethernet (802.3)
bool LAN_Init();
void LAN_DeInit();
int LAN_SendPacket(u8* data, int len);
int LAN_RecvPacket(u8* data);

void Sleep(u64 usecs);

}

#endif // PLATFORM_H
