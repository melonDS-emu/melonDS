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
#include <stdlib.h>
#include <string.h>

#include <string>
#include <QStandardPaths>
#include <QString>
#include <QDir>
#include <QThread>
#include <QSemaphore>
#include <QMutex>
#include <QOpenGLContext>
#include <QSharedMemory>
#include <SDL_loadso.h>

#include "Platform.h"
#include "Config.h"
#include "ROMManager.h"
#include "CameraManager.h"
#include "LAN_Socket.h"
#include "LAN_PCap.h"
#include "LocalMP.h"
#include "OSD.h"
#include "SPI_Firmware.h"

#ifdef __WIN32__
#define fseek _fseeki64
#define ftell _ftelli64
#endif // __WIN32__

std::string EmuDirectory;

extern CameraManager* camManager[2];

void emuStop();


namespace Platform
{

QSharedMemory* IPCBuffer = nullptr;
int IPCInstanceID;

void IPCInit()
{
    IPCInstanceID = 0;

    IPCBuffer = new QSharedMemory("melonIPC");

    if (!IPCBuffer->attach())
    {
        Log(LogLevel::Info, "IPC sharedmem doesn't exist. creating\n");
        if (!IPCBuffer->create(1024))
        {
            Log(LogLevel::Error, "IPC sharedmem create failed :(\n");
            delete IPCBuffer;
            IPCBuffer = nullptr;
            return;
        }

        IPCBuffer->lock();
        memset(IPCBuffer->data(), 0, IPCBuffer->size());
        IPCBuffer->unlock();
    }

    IPCBuffer->lock();
    u8* data = (u8*)IPCBuffer->data();
    u16 mask = *(u16*)&data[0];
    for (int i = 0; i < 16; i++)
    {
        if (!(mask & (1<<i)))
        {
            IPCInstanceID = i;
            *(u16*)&data[0] |= (1<<i);
            break;
        }
    }
    IPCBuffer->unlock();

    Log(LogLevel::Info, "IPC: instance ID %d\n", IPCInstanceID);
}

void IPCDeInit()
{
    if (IPCBuffer)
    {
        IPCBuffer->lock();
        u8* data = (u8*)IPCBuffer->data();
        *(u16*)&data[0] &= ~(1<<IPCInstanceID);
        IPCBuffer->unlock();

        IPCBuffer->detach();
        delete IPCBuffer;
    }
    IPCBuffer = nullptr;
}


void Init(int argc, char** argv)
{
#if defined(__WIN32__) || defined(PORTABLE)
    if (argc > 0 && strlen(argv[0]) > 0)
    {
        int len = strlen(argv[0]);
        while (len > 0)
        {
            if (argv[0][len] == '/') break;
            if (argv[0][len] == '\\') break;
            len--;
        }
        if (len > 0)
        {
            std::string emudir = argv[0];
            EmuDirectory = emudir.substr(0, len);
        }
        else
        {
            EmuDirectory = ".";
        }
    }
    else
    {
        EmuDirectory = ".";
    }
#else
    QString confdir;
    QDir config(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));
    config.mkdir("melonDS");
    confdir = config.absolutePath() + "/melonDS/";
    EmuDirectory = confdir.toStdString();
#endif

    IPCInit();
}

void DeInit()
{
    IPCDeInit();
}

void SignalStop(StopReason reason)
{
    emuStop();
    switch (reason)
    {
        case StopReason::GBAModeNotSupported:
            Log(LogLevel::Error, "!! GBA MODE NOT SUPPORTED\n");
            OSD::AddMessage(0xFFA0A0, "GBA mode not supported.");
            break;
        case StopReason::BadExceptionRegion:
            OSD::AddMessage(0xFFA0A0, "Internal error.");
            break;
        case StopReason::PowerOff:
        case StopReason::External:
            OSD::AddMessage(0xFFC040, "Shutdown");
        default:
            break;
    }
}


int InstanceID()
{
    return IPCInstanceID;
}

std::string InstanceFileSuffix()
{
    int inst = IPCInstanceID;
    if (inst == 0) return "";

    char suffix[16] = {0};
    snprintf(suffix, 15, ".%d", inst+1);
    return suffix;
}


