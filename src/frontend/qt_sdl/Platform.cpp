/*
    Copyright 2016-2024 melonDS team

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

#include <string>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QThread>
#include <QSemaphore>
#include <QMutex>
#include <QSharedMemory>
#include <QTemporaryFile>
#include <SDL_loadso.h>

#include "Platform.h"
#include "Config.h"
#include "EmuInstance.h"
#include "main.h"
#include "CameraManager.h"
#include "Net.h"
#include "MPInterface.h"
#include "SPI_Firmware.h"

#ifdef __WIN32__
#define fseek _fseeki64
#define ftell _ftelli64
#endif // __WIN32__

extern CameraManager* camManager[2];

extern melonDS::Net net;

namespace melonDS::Platform
{

void SignalStop(StopReason reason, void* userdata)
{
    EmuInstance* inst = (EmuInstance*)userdata;
    inst->emuStop(reason);
}


static QIODevice::OpenMode GetQMode(FileMode mode)
{
    QIODevice::OpenMode qmode = QIODevice::OpenModeFlag::NotOpen;
    if (mode & FileMode::Read)
        qmode |= QIODevice::OpenModeFlag::ReadOnly;
    if (mode & FileMode::Write)
        qmode |= QIODevice::OpenModeFlag::WriteOnly;
    if (mode & FileMode::Append)
        qmode |= QIODevice::OpenModeFlag::Append;

    if ((mode & FileMode::Write) && !(mode & FileMode::Preserve))
        qmode |= QIODevice::OpenModeFlag::Truncate;

    if (mode & FileMode::NoCreate)
        qmode |= QIODevice::OpenModeFlag::ExistingOnly;

    if (mode & FileMode::Text)
        qmode |= QIODevice::OpenModeFlag::Text;

    return qmode;
}

constexpr char AccessMode(FileMode mode, bool file_exists)
{
    if (mode & FileMode::Append)
        return  'a';

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
    if ((mode & (FileMode::ReadWrite | FileMode::Append)) == FileMode::None)
    { // If we aren't reading or writing, then we can't open the file
        Log(LogLevel::Error, "Attempted to open \"%s\" in neither read nor write mode (FileMode 0x%x)\n", path.c_str(), mode);
        return nullptr;
    }

    QString qpath{QString::fromStdString(path)};

    std::string modeString = GetModeString(mode, QFile::exists(qpath));
    QIODevice::OpenMode qmode = GetQMode(mode);
    QFile qfile{qpath};
    qfile.open(qmode);
    FILE* file = fdopen(dup(qfile.handle()), modeString.c_str());
    qfile.close();

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

std::string GetLocalFilePath(const std::string& filename)
{
    QString qpath = QString::fromStdString(filename);
    QDir dir(qpath);
    QString fullpath;

    if (dir.isAbsolute())
    {
        // If it's an absolute path, just open that.
        fullpath = qpath;
    }
    else
    {
        fullpath = emuDirectory + QDir::separator() + qpath;
    }

    return fullpath.toStdString();
}

FileHandle* OpenLocalFile(const std::string& path, FileMode mode)
{
    return OpenFile(GetLocalFilePath(path), mode);
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

bool CheckFileWritable(const std::string& filepath)
{
    FileHandle* file = Platform::OpenFile(filepath.c_str(), FileMode::Read);

    if (file)
    {
        // if the file exists, check if it can be opened for writing.
        Platform::CloseFile(file);
        file = Platform::OpenFile(filepath.c_str(), FileMode::Append);
        if (file)
        {
            Platform::CloseFile(file);
            return true;
        }
        else return false;
    }
    else
    {
        // if the file does not exist, create a temporary file to check, to avoid creating an empty file.
        if (QTemporaryFile(filepath.c_str()).open())
        {
            return true;
        }
        else return false;
    }
}

bool CheckLocalFileWritable(const std::string& name)
{
    FileHandle* file = Platform::OpenLocalFile(name.c_str(), FileMode::Append);
    if (file)
    {
        Platform::CloseFile(file);
        return true;
    }
    else return false;
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
    vprintf(fmt, args);
    va_end(args);
}

Thread* Thread_Create(std::function<void()> func)
{
    QThread* t = QThread::create(func);
    t->start();
    return (Thread*) t;
}

void Thread_Free(Thread* thread)
{
    QThread* t = (QThread*) thread;
    t->terminate();
    delete t;
}

void Thread_Wait(Thread* thread)
{
    ((QThread*) thread)->wait();
}

Semaphore* Semaphore_Create()
{
    return (Semaphore*)new QSemaphore();
}

void Semaphore_Free(Semaphore* sema)
{
    delete (QSemaphore*) sema;
}

void Semaphore_Reset(Semaphore* sema)
{
    QSemaphore* s = (QSemaphore*) sema;

    s->acquire(s->available());
}

void Semaphore_Wait(Semaphore* sema)
{
    ((QSemaphore*) sema)->acquire();
}

bool Semaphore_TryWait(Semaphore* sema, int timeout_ms)
{
    if (!timeout_ms)
        return ((QSemaphore*)sema)->tryAcquire(1);

    return ((QSemaphore*)sema)->tryAcquire(1, timeout_ms);
}

void Semaphore_Post(Semaphore* sema, int count)
{
    ((QSemaphore*) sema)->release(count);
}

Mutex* Mutex_Create()
{
    return (Mutex*)new QMutex();
}

void Mutex_Free(Mutex* mutex)
{
    delete (QMutex*) mutex;
}

void Mutex_Lock(Mutex* mutex)
{
    ((QMutex*) mutex)->lock();
}

void Mutex_Unlock(Mutex* mutex)
{
    ((QMutex*) mutex)->unlock();
}

bool Mutex_TryLock(Mutex* mutex)
{
    return ((QMutex*) mutex)->try_lock();
}

void Sleep(u64 usecs)
{
    QThread::usleep(usecs);
}

u64 GetMSCount()
{
    return sysTimer.elapsed();
}

u64 GetUSCount()
{
    return sysTimer.nsecsElapsed() / 1000;
}


void WriteNDSSave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata)
{
    EmuInstance* inst = (EmuInstance*)userdata;
    if (inst->ndsSave)
        inst->ndsSave->RequestFlush(savedata, savelen, writeoffset, writelen);
}

void WriteGBASave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen, void* userdata)
{
    EmuInstance* inst = (EmuInstance*)userdata;
    if (inst->gbaSave)
        inst->gbaSave->RequestFlush(savedata, savelen, writeoffset, writelen);
}

void WriteFirmware(const Firmware& firmware, u32 writeoffset, u32 writelen, void* userdata)
{
    EmuInstance* inst = (EmuInstance*)userdata;
    printf("saving firmware for instance %d\n", inst->getInstanceID());
    if (!inst->firmwareSave)
        return;

    if (firmware.GetHeader().Identifier != GENERATED_FIRMWARE_IDENTIFIER)
    { // If this is not the default built-in firmware...
        // ...then write the whole thing back.
        inst->firmwareSave->RequestFlush(firmware.Buffer(), firmware.Length(), writeoffset, writelen);
    }
    else
    {
        u32 eapstart = firmware.GetExtendedAccessPointOffset();
        u32 eapend = eapstart + sizeof(firmware.GetExtendedAccessPoints());

        u32 apstart = firmware.GetWifiAccessPointOffset();
        u32 apend = apstart + sizeof(firmware.GetAccessPoints());

        // assert that the extended access points come just before the regular ones
        assert(eapend == apstart);

        if (eapstart <= writeoffset && writeoffset < apend)
        { // If we're writing to the access points...
            const u8* buffer = firmware.GetExtendedAccessPointPosition();
            u32 length = sizeof(firmware.GetExtendedAccessPoints()) + sizeof(firmware.GetAccessPoints());
            inst->firmwareSave->RequestFlush(buffer, length, writeoffset - eapstart, writelen);
        }
    }

}

void WriteDateTime(int year, int month, int day, int hour, int minute, int second, void* userdata)
{
    EmuInstance* inst = (EmuInstance*)userdata;
    QDateTime hosttime = QDateTime::currentDateTime();
    QDateTime time = QDateTime(QDate(year, month, day), QTime(hour, minute, second));
    auto& cfg = inst->getLocalConfig();

    cfg.SetInt64("RTC.Offset", hosttime.secsTo(time));
    Config::Save();
}


void MP_Begin(void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    MPInterface::Get().Begin(inst);
}

void MP_End(void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    MPInterface::Get().End(inst);
}

int MP_SendPacket(u8* data, int len, u64 timestamp, void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    return MPInterface::Get().SendPacket(inst, data, len, timestamp);
}

int MP_RecvPacket(u8* data, u64* timestamp, void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    return MPInterface::Get().RecvPacket(inst, data, timestamp);
}

int MP_SendCmd(u8* data, int len, u64 timestamp, void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    return MPInterface::Get().SendCmd(inst, data, len, timestamp);
}

int MP_SendReply(u8* data, int len, u64 timestamp, u16 aid, void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    return MPInterface::Get().SendReply(inst, data, len, timestamp, aid);
}

int MP_SendAck(u8* data, int len, u64 timestamp, void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    return MPInterface::Get().SendAck(inst, data, len, timestamp);
}

int MP_RecvHostPacket(u8* data, u64* timestamp, void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    return MPInterface::Get().RecvHostPacket(inst, data, timestamp);
}

u16 MP_RecvReplies(u8* data, u64 timestamp, u16 aidmask, void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    return MPInterface::Get().RecvReplies(inst, data, timestamp, aidmask);
}


int Net_SendPacket(u8* data, int len, void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    net.SendPacket(data, len, inst);
    return 0;
}

int Net_RecvPacket(u8* data, void* userdata)
{
    int inst = ((EmuInstance*)userdata)->getInstanceID();
    return net.RecvPacket(data, inst);
}


void Camera_Start(int num, void* userdata)
{
    return camManager[num]->start();
}

void Camera_Stop(int num, void* userdata)
{
    return camManager[num]->stop();
}

void Camera_CaptureFrame(int num, u32* frame, int width, int height, bool yuv, void* userdata)
{
    return camManager[num]->captureFrame(frame, width, height, yuv);
}

void Addon_RumbleStart(u32 len, void* userdata)
{
    ((EmuInstance*)userdata)->inputRumbleStart(len);
}

void Addon_RumbleStop(void* userdata)
{
    ((EmuInstance*)userdata)->inputRumbleStop();
}

float Addon_MotionQuery(MotionQueryType type, void* userdata)
{
    return ((EmuInstance*)userdata)->inputMotionQuery(type);
}

DynamicLibrary* DynamicLibrary_Load(const char* lib)
{
    return (DynamicLibrary*) SDL_LoadObject(lib);
}

void DynamicLibrary_Unload(DynamicLibrary* lib)
{
    SDL_UnloadObject(lib);
}

void* DynamicLibrary_LoadFunction(DynamicLibrary* lib, const char* name)
{
    return SDL_LoadFunction(lib, name);
}

}
