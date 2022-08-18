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
#include "SPI.h"
#include <string>

#ifndef INVALID_SOCKET
    #define INVALID_SOCKET  (socket_t)-1
#endif


std::string EmuDirectory;

void emuStop();
extern u16 zanf;

namespace Platform
{

u32 MPUniqueID;
socket_t MPSocket[2];
sockaddr_t MPSendAddr[2];
u8 PacketBuffer[2048];

#define NIFI_VER 2


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


void _logpacket(bool tx, u8* data, int len)
{return;
    char path[256];
    sprintf(path, "framelog_%08X.log", MPUniqueID);
    static FILE* f = nullptr;
    if (!f) f = fopen(path, "a");

    /*fprintf(f, "---- %s PACKET LEN=%d ----\n", tx?"SENDING":"RECEIVING", len);

    for (int y = 0; y < len; y+=16)
    {
        fprintf(f, "%04X: ", y);
        int linelen = 16;
        if ((y+linelen) > len) linelen = len-y;
        for (int x = 0; x < linelen; x++)
        {
            fprintf(f, " %02X", data[y+x]);
        }
        fprintf(f, "\n");
    }

    fprintf(f, "-------------------------------------\n\n\n");*/

    fprintf(f, "%s PACKET: LEN=%0.4d FC=%04X SN=%04X CL=%04X/%04X\n", tx?"TX":"RX",
            len, *(u16*)&data[12], *(u16*)&data[12+22], *(u16*)&data[12+24], *(u16*)&data[12+26]);
    fflush(f);
}

bool MP_Init()
{
    int opt_true = 1;
    int res0, res1;

#ifdef __WIN32__
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0)
    {
        return false;
    }
#endif // __WIN32__

    MPSocket[0] = socket(AF_INET, SOCK_DGRAM, 0);
    if (MPSocket[0] < 0)
    {
        return false;
    }

    MPSocket[1] = socket(AF_INET, SOCK_DGRAM, 0);
    if (MPSocket[1] < 0)
    {
        closesocket(MPSocket[0]);
        MPSocket[0] = INVALID_SOCKET;
        return false;
    }

    res0 = setsockopt(MPSocket[0], SOL_SOCKET, SO_REUSEADDR, (const char*)&opt_true, sizeof(int));
    res1 = setsockopt(MPSocket[1], SOL_SOCKET, SO_REUSEADDR, (const char*)&opt_true, sizeof(int));
    if (res0 < 0 || res1 < 0)
    {
        closesocket(MPSocket[0]);
        MPSocket[0] = INVALID_SOCKET;
        closesocket(MPSocket[1]);
        MPSocket[1] = INVALID_SOCKET;
        return false;
    }

#if defined(BSD) || defined(__APPLE__)
    res0 = setsockopt(MPSocket[0], SOL_SOCKET, SO_REUSEPORT, (const char*)&opt_true, sizeof(int));
    res0 = setsockopt(MPSocket[1], SOL_SOCKET, SO_REUSEPORT, (const char*)&opt_true, sizeof(int));
    if (res0 < 0 || res1 < 0)
    {
        closesocket(MPSocket[0]);
        MPSocket[0] = INVALID_SOCKET;
        closesocket(MPSocket[1]);
        MPSocket[1] = INVALID_SOCKET;
        return false;
    }
#endif

    sockaddr_t saddr;
    saddr.sa_family = AF_INET;
    *(u32*)&saddr.sa_data[2] = htonl(Config::SocketBindAnyAddr ? INADDR_ANY : INADDR_LOOPBACK);
    *(u16*)&saddr.sa_data[0] = htons(7064);
    res0 = bind(MPSocket[0], &saddr, sizeof(sockaddr_t));
    *(u16*)&saddr.sa_data[0] = htons(7065);
    res1 = bind(MPSocket[1], &saddr, sizeof(sockaddr_t));
    if (res0 < 0 || res1 < 0)
    {
        closesocket(MPSocket[0]);
        MPSocket[0] = INVALID_SOCKET;
        closesocket(MPSocket[1]);
        MPSocket[1] = INVALID_SOCKET;
        return false;
    }

