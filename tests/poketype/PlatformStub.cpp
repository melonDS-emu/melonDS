/*
    Copyright 2016-2026 melonDS team

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

// Constructing a cart links most of the emulator core, which needs these
// Platform functions defined. Log, WriteNDSSave, the mutex and the semaphore work;
// the rest do nothing.

#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

#include "Platform.h"
#include "PlatformStub.h"

namespace melonDS::Platform
{

void Log(LogLevel level, const char* fmt, ...)
{
    if (level < Warn) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void WriteNDSSave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata)
{
    SaveWrites().push_back({writeoffset, writelen});
}

bool AAC_Configure(AACDecoder* dec, int frequency, int channels)
{
    return false;
}

bool AAC_DecodeFrame(AACDecoder* dec, const void* input, int inputlen, void* output, int outputlen)
{
    return false;
}

void AAC_DeInit(AACDecoder* dec)
{
}

AACDecoder* AAC_Init()
{
    return nullptr;
}

bool Addon_KeyDown(KeyType type, void* userdata)
{
    return false;
}

float Addon_MotionQuery(MotionQueryType type, void* userdata)
{
    return 0;
}

void Addon_RumbleStart(u32 len, void* userdata)
{
}

void Addon_RumbleStop(void* userdata)
{
}

void Camera_CaptureFrame(int num, u32* frame, int width, int height, bool yuv, void* userdata)
{
}

void Camera_Start(int num, void* userdata)
{
}

void Camera_Stop(int num, void* userdata)
{
}

bool CloseFile(FileHandle* file)
{
    return false;
}

bool FileFlush(FileHandle* file)
{
    return false;
}

u64 FileLength(FileHandle* file)
{
    return 0;
}

u64 FileRead(void* data, u64 size, u64 count, FileHandle* file)
{
    return 0;
}

bool FileReadLine(char* str, int count, FileHandle* file)
{
    return false;
}

void FileRewind(FileHandle* file)
{
}

bool FileSeek(FileHandle* file, s64 offset, FileSeekOrigin origin)
{
    return false;
}

u64 FileWrite(const void* data, u64 size, u64 count, FileHandle* file)
{
    return 0;
}

u64 FileWriteFormatted(FileHandle* file, const char* fmt, ...)
{
    return 0;
}

bool IsEndOfFile(FileHandle* file)
{
    return false;
}

bool LocalFileExists(const std::string& name)
{
    return false;
}

int Mic_ReadInput(s16* data, int maxlength, void* userdata)
{
    return 0;
}

void Mic_Start(void* userdata)
{
}

void Mic_Stop(void* userdata)
{
}

void MP_Begin(void* userdata)
{
}

void MP_End(void* userdata)
{
}

int MP_RecvHostPacket(u8* data, u64* timestamp, void* userdata)
{
    return 0;
}

int MP_RecvPacket(u8* data, u64* timestamp, void* userdata)
{
    return 0;
}

u16 MP_RecvReplies(u8* data, u64 timestamp, u16 aidmask, void* userdata)
{
    return 0;
}

int MP_SendAck(u8* data, int len, u64 timestamp, void* userdata)
{
    return 0;
}

int MP_SendCmd(u8* data, int len, u64 timestamp, void* userdata)
{
    return 0;
}

int MP_SendPacket(u8* data, int len, u64 timestamp, void* userdata)
{
    return 0;
}

int MP_SendReply(u8* data, int len, u64 timestamp, u16 aid, void* userdata)
{
    return 0;
}

// a real lock, since constructing an NDS creates one (SPU)
struct Mutex
{
    std::mutex Lock;
};

Mutex* Mutex_Create()
{
    return new Mutex;
}

void Mutex_Free(Mutex* mutex)
{
    delete mutex;
}

void Mutex_Lock(Mutex* mutex)
{
    mutex->Lock.lock();
}

void Mutex_Unlock(Mutex* mutex)
{
    mutex->Lock.unlock();
}

int Net_RecvPacket(u8* data, void* userdata)
{
    return 0;
}

int Net_SendPacket(u8* data, int len, void* userdata)
{
    return 0;
}

FileHandle* OpenFile(const std::string& path, FileMode mode)
{
    return nullptr;
}

FileHandle* OpenLocalFile(const std::string& path, FileMode mode)
{
    return nullptr;
}

// real as well: the software renderer creates three
struct Semaphore
{
    std::mutex Lock;
    std::condition_variable Cond;
    int Count = 0;
};

Semaphore* Semaphore_Create()
{
    return new Semaphore;
}

void Semaphore_Free(Semaphore* sema)
{
    delete sema;
}

void Semaphore_Post(Semaphore* sema, int count)
{
    {
        std::lock_guard<std::mutex> lock(sema->Lock);
        sema->Count += count;
    }
    sema->Cond.notify_all();
}

void Semaphore_Reset(Semaphore* sema)
{
    std::lock_guard<std::mutex> lock(sema->Lock);
    sema->Count = 0;
}

void Semaphore_Wait(Semaphore* sema)
{
    std::unique_lock<std::mutex> lock(sema->Lock);
    sema->Cond.wait(lock, [sema] { return sema->Count > 0; });
    sema->Count--;
}

void SignalStop(StopReason reason, void* userdata)
{
}

Thread* Thread_Create(std::function<void()> func)
{
    abort();     // Thread_Wait is a no-op, so fail loudly rather than run a thread unjoined
}

void Thread_Free(Thread* thread)
{
}

void Thread_Wait(Thread* thread)
{
}

void WriteDateTime(int year, int month, int day, int hour, int minute, int second, void* userdata)
{
}

void WriteFirmware(const Firmware& firmware, u32 writeoffset, u32 writelen, void* userdata)
{
}

void WriteGBASave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata)
{
}

}

std::vector<SaveWrite>& SaveWrites()
{
    static std::vector<SaveWrite> writes;
    return writes;
}

void ClearSaveWrites()
{
    SaveWrites().clear();
}
