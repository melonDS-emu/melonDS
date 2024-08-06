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

#ifndef LAN_H
#define LAN_H

#include <string>
#include <map>
#include <QMutex>

#include "types.h"

namespace LAN
{

struct Player
{
    int ID;
    char Name[32];
    int Status; // 0=no player 1=normal 2=host 3=connecting 4=disconnected
    melonDS::u32 Address;
};

struct DiscoveryData
{
    melonDS::u32 Magic;
    melonDS::u32 Version;
    melonDS::u32 Tick;
    char SessionName[64];
    melonDS::u8 NumPlayers;
    melonDS::u8 MaxPlayers;
    melonDS::u8 Status; // 0=idle 1=playing
};


extern bool Active;

extern std::map<melonDS::u32, DiscoveryData> DiscoveryList;
extern QMutex DiscoveryMutex; // TODO: turn into Platform::Mutex or rework this to be nicer

extern Player Players[16];
extern melonDS::u32 PlayerPing[16];
extern int NumPlayers;
extern int MaxPlayers;

extern Player MyPlayer;
extern melonDS::u32 HostAddress;

bool Init();
void DeInit();

bool StartDiscovery();
void EndDiscovery();
bool StartHost(const char* player, int numplayers);
bool StartClient(const char* player, const char* host);

void ProcessFrame();

void SetMPRecvTimeout(int timeout);
void MPBegin();
void MPEnd();

int SendMPPacket(melonDS::u8* data, int len, melonDS::u64 timestamp);
int RecvMPPacket(melonDS::u8* data, melonDS::u64* timestamp);
int SendMPCmd(melonDS::u8* data, int len, melonDS::u64 timestamp);
int SendMPReply(melonDS::u8* data, int len, melonDS::u64 timestamp, melonDS::u16 aid);
int SendMPAck(melonDS::u8* data, int len, melonDS::u64 timestamp);
int RecvMPHostPacket(melonDS::u8* data, melonDS::u64* timestamp);
melonDS::u16 RecvMPReplies(melonDS::u8* data, melonDS::u64 timestamp, melonDS::u16 aidmask);

}

#endif // LAN_H
