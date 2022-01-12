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

#ifdef __WIN32__
    #define NTDDI_VERSION        0x06000000 // GROSS FUCKING HACK
    #include <winsock2.h>
    #include <windows.h>
    //#include <knownfolders.h> // FUCK THAT SHIT
    #include <shlobj.h>
    #include <ws2tcpip.h>
    #include <io.h>
    #define dup _dup
    #define socket_t    SOCKET
    #define sockaddr_t  SOCKADDR
#else
    #include <unistd.h>
    #include <netinet/in.h>
    #include <sys/select.h>
    #include <sys/socket.h>

    #define socket_t    int
    #define sockaddr_t  struct sockaddr
    #define closesocket close
#endif

#include <QStandardPaths>
#include <QString>
#include <QDir>
#include <QThread>
#include <QSemaphore>
#include <QMutex>
#include <QOpenGLContext>

#include "Platform.h"
#include "Config.h"
#include "ROMManager.h"
#include "LAN_Socket.h"
#include "LAN_PCap.h"
#include <string>

#ifndef INVALID_SOCKET
    #define INVALID_SOCKET  (socket_t)-1
#endif


std::string EmuDirectory;

void emuStop();


namespace Platform
{

socket_t MPSocket;
sockaddr_t MPSendAddr;
u8 PacketBuffer[2048];

#define NIFI_VER 1


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
}

void DeInit()
{
}


void StopEmu()
{
    emuStop();
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

    case Firm_RandomizeMAC: return Config::RandomizeMAC != 0;
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
    int opt_true = 1;
    int res;

#ifdef __WIN32__
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
    {
        return false;
    }
#endif // __WIN32__

    MPSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (MPSocket < 0)
    {
        return false;
    }

    res = setsockopt(MPSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt_true, sizeof(int));
    if (res < 0)
    {
        closesocket(MPSocket);
        MPSocket = INVALID_SOCKET;
        return false;
    }

#if defined(BSD) || defined(__APPLE__)
    res = setsockopt(MPSocket, SOL_SOCKET, SO_REUSEPORT, (const char*)&opt_true, sizeof(int));
    if (res < 0)
    {
        closesocket(MPSocket);
        MPSocket = INVALID_SOCKET;
        return false;
    }
#endif

    sockaddr_t saddr;
    saddr.sa_family = AF_INET;
    *(u32*)&saddr.sa_data[2] = htonl(Config::SocketBindAnyAddr ? INADDR_ANY : INADDR_LOOPBACK);
    *(u16*)&saddr.sa_data[0] = htons(7064);
    res = bind(MPSocket, &saddr, sizeof(sockaddr_t));
    if (res < 0)
    {
        closesocket(MPSocket);
        MPSocket = INVALID_SOCKET;
        return false;
    }

    res = setsockopt(MPSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&opt_true, sizeof(int));
    if (res < 0)
    {
        closesocket(MPSocket);
        MPSocket = INVALID_SOCKET;
        return false;
    }

    MPSendAddr.sa_family = AF_INET;
    *(u32*)&MPSendAddr.sa_data[2] = htonl(INADDR_BROADCAST);
    *(u16*)&MPSendAddr.sa_data[0] = htons(7064);

    return true;
}

void MP_DeInit()
{
    if (MPSocket >= 0)
        closesocket(MPSocket);

#ifdef __WIN32__
    WSACleanup();
#endif // __WIN32__
}

int MP_SendPacket(u8* data, int len)
{
    if (MPSocket < 0)
        return 0;

    if (len > 2048-8)
    {
        printf("MP_SendPacket: error: packet too long (%d)\n", len);
        return 0;
    }

    *(u32*)&PacketBuffer[0] = htonl(0x4946494E); // NIFI
    PacketBuffer[4] = NIFI_VER;
    PacketBuffer[5] = 0;
    *(u16*)&PacketBuffer[6] = htons(len);
    memcpy(&PacketBuffer[8], data, len);

    int slen = sendto(MPSocket, (const char*)PacketBuffer, len+8, 0, &MPSendAddr, sizeof(sockaddr_t));
    if (slen < 8) return 0;
    return slen - 8;
}

int MP_RecvPacket(u8* data, bool block)
{
    if (MPSocket < 0)
        return 0;

    fd_set fd;
    struct timeval tv;

    FD_ZERO(&fd);
    FD_SET(MPSocket, &fd);
    tv.tv_sec = 0;
    tv.tv_usec = block ? 5000 : 0;

    if (!select(MPSocket+1, &fd, 0, 0, &tv))
    {
        return 0;
    }

    sockaddr_t fromAddr;
    socklen_t fromLen = sizeof(sockaddr_t);
    int rlen = recvfrom(MPSocket, (char*)PacketBuffer, 2048, 0, &fromAddr, &fromLen);
    if (rlen < 8+24)
    {
        return 0;
    }
    rlen -= 8;

    if (ntohl(*(u32*)&PacketBuffer[0]) != 0x4946494E)
    {
        return 0;
    }

    if (PacketBuffer[4] != NIFI_VER)
    {
        return 0;
    }

    if (ntohs(*(u16*)&PacketBuffer[6]) != rlen)
    {
        return 0;
    }

    memcpy(data, &PacketBuffer[8], rlen);
    return rlen;
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

void Sleep(u64 usecs)
{
    QThread::usleep(usecs);
}

}