    res0 = setsockopt(MPSocket[0], SOL_SOCKET, SO_BROADCAST, (const char*)&opt_true, sizeof(int));
    res1 = setsockopt(MPSocket[1], SOL_SOCKET, SO_BROADCAST, (const char*)&opt_true, sizeof(int));
    if (res0 < 0 || res1 < 0)
    {
        closesocket(MPSocket[0]);
        MPSocket[0] = INVALID_SOCKET;
        closesocket(MPSocket[1]);
        MPSocket[1] = INVALID_SOCKET;
        return false;
    }

    MPSendAddr[0].sa_family = AF_INET;
    *(u32*)&MPSendAddr[0].sa_data[2] = htonl(INADDR_BROADCAST);
    *(u16*)&MPSendAddr[0].sa_data[0] = htons(7064);

    MPSendAddr[1].sa_family = AF_INET;
    *(u32*)&MPSendAddr[1].sa_data[2] = htonl(INADDR_BROADCAST);
    *(u16*)&MPSendAddr[1].sa_data[0] = htons(7065);

    u8* mac = SPI_Firmware::GetWifiMAC();
    MPUniqueID = *(u32*)&mac[0];
    MPUniqueID ^= *(u32*)&mac[2];
    printf("local MP unique ID: %08X\n", MPUniqueID);

    return true;
}

void MP_DeInit()
{
    if (MPSocket[0] >= 0)
        closesocket(MPSocket[0]);
    if (MPSocket[1] >= 0)
        closesocket(MPSocket[1]);

#ifdef __WIN32__
    WSACleanup();
#endif // __WIN32__
}

int MP_SendPacket(u8* data, int len)
{
    if (MPSocket[0] < 0)
        return 0;

    if (len > 2048-12)
    {
        printf("MP_SendPacket: error: packet too long (%d)\n", len);
        return 0;
    }

    *(u32*)&PacketBuffer[0] = htonl(0x4946494E); // NIFI
    PacketBuffer[4] = NIFI_VER;
    PacketBuffer[5] = 0;
    *(u16*)&PacketBuffer[6] = htons(len);
    *(u32*)&PacketBuffer[8] = MPUniqueID;
    memcpy(&PacketBuffer[12], data, len);

    _logpacket(true, data, len);

    int slen = sendto(MPSocket[0], (const char*)PacketBuffer, len+12, 0, &MPSendAddr[0], sizeof(sockaddr_t));
    if (slen < 12) return 0;
    return slen - 12;
}

int MP_RecvPacket(u8* data, bool block)
{
    if (MPSocket[0] < 0)
        return 0;

    fd_set fd;
    struct timeval tv;

    for (;;)
    {
        FD_ZERO(&fd);
        FD_SET(MPSocket[0], &fd);
        tv.tv_sec = 0;
        //tv.tv_usec = block ? 5000 : 0;
        tv.tv_usec = block ? 500*1000 : 0;

        if (!select(MPSocket[0]+1, &fd, 0, 0, &tv))
        {
            return 0;
        }

        sockaddr_t fromAddr;
        socklen_t fromLen = sizeof(sockaddr_t);
        int rlen = recvfrom(MPSocket[0], (char*)PacketBuffer, 2048, 0, &fromAddr, &fromLen);
        if (rlen < 12+24)
        {
            continue;
        }
        rlen -= 12;

        if (ntohl(*(u32*)&PacketBuffer[0]) != 0x4946494E)
        {
            continue;
        }

        if (PacketBuffer[4] != NIFI_VER || PacketBuffer[5] != 0)
        {
            continue;
        }

        if (ntohs(*(u16*)&PacketBuffer[6]) != rlen)
        {
            continue;
        }

        if (*(u32*)&PacketBuffer[8] == MPUniqueID)
        {
            continue;
        }

        zanf = *(u16*)&PacketBuffer[12+6];
        _logpacket(false, &PacketBuffer[12], rlen);

        memcpy(data, &PacketBuffer[12], rlen);
        return rlen;
    }
}

bool MP_SendSync(u16 clientmask, u16 type, u64 val)
{
    u8 syncbuf[32];

    if (MPSocket[1] < 0)
        return false;

    int len = 16;
    *(u32*)&syncbuf[0] = htonl(0x4946494E); // NIFI
    syncbuf[4] = NIFI_VER;
    syncbuf[5] = 1;
    *(u16*)&syncbuf[6] = htons(len);
    *(u16*)&syncbuf[8] = htons(type);
    *(u16*)&syncbuf[10] = htons(clientmask);
    *(u32*)&syncbuf[12] = MPUniqueID;
    *(u32*)&syncbuf[16] = htonl((u32)val);
    *(u32*)&syncbuf[20] = htonl((u32)(val>>32));

    int slen = sendto(MPSocket[1], (const char*)syncbuf, len+8, 0, &MPSendAddr[1], sizeof(sockaddr_t));
    return slen == len+8;
}