int GetConfigInt(ConfigEntry entry)
{
    const int imgsizes[] = {0, 256, 512, 1024, 2048, 4096};

    switch (entry)
    {
#ifdef JIT_ENABLED
    case JIT_MaxBlockSize: return Config::JIT_MaxBlockSize;
#endif

    case DLDI_ImageSize: return imgsizes[Config::DLDISize];

    case DSiSD_ImageSize: return imgsizes[Config::DSiSDSize];

    case AudioBitDepth: return Config::AudioBitDepth;

#ifdef GDBSTUB_ENABLED
    case GdbPortARM7: return Config::GdbPortARM7;
    case GdbPortARM9: return Config::GdbPortARM9;
#endif
    }

    return 0;
}

bool GetConfigBool(ConfigEntry entry)
{
    switch (entry)
    {
#ifdef JIT_ENABLED
    case JIT_Enable: return Config::JIT_Enable != 0;
    case JIT_LiteralOptimizations: return Config::JIT_LiteralOptimisations != 0;
    case JIT_BranchOptimizations: return Config::JIT_BranchOptimisations != 0;
    case JIT_FastMemory: return Config::JIT_FastMemory != 0;
#endif

    case ExternalBIOSEnable: return Config::ExternalBIOSEnable != 0;

    case DLDI_Enable: return Config::DLDIEnable != 0;
    case DLDI_ReadOnly: return Config::DLDIReadOnly != 0;
    case DLDI_FolderSync: return Config::DLDIFolderSync != 0;

    case DSiSD_Enable: return Config::DSiSDEnable != 0;
    case DSiSD_ReadOnly: return Config::DSiSDReadOnly != 0;
    case DSiSD_FolderSync: return Config::DSiSDFolderSync != 0;

    case DSi_FullBIOSBoot: return Config::DSiFullBIOSBoot != 0;

#ifdef GDBSTUB_ENABLED
    case GdbEnabled: return Config::GdbEnabled;
    case GdbARM7BreakOnStartup: return Config::GdbARM7BreakOnStartup;
    case GdbARM9BreakOnStartup: return Config::GdbARM9BreakOnStartup;
#endif
    }

    return false;
}

std::string GetConfigString(ConfigEntry entry)
{
    switch (entry)
    {
    case DLDI_ImagePath: return Config::DLDISDPath;
    case DLDI_FolderPath: return Config::DLDIFolderPath;

    case DSiSD_ImagePath: return Config::DSiSDPath;
    case DSiSD_FolderPath: return Config::DSiSDFolderPath;

    case WifiSettingsPath: return Config::WifiSettingsPath;
    }

    return "";
}

bool GetConfigArray(ConfigEntry entry, void* data)
{
    switch (entry)
    {
    case Firm_MAC:
        {
            std::string& mac_in = Config::FirmwareMAC;
            u8* mac_out = (u8*)data;

            int o = 0;
            u8 tmp = 0;
            for (int i = 0; i < 18; i++)
            {
                char c = mac_in[i];
                if (c == '\0') break;

                int n;
                if      (c >= '0' && c <= '9') n = c - '0';
                else if (c >= 'a' && c <= 'f') n = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') n = c - 'A' + 10;
                else continue;

                if (!(o & 1))
                    tmp = n;
                else
                    mac_out[o >> 1] = n | (tmp << 4);

                o++;
                if (o >= 12) return true;
            }
        }
        return false;
    }

    return false;
}

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

    bool file_exists = QFile::exists(QString::fromStdString(path));
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
    QString qpath = QString::fromStdString(path);
    QDir dir(qpath);
    QString fullpath;

    if (dir.isAbsolute())
    {
        // If it's an absolute path, just open that.
        fullpath = qpath;
    }
    else
    {
#ifdef PORTABLE
        fullpath = QString::fromStdString(EmuDirectory) + QDir::separator() + qpath;
#else
        // Check user configuration directory
        QDir config(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
        config.mkdir("melonDS");
        fullpath = config.absolutePath() + "/melonDS/";
        fullpath.append(qpath);
#endif
    }

    return OpenFile(fullpath.toStdString(), mode);
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


void WriteNDSSave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen)
{
    if (ROMManager::NDSSave)
        ROMManager::NDSSave->RequestFlush(savedata, savelen, writeoffset, writelen);
}

