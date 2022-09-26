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

#include "Platform.h"
#include "Config.h"
#include "ROMManager.h"
#include "CameraManager.h"
#include "LAN_Socket.h"
#include "LAN_PCap.h"
#include "LocalMP.h"


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
        printf("IPC sharedmem doesn't exist. creating\n");
        if (!IPCBuffer->create(1024))
        {
            printf("IPC sharedmem create failed :(\n");
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

    printf("IPC: instance ID %d\n", IPCInstanceID);
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


void StopEmu()
{
    emuStop();
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

    case Firm_Language: return Config::FirmwareLanguage;
    case Firm_BirthdayMonth: return Config::FirmwareBirthdayMonth;
    case Firm_BirthdayDay: return Config::FirmwareBirthdayDay;
    case Firm_Color: return Config::FirmwareFavouriteColour;

    case AudioBitrate: return Config::AudioBitrate;
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

    case Firm_OverrideSettings: return Config::FirmwareOverrideSettings != 0;
    }

    return false;
}

std::string GetConfigString(ConfigEntry entry)
{
    switch (entry)
    {
    case BIOS9Path: return Config::BIOS9Path;
    case BIOS7Path: return Config::BIOS7Path;
    case FirmwarePath: return Config::FirmwarePath;

    case DSi_BIOS9Path: return Config::DSiBIOS9Path;
    case DSi_BIOS7Path: return Config::DSiBIOS7Path;
    case DSi_FirmwarePath: return Config::DSiFirmwarePath;
    case DSi_NANDPath: return Config::DSiNANDPath;

    case DLDI_ImagePath: return Config::DLDISDPath;
    case DLDI_FolderPath: return Config::DLDIFolderPath;

    case DSiSD_ImagePath: return Config::DSiSDPath;
    case DSiSD_FolderPath: return Config::DSiSDFolderPath;

    case Firm_Username: return Config::FirmwareUsername;
    case Firm_Message: return Config::FirmwareMessage;
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


FILE* OpenFile(std::string path, std::string mode, bool mustexist)
{
    QFile f(QString::fromStdString(path));

    if (mustexist && !f.exists())
    {
        return nullptr;
    }

    QIODevice::OpenMode qmode;
    if (mode.length() > 1 && mode[0] == 'r' && mode[1] == '+')
    {
		qmode = QIODevice::OpenModeFlag::ReadWrite;
	}
	else if (mode.length() > 1 && mode[0] == 'w' && mode[1] == '+')
    {
    	qmode = QIODevice::OpenModeFlag::Truncate | QIODevice::OpenModeFlag::ReadWrite;
	}
	else if (mode[0] == 'w')
    {
        qmode = QIODevice::OpenModeFlag::Truncate | QIODevice::OpenModeFlag::WriteOnly;
    }
    else
    {
        qmode = QIODevice::OpenModeFlag::ReadOnly;
    }

    f.open(qmode);
    FILE* file = fdopen(dup(f.handle()), mode.c_str());
    f.close();

    return file;
}

FILE* OpenLocalFile(std::string path, std::string mode)
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

    return OpenFile(fullpath.toStdString(), mode, mode[0] != 'w');
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

}
