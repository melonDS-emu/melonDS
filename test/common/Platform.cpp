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

#include "Platform.h"

#include <filesystem>
#include <mutex>
#include <semaphore>
#include <thread>

namespace melonDS::Platform
{
void SignalStop(StopReason reason) {}
int InstanceID() { return 0; }

std::string InstanceFileSuffix() { return ""; }

constexpr char AccessMode(FileMode mode, bool file_exists)
{
    if (!(mode & FileMode::Write))
        // If we're only opening the file for reading...
            return 'r';

    if (mode & (FileMode::NoCreate))
        // If we're not allowed to create a new file...
            return 'r'; // Open in "r+" mode (IsExtended will add the "+")

    if ((mode & FileMode::Preserve) && file_exists)
        // If we're not allowed to overwrite a file that already exists...
            return 'r'; // Open in "r+" mode (IsExtended will add the "+")

    return 'w';
}

constexpr bool IsExtended(FileMode mode)
{
    // fopen's "+" flag always opens the file for read/write
    return (mode & FileMode::ReadWrite) == FileMode::ReadWrite;
}

static std::string GetModeString(FileMode mode, bool file_exists)
{
    std::string modeString;

    modeString += AccessMode(mode, file_exists);

    if (IsExtended(mode))
        modeString += '+';

    if (!(mode & FileMode::Text))
        modeString += 'b';

    return modeString;
}

FileHandle* OpenFile(const std::string& path, FileMode mode)
{
    if ((mode & FileMode::ReadWrite) == FileMode::None)
    { // If we aren't reading or writing, then we can't open the file
        Log(LogLevel::Error, "Attempted to open \"%s\" in neither read nor write mode (FileMode 0x%x)\n", path.c_str(), mode);
        return nullptr;
    }

    bool file_exists = std::filesystem::exists(path);
    std::string modeString = GetModeString(mode, file_exists);

    FILE* file = fopen(path.c_str(), modeString.c_str());
    if (file)
    {
        Log(LogLevel::Debug, "Opened \"%s\" with FileMode 0x%x (effective mode \"%s\")\n", path.c_str(), mode, modeString.c_str());
        return reinterpret_cast<FileHandle *>(file);
    }
    else
    {
        Log(LogLevel::Warn, "Failed to open \"%s\" with FileMode 0x%x (effective mode \"%s\")\n", path.c_str(), mode, modeString.c_str());
        return nullptr;
    }
}

FileHandle* OpenLocalFile(const std::string& path, FileMode mode)
{
    return OpenFile(path, mode);
}

bool CloseFile(FileHandle* file)
{
    return fclose(reinterpret_cast<FILE *>(file)) == 0;
}

bool IsEndOfFile(FileHandle* file)
{
    return feof(reinterpret_cast<FILE *>(file)) != 0;
}

bool FileReadLine(char* str, int count, FileHandle* file)
{
    return fgets(str, count, reinterpret_cast<FILE *>(file)) != nullptr;
}

bool FileExists(const std::string& name)
{
    FileHandle* f = OpenFile(name, FileMode::Read);
    if (!f) return false;
    CloseFile(f);
    return true;
}

bool LocalFileExists(const std::string& name)
{
    FileHandle* f = OpenLocalFile(name, FileMode::Read);
    if (!f) return false;
    CloseFile(f);
    return true;
}

bool FileSeek(FileHandle* file, s64 offset, FileSeekOrigin origin)
{
    int stdorigin;
    switch (origin)
    {
    case FileSeekOrigin::Start: stdorigin = SEEK_SET; break;
    case FileSeekOrigin::Current: stdorigin = SEEK_CUR; break;
    case FileSeekOrigin::End: stdorigin = SEEK_END; break;
    }

    return fseek(reinterpret_cast<FILE *>(file), offset, stdorigin) == 0;
}

void FileRewind(FileHandle* file)
{
    rewind(reinterpret_cast<FILE *>(file));
}

u64 FileRead(void* data, u64 size, u64 count, FileHandle* file)
{
    return fread(data, size, count, reinterpret_cast<FILE *>(file));
}

bool FileFlush(FileHandle* file)
{
    return fflush(reinterpret_cast<FILE *>(file)) == 0;
}

u64 FileWrite(const void* data, u64 size, u64 count, FileHandle* file)
{
    return fwrite(data, size, count, reinterpret_cast<FILE *>(file));
}

u64 FileWriteFormatted(FileHandle* file, const char* fmt, ...)
{
    if (fmt == nullptr)
        return 0;

    va_list args;
    va_start(args, fmt);
    u64 ret = vfprintf(reinterpret_cast<FILE *>(file), fmt, args);
    va_end(args);
    return ret;
}

u64 FileLength(FileHandle* file)
{
    FILE* stdfile = reinterpret_cast<FILE *>(file);
    long pos = ftell(stdfile);
    fseek(stdfile, 0, SEEK_END);
    long len = ftell(stdfile);
    fseek(stdfile, pos, SEEK_SET);
    return len;
}

void Log(LogLevel level, const char* fmt, ...)
{
    if (fmt == nullptr)
        return;

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

struct Thread
{
    std::thread thread;
};

Thread* Thread_Create(std::function<void()> func)
{
    return new Thread { std::thread(func) };
}

void Thread_Free(Thread* thread)
{
    if (thread)
    {
        thread->thread.join();
        delete thread;
    }
}

void Thread_Wait(Thread* thread)
{
    if (thread)
    {
        thread->thread.join();
    }
}

struct Semaphore
{
    std::counting_semaphore<> semaphore;

    Semaphore() : semaphore(0) {}
};

Semaphore* Semaphore_Create()
{
    return new Semaphore;
}

void Semaphore_Free(Semaphore* sema)
{
    delete sema;
}

void Semaphore_Reset(Semaphore* sema)
{
    while (sema->semaphore.try_acquire());
}

void Semaphore_Wait(Semaphore* sema)
{
    sema->semaphore.acquire();
}

void Semaphore_Post(Semaphore* sema, int count)
{
    sema->semaphore.release(count);
}

struct Mutex
{
    std::mutex mutex;
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
    if (mutex)
        mutex->mutex.lock();
}

void Mutex_Unlock(Mutex* mutex)
{
    if (mutex)
        mutex->mutex.unlock();
}

bool Mutex_TryLock(Mutex* mutex)
{
    if (!mutex)
        return false;

    return mutex->mutex.try_lock();
}

void WriteNDSSave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen) {}
void WriteGBASave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen) {}
void WriteFirmware(const Firmware& firmware, u32 writeoffset, u32 writelen) {}
void WriteDateTime(int year, int month, int day, int hour, int minute, int second) {}
bool MP_Init() { return false; }
void MP_DeInit() {}
void MP_Begin() {}
void MP_End() {}
int MP_SendPacket(u8* data, int len, u64 timestamp) { return 0; }
int MP_RecvPacket(u8* data, u64* timestamp) { return 0;}
int MP_SendCmd(u8* data, int len, u64 timestamp) { return 0; }
int MP_SendReply(u8* data, int len, u64 timestamp, u16 aid) { return 0;}
int MP_SendAck(u8* data, int len, u64 timestamp) { return 0; }
int MP_RecvHostPacket(u8* data, u64* timestamp) { return 0; }
u16 MP_RecvReplies(u8* data, u64 timestamp, u16 aidmask) { return 0; }
bool LAN_Init() { return false; }
void LAN_DeInit() {}
int LAN_SendPacket(u8* data, int len) { return 0; }
int LAN_RecvPacket(u8* data) { return 0; }
void Camera_Start(int num) {}
void Camera_Stop(int num) {}
void Camera_CaptureFrame(int num, u32* frame, int width, int height, bool yuv) {}
DynamicLibrary* DynamicLibrary_Load(const char* lib) { return nullptr;}
void DynamicLibrary_Unload(DynamicLibrary* lib) {}
void* DynamicLibrary_LoadFunction(DynamicLibrary* lib, const char* name) { return nullptr; }
}