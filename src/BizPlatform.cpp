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
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <thread>
#include <mutex>
#include "Platform.h"

namespace Platform
{

FileStruct files[NUM_FILES];
time_t FrontendTime;

void Init(int argc, char** argv)
{
}

void DeInit()
{
}

void StopEmu()
{
}

FILE* OpenFile(const char* path, const char* mode, bool mustexist)
{
    return nullptr;
}

FILE* OpenLocalFile(const char* path, const char* mode)
{
    return nullptr;
}

Thread* Thread_Create(std::function<void()> func)
{
    std::thread* t = new std::thread(func);
    return (Thread*) t;
}

void Thread_Free(Thread* thread)
{
    delete (std::thread*) thread;
}

void Thread_Wait(Thread* thread)
{
	((std::thread*) thread)->join();
}

Semaphore* Semaphore_Create()
{
    sem_t* s = new sem_t;
    sem_init(s, 0, 1);
    return (Semaphore*) s;
}

void Semaphore_Free(Semaphore* sema)
{
    sem_destroy((sem_t*) sema);
    delete (sem_t*) sema;
}

void Semaphore_Reset(Semaphore* sema)
{
    while (!sem_trywait((sem_t*) sema)) {};
}

void Semaphore_Wait(Semaphore* sema)
{
    sem_wait((sem_t*) sema);
}

void Semaphore_Post(Semaphore* sema, int count)
{
    while (count--) sem_post((sem_t*) sema);
}

Mutex* Mutex_Create()
{
    std::mutex* m = new std::mutex();
    return (Mutex*) m;
}

void Mutex_Free(Mutex* mutex)
{
    delete (std::mutex*) mutex;
}

void Mutex_Lock(Mutex* mutex)
{
    ((std::mutex*) mutex)->lock();
}

void Mutex_Unlock(Mutex* mutex)
{
    ((std::mutex*) mutex)->unlock();
}

bool Mutex_TryLock(Mutex* mutex)
{
    return ((std::mutex*) mutex)->try_lock();
}

bool MP_Init()
{
    return false;
}

void MP_DeInit()
{
}

int MP_SendPacket(u8* data, int len)
{
    return 0;
}

int MP_RecvPacket(u8* data, bool block)
{
    return 0;
}

bool LAN_Init()
{
    return false;
}

void LAN_DeInit()
{
}

int LAN_SendPacket(u8* data, int len)
{
    return 0;
}

int LAN_RecvPacket(u8* data)
{
    return 0;
}

void Sleep(u64 usecs)
{
}

void SetFrontendTime(time_t newTime)
{
    FrontendTime = newTime;
}

time_t GetFrontendTime()
{
    return FrontendTime;
}

tm GetFrontendDate(time_t basetime)
{
    time_t t = basetime + FrontendTime; 
    return *gmtime(&t);
}

time_t ConvertDateToTime(tm date)
{
    return timegm(&date);
}

}