bool MP_WaitSync(u16 clientmask, u16* type, u64* val)
{
    u8 syncbuf[32];

    if (MPSocket[1] < 0)
        return false;

    fd_set fd;
    struct timeval tv;

    for (;;)
    {
        FD_ZERO(&fd);
        FD_SET(MPSocket[1], &fd);
        tv.tv_sec = 0;
        tv.tv_usec = 500*1000;

        if (!select(MPSocket[1]+1, &fd, 0, 0, &tv))
        {printf("sync fail\n");
            return false;
        }

        sockaddr_t fromAddr;
        socklen_t fromLen = sizeof(sockaddr_t);
        int rlen = recvfrom(MPSocket[1], (char*)syncbuf, 32, 0, &fromAddr, &fromLen);
        if (rlen != 8+16)
            continue;
        rlen -= 8;

        if (ntohl(*(u32*)&syncbuf[0]) != 0x4946494E)
            continue;

        if (syncbuf[4] != NIFI_VER || syncbuf[5] != 1)
            continue;

        if (ntohs(*(u16*)&syncbuf[6]) != rlen)
            continue;

        if (*(u32*)&syncbuf[12] == MPUniqueID)
            continue;

        u16 clientval = ntohs(*(u16*)&syncbuf[10]);
        if (!(clientmask & clientval))
            continue;

        // check the sync val, it should be ahead of the current sync val
        u64 syncval = ntohl(*(u32*)&syncbuf[16]) | (((u64)ntohl(*(u32*)&syncbuf[20])) << 32);
        //if (syncval <= curval)
        //    continue;

        if (type) *type = ntohs(*(u16*)&syncbuf[8]);
        if (val) *val = syncval;

        return true;
    }
}

u16 MP_WaitMultipleSyncs(u16 type, u16 clientmask, u64 curval)
{
    u8 syncbuf[32];

    if (!clientmask)
        return 0;

    if (MPSocket[1] < 0)
        return 0;

    fd_set fd;
    struct timeval tv;

    for (;;)
    {
        FD_ZERO(&fd);
        FD_SET(MPSocket[1], &fd);
        tv.tv_sec = 0;
        tv.tv_usec = 500*1000;

        if (!select(MPSocket[1]+1, &fd, 0, 0, &tv))
        {printf("[sync3] nope :(\n");
            return clientmask;
        }

        sockaddr_t fromAddr;
        socklen_t fromLen = sizeof(sockaddr_t);
        int rlen = recvfrom(MPSocket[1], (char*)syncbuf, 32, 0, &fromAddr, &fromLen);
        if (rlen != 8+16)
            continue;
        rlen -= 8;

        if (ntohl(*(u32*)&syncbuf[0]) != 0x4946494E)
            continue;

        if (syncbuf[4] != NIFI_VER || syncbuf[5] != 1)
            continue;
//printf("[sync3] atleast header is good\n");
        if (ntohs(*(u16*)&syncbuf[6]) != rlen)
            continue;

        if (*(u32*)&syncbuf[12] == MPUniqueID)
            continue;

        if (ntohs(*(u16*)&syncbuf[8]) != type)
            continue;

        u16 clientval = ntohs(*(u16*)&syncbuf[10]);
        //printf("[sync3] good rlen/type %04X %04X, clientmask=%04X \n", ntohs(*(u16*)&syncbuf[6]), ntohs(*(u16*)&syncbuf[8]), clientval);
        if (!(clientmask & clientval))
            continue;

        // check the sync val, it should be ahead of the current sync val
        u64 syncval = ntohl(*(u32*)&syncbuf[12]) | (((u64)ntohl(*(u32*)&syncbuf[16])) << 32);
        //if (syncval <= curval)
        //    continue;
//printf("[sync3] good\n");
        clientmask &= ~clientval;
        //if (!clientmask)
            return 0;
    }

    return clientmask;
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