void WriteGBASave(const u8* savedata, u32 savelen, u32 writeoffset, u32 writelen)
{
    if (ROMManager::GBASave)
        ROMManager::GBASave->RequestFlush(savedata, savelen, writeoffset, writelen);
}

void WriteFirmware(const SPI_Firmware::Firmware& firmware, u32 writeoffset, u32 writelen)
{
    if (!ROMManager::FirmwareSave)
        return;

    if (firmware.Header().Identifier != SPI_Firmware::GENERATED_FIRMWARE_IDENTIFIER)
    { // If this is not the default built-in firmware...
        // ...then write the whole thing back.
        ROMManager::FirmwareSave->RequestFlush(firmware.Buffer(), firmware.Length(), writeoffset, writelen);
    }
    else
    {
        u32 eapstart = firmware.ExtendedAccessPointOffset();
        u32 eapend = eapstart + sizeof(firmware.ExtendedAccessPoints());

        u32 apstart = firmware.WifiAccessPointOffset();
        u32 apend = apstart + sizeof(firmware.AccessPoints());

        // assert that the extended access points come just before the regular ones
        assert(eapend == apstart);

        if (eapstart <= writeoffset && writeoffset < apend)
        { // If we're writing to the access points...
            const u8* buffer = firmware.ExtendedAccessPointPosition();
            u32 length = sizeof(firmware.ExtendedAccessPoints()) + sizeof(firmware.AccessPoints());
            ROMManager::FirmwareSave->RequestFlush(buffer, length, writeoffset - eapstart, writelen);
        }
    }

}

bool MP_Init()
{
    return LocalMP::Init();
}

void MP_DeInit()
{
    return LocalMP::DeInit();
}

void MP_Begin()
{
    return LocalMP::Begin();
}

void MP_End()
{
    return LocalMP::End();
}

int MP_SendPacket(u8* data, int len, u64 timestamp)
{
    return LocalMP::SendPacket(data, len, timestamp);
}

int MP_RecvPacket(u8* data, u64* timestamp)
{
    return LocalMP::RecvPacket(data, timestamp);
}

int MP_SendCmd(u8* data, int len, u64 timestamp)
{
    return LocalMP::SendCmd(data, len, timestamp);
}

int MP_SendReply(u8* data, int len, u64 timestamp, u16 aid)
{
    return LocalMP::SendReply(data, len, timestamp, aid);
}

int MP_SendAck(u8* data, int len, u64 timestamp)
{
    return LocalMP::SendAck(data, len, timestamp);
}

int MP_RecvHostPacket(u8* data, u64* timestamp)
{
    return LocalMP::RecvHostPacket(data, timestamp);
}

u16 MP_RecvReplies(u8* data, u64 timestamp, u16 aidmask)
{
    return LocalMP::RecvReplies(data, timestamp, aidmask);
}

bool LAN_Init()
{
    if (Config::DirectLAN)
    {
        if (!LAN_PCap::Init(true))
            return false;
    }
    else
    {
        if (!LAN_Socket::Init())
            return false;
    }

    return true;
}

void LAN_DeInit()
{
    // checkme. blarg
    //if (Config::DirectLAN)
    //    LAN_PCap::DeInit();
    //else
    //    LAN_Socket::DeInit();
    LAN_PCap::DeInit();
    LAN_Socket::DeInit();
}

int LAN_SendPacket(u8* data, int len)
{
    if (Config::DirectLAN)
        return LAN_PCap::SendPacket(data, len);
    else
        return LAN_Socket::SendPacket(data, len);
}

int LAN_RecvPacket(u8* data)
{
    if (Config::DirectLAN)
        return LAN_PCap::RecvPacket(data);
    else
        return LAN_Socket::RecvPacket(data);
}


void Camera_Start(int num)
{
    return camManager[num]->start();
}

void Camera_Stop(int num)
{
    return camManager[num]->stop();
}

void Camera_CaptureFrame(int num, u32* frame, int width, int height, bool yuv)
{
    return camManager[num]->captureFrame(frame, width, height, yuv);
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
