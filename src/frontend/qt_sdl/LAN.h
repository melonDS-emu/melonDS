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

#ifndef LAN_H
#define LAN_H

#include <string>
#include <map>
#include <QMutex>

#include "types.h"

namespace LAN
{
using namespace melonDS;

enum PlayerStatus
{
    Player_None = 0,        // no player in this entry
    Player_Client,          // game client
    Player_Host,            // game host
    Player_Connecting,      // player still connecting
    Player_Disconnected,    // player disconnected
};

struct Player
{
    int ID;
    char Name[32];
    PlayerStatus Status;
    u32 Address;
};

struct DiscoveryData
{
    u32 Magic;
    u32 Version;
    u32 Tick;
    char SessionName[64];
    u8 NumPlayers;
    u8 MaxPlayers;
    u8 Status; // 0=idle 1=playing
};


extern bool Active;

extern std::map<u32, DiscoveryData> DiscoveryList;
extern QMutex DiscoveryMutex; // TODO: turn into Platform::Mutex or rework this to be nicer

extern Player Players[16];
extern u32 PlayerPing[16];
extern int NumPlayers;
extern int MaxPlayers;

extern Player MyPlayer;
extern u32 HostAddress;

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

int SendMPPacket(u8* data, int len, u64 timestamp);
int RecvMPPacket(u8* data, u64* timestamp);
int SendMPCmd(u8* data, int len, u64 timestamp);
int SendMPReply(u8* data, int len, u64 timestamp, u16 aid);
int SendMPAck(u8* data, int len, u64 timestamp);
int RecvMPHostPacket(u8* data, u64* timestamp);
u16 RecvMPReplies(u8* data, u64 timestamp, u16 aidmask);

}

#endif // LAN_H
