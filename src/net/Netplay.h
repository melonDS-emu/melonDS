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

#ifndef NETPLAY_H
#define NETPLAY_H

#include <queue>

#include <enet/enet.h>

#include "types.h"
#include "Platform.h"
#include "LocalMP.h"

namespace melonDS
{

// since netplay relies on local MP comm locally,
// we extend the LocalMP class to reuse its functionality
class Netplay : public LocalMP
{
public:
    Netplay() noexcept;
    Netplay(const Netplay&) = delete;
    Netplay& operator=(const Netplay&) = delete;
    Netplay(Netplay&& other) = delete;
    Netplay& operator=(Netplay&& other) = delete;
    ~Netplay() noexcept;

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
        int Status; // 0=no player 1=normal 2=host 3=connecting
        u32 Address;
        bool IsLocalPlayer;
    };

    bool StartHost(const char* player, int port);
    bool StartClient(const char* player, const char* host, int port);
    void EndSession();

    void StartGame();

    std::vector<Player> GetPlayerList();
    int GetNumPlayers() { return NumPlayers; }
    int GetMaxPlayers() { return MaxPlayers; }

    void Process(int inst) override;

private:
    bool Inited;
    bool Active;
    bool IsHost;

    ENetHost* Host;
    ENetPeer* RemotePeers[16];

    Player Players[16];
    int NumPlayers;
    int MaxPlayers;
    Platform::Mutex* PlayersMutex;

    Player MyPlayer;
    u32 HostAddress;
    bool Lag;

    int NumMirrorClients;

    // maps to convert between player IDs and local instance IDs
    int PlayerToInstance[16];
    int InstanceToPlayer[16];

    struct InputFrame
    {
        u32 FrameNum;
        u32 KeyMask;
        u32 Touching;
        u32 TouchX, TouchY;
    };

    std::queue<InputFrame> InputQueue;

    void StartLocal();
    void ProcessHost();
    void ProcessClient();
    void ProcessFrame(int inst);
};

}

#endif // NETPLAY_H
