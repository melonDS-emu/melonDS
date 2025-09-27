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
#include "NDS.h"

namespace melonDS
{

extern std::function<void()> OnStartEmulatorThread;

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

    enum InputDelayMode {
        InputDelayMode_Fair, // Attempt to make everyone's input delay the same
        InputDelayMode_Golf, // Make one specific player have the least delay
    };

    struct Player
    {
        int ID;
        char Name[32];
        PlayerStatus Status;
        u32 Address;
        u16 Port;
        bool IsLocalPlayer;
        u32 Ping;
        u32 LastCompletedFrame;
    };

    InputDelayMode NetworkMode = InputDelayMode_Fair;
    int GolfModePlayerID;
    bool StallFrame;
    NDS *nds;

    bool StartHost(const char* player, int port);
    bool StartClient(const char* player, const char* host, int port);
    void EndSession();

    void SetInputBufferSize(int value);

    void StartGame();

    std::vector<Player> GetPlayerList();
    int GetNumPlayers() { return NumPlayers; }
    int GetMaxPlayers() { return MaxPlayers; }
    bool GetHostStatus() { return IsHost; }
    Player GetMyPlayer() { return MyPlayer; }

    void Process(int inst) override;

    void ProcessInput(int netplayID, NDS *nds, u32 inputMask, bool isTouching, u16 touchX, u16 touchY);
    void ApplyInput(int netplayID, NDS *nds);

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
    bool ReceivedInputThisFrame[16];

    int NumMirrorClients;

    // maps to convert between player IDs and local instance IDs
    int PlayerToInstance[16];
    int InstanceToPlayer[16];
    std::unordered_map<u16, u8> PortToPlayerIndex;

    struct NetworkSettings
    {
        u32 Delay;
    };

    NetworkSettings Settings;

    struct InputFrame
    {
        u32 FrameNum;
        u32 KeyMask;
        u32 Touching;
        u32 TouchX, TouchY;
    };

#pragma pack(push, 1)
    struct InputReport {
        u8 stallFrame;
        u32 frameIndex;
        u32 lastCompleteFrame; // The last frame index that we have everyone's inputs for
    };
#pragma pack(pop)

    std::map<u32, InputFrame> InputHistory[16];

    struct WaitingFrame
    {
        bool Active;
        u32 FrameNum;
        Savestate SavestateBuffer[16];
    };
    WaitingFrame PendingFrame;

    void SyncClients();

    void SendNetworkSettings();

    void StartLocal();
    void ProcessHost();
    void ProcessClient();
    void ProcessFrame(int inst);

    int GetPlayerIndexFromPort(u16 port);
    InputFrame *GetInputFrame(u16 playerID, u32 frameNum);


    void ReceiveInputs(ENetEvent &event);

    bool SendBlobToMirrorClients(int type, u32 len, u8* data);
    void RecvBlobFromMirrorHost(ENetPeer* peer, ENetPacket* pkt);
};

}

#endif // NETPLAY_H
