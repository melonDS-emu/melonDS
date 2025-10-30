/*
    Copyright 2016-2025 melonDS team

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
#include <queue>
#include <cassert>

#include <enet/enet.h>
#include <zlib.h>

#include "NDSCart.h"
#include "Netplay.h"
#include "Savestate.h"
#include "Platform.h"


namespace melonDS
{

const u32 kNetplayMagic = 0x5054454E; // NETP

const u32 kProtocolVersion = 1;

const u32 kLocalhost = 0x0100007F;

enum
{
    Chan_Input0 = 0,        // channels 0-15 -- input from players 0-15 resp.
    Chan_Cmd = 16,          // channel 16 -- control commands
    Chan_Blob,              // channel 17 -- blob
    Chan_Max,
};

enum
{
    Cmd_ClientInit = 1,             // 01 -- host->client -- init new client and assign ID
    Cmd_PlayerInfo,                 // 02 -- client->host -- send client player info to host
    Cmd_PlayerList,                 // 03 -- host->client -- broadcast updated player list
    Cmd_StartGame,                  // 04 -- host->client -- start the game on all players
    Cmd_UpdateSettings,             // 05 -- host->client -- game settings changed
};

std::function<void()> OnStartEmulatorThread = nullptr;

Netplay::Netplay() noexcept : LocalMP(), Inited(false)
{
    Active = false;
    IsHost = false;
    Host = nullptr;
    StallFrame = false;
    Settings.Delay = 4;

    CurBlobType = -1;
    CurBlobLen = 0;

    for (int i = 0; i < Blob_MAX; i++)
    {
        Blobs[i] = nullptr;
        BlobLens[i] = 0;
    }

    PlayersMutex = Platform::Mutex_Create();
    InstanceMutex = Platform::Mutex_Create();
    NetworkMutex = Platform::Mutex_Create();

    memset(RemotePeers, 0, sizeof(RemotePeers));
    memset(Players, 0, sizeof(Players));
    NumPlayers = 0;
    MaxPlayers = 0;

    memset(PlayerToInstance, 0, sizeof(PlayerToInstance));
    memset(InstanceToPlayer, 0, sizeof(InstanceToPlayer));

    // TODO make this somewhat nicer
    if (enet_initialize() != 0)
    {
        Platform::Log(Platform::LogLevel::Error, "Netplay: failed to initialize enet\n");
        return;
    }

    Platform::Log(Platform::LogLevel::Info, "Netplay: enet initialized\n");
    Inited = true;
}

Netplay::~Netplay() noexcept
{
    EndSession();

    Inited = false;
    enet_deinitialize();

    Platform::Mutex_Free(PlayersMutex);

    Platform::Log(Platform::LogLevel::Info, "Netplay: enet deinitialized\n");
}

// To be called just before a game starts
bool Netplay::InitGame()
{
    static bool isInited = false;
    if (isInited) return true;

    if (!OnStartEmulatorThread)
    {
        printf("error, tried to start netplay game with OnStartEmulatorThread null!\n");
        return false;
    }

    OnStartEmulatorThread(); // Hack to access frontend code

    isInited = true;
    return true;
}

std::vector<Netplay::Player> Netplay::GetPlayerList()
{
    Platform::Mutex_Lock(PlayersMutex);

    std::vector<Player> ret;
    for (int i = 0; i < 16; i++)
    {
        if (Players[i].Status == Player_None) continue;

        // make a copy of the player entry, fix up the address field
        Player newp = Players[i];
        if (newp.ID == MyPlayer.ID)
        {
            newp.IsLocalPlayer = true;
            newp.Address = kLocalhost;
        }
        else
        {
            newp.IsLocalPlayer = false;
            if (newp.Status == Player_Host)
                newp.Address = HostAddress;
        }

        ret.push_back(newp);
    }

    Platform::Mutex_Unlock(PlayersMutex);
    return ret;
}

int Netplay::GetPlayerIndexFromPort(u16 port)
{
    auto it = PortToPlayerIndex.find(port);
    if (it != PortToPlayerIndex.end()) {
        return it->second;
    }
    printf("what? %d\n", port);
    return -1;
}

bool Netplay::StartHost(const char* playername, int port)
{
    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = port;

    Host = enet_host_create(&addr, 16, Chan_Max, 0, 0);
    if (!Host)
    {
        printf("host shat itself :(\n");
        return false;
    }

    Platform::Mutex_Lock(PlayersMutex);

    Player* player = &Players[0];
    memset(player, 0, sizeof(Player));
    player->ID = 0;
    strncpy(player->Name, playername, 31);
    player->Status = Player_Host;
    player->Address = 0x0100007F;
    NumPlayers = 1;
    MaxPlayers = 16;
    memcpy(&MyPlayer, player, sizeof(Player));

    Platform::Mutex_Unlock(PlayersMutex);

    HostAddress = 0x0100007F;

    Active = true;
    IsHost = true;

    return true;
}

bool Netplay::StartClient(const char* playername, const char* host, int port)
{
    Host = enet_host_create(nullptr, 16, Chan_Max, 0, 0);
    if (!Host)
    {
        printf("connection failed\n");
        return false;
    }

    printf("client created, connecting (%s, %s:%d)\n", playername, host, port);

    ENetAddress addr;
    enet_address_set_host(&addr, host);
    addr.port = port;
    ENetPeer* peer = enet_host_connect(Host, &addr, Chan_Max, 0);
    if (!peer)
    {
        printf("connection failed\n");
        return false;
    }

    Platform::Mutex_Lock(PlayersMutex);

    Player* player = &MyPlayer;
    memset(player, 0, sizeof(Player));
    player->ID = 0;
    strncpy(player->Name, playername, 31);
    player->Status = Player_Connecting;

    Platform::Mutex_Unlock(PlayersMutex);

    ENetEvent event;
    int conn = 0;
    u32 starttick = (u32)Platform::GetMSCount();
    const int conntimeout = 5000;
    for (;;)
    {
        u32 curtick = (u32)Platform::GetMSCount();
        if (curtick < starttick) break;
        int timeout = conntimeout - (int)(curtick - starttick);
        if (timeout < 0) break;
        if (enet_host_service(Host, &event, timeout) > 0)
        {
            if (conn == 0 && event.type == ENET_EVENT_TYPE_CONNECT)
            {
                conn = 1;
            }
            else if (conn == 1 && event.type == ENET_EVENT_TYPE_RECEIVE)
            {
                u8* data = event.packet->data;
                if (event.channelID != Chan_Cmd) continue;
                if (data[0] != Cmd_ClientInit) continue;
                if (event.packet->dataLength != 11 + sizeof(NetworkSettings)) continue;

                u32 magic = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
                u32 version = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
                if (magic != kNetplayMagic) continue;
                if (version != kProtocolVersion) continue;
                if (data[10] > 16) continue;

                MaxPlayers = data[10];
                memcpy(&Settings, &data[11], sizeof(NetworkSettings));

                // send player information
                MyPlayer.ID = data[9];
                u8 cmd[9+sizeof(Player)];
                cmd[0] = Cmd_PlayerInfo;
                cmd[1] = (u8)kNetplayMagic;
                cmd[2] = (u8)(kNetplayMagic >> 8);
                cmd[3] = (u8)(kNetplayMagic >> 16);
                cmd[4] = (u8)(kNetplayMagic >> 24);
                cmd[5] = (u8)kProtocolVersion;
                cmd[6] = (u8)(kProtocolVersion >> 8);
                cmd[7] = (u8)(kProtocolVersion >> 16);
                cmd[8] = (u8)(kProtocolVersion >> 24);
                memcpy(&cmd[9], &MyPlayer, sizeof(Player));
                ENetPacket* pkt = enet_packet_create(cmd, 9+sizeof(Player), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(event.peer, Chan_Cmd, pkt);

                conn = 2;
                break;
            }
            else if (event.type == ENET_EVENT_TYPE_DISCONNECT)
            {
                conn = 0;
                break;
            }
        }
        else
            break;
    }

    if (conn != 2)
    {
        enet_peer_reset(peer);
        enet_host_destroy(Host);
        Host = nullptr;
        return false;
    }

    HostAddress = addr.host;
    RemotePeers[0] = peer;
    PortToPlayerIndex[port] = 0;

    Active = true;
    IsHost = false;
    return true;
}

void Netplay::EndSession()
{
    if (!Active) return;

    Active = false;

    for (int i = 0; i < 16; i++)
    {
        if (i == MyPlayer.ID) continue;

        if (RemotePeers[i])
            enet_peer_disconnect(RemotePeers[i], 0);

        RemotePeers[i] = nullptr;
    }

    enet_host_destroy(Host);
    Host = nullptr;
    IsHost = false;
}

void Netplay::SendNetworkSettings()
{
    if (!IsHost) return;

    u8 cmd[1 + sizeof(NetworkSettings)];
    cmd[0] = Cmd_UpdateSettings;
    memcpy(&cmd[1], &Settings, sizeof(NetworkSettings));
    ENetPacket* pkt = enet_packet_create(cmd, sizeof(cmd), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, Chan_Cmd, pkt);
}

void Netplay::SetInputBufferSize(int value)
{
    if (!IsHost) return;
    Settings.Delay = value;

    // tell clients that it changed
    SendNetworkSettings();
}

bool Netplay::SendBlob(int type, u32 len, u8* data)
{
    u8* buf = ChunkBuffer;

    buf[0] = 0x01;
    buf[1] = type & 0xFF;
    buf[2] = 0;
    buf[3] = 0;
    *(u32*)&buf[4] = len;

    u32 crc = crc32(0L, Z_NULL, 0);
    if (len > 0)
        crc = crc32(crc, data, len);
    *(u32*)&buf[8] = crc;

    printf("send blob, type %d, len 0x%08X, hash: 0x%08X\n", type, len, crc);

    ENetPacket* pkt = enet_packet_create(buf, 12, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, Chan_Blob, pkt);

    if (len > 0)
    {
        buf[0] = 0x02;
        *(u32*)&buf[12] = 0;

        for (u32 pos = 0; pos < len; pos += kChunkSize)
        {
            u32 chunklen = kChunkSize;
            if ((pos + chunklen) > len)
                chunklen = len - pos;

            *(u32*)&buf[8] = pos;
            memcpy(&buf[16], &data[pos], chunklen);

            ENetPacket* pkt = enet_packet_create(buf, 16+chunklen, ENET_PACKET_FLAG_RELIABLE);
            enet_host_broadcast(Host, Chan_Blob, pkt);
        }
    }

    buf[0] = 0x03;
    *(u32*)&buf[8] = crc;

    pkt = enet_packet_create(buf, 12, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, Chan_Blob, pkt);

    return true;
}

void Netplay::RecvBlob(ENetPeer* peer, ENetPacket* pkt)
{
    u8* buf = pkt->data;
    printf("got a blob packet %d\n", buf[0]);
    if (buf[0] == 0x01)
    {
        if (CurBlobType != -1) return;
        if (pkt->dataLength != 12) return;

        int type = buf[1];
        if (type > Blob_MAX) return;

        u32 len = *(u32*)&buf[4];
        if (len > 0x40000000) return;

        if (Blobs[type] != nullptr) return;
        if (BlobLens[type] != 0) return;
        printf("[MC] start blob type=%d len=%d\n", type, len);
        if (len) Blobs[type] = new u8[len];
        BlobLens[type] = len;

        CurBlobType = type;
        CurBlobLen = len;

        CurBlobCRC  = crc32(0L, Z_NULL, 0);

        BlobCurrSize = 0;

        ENetEvent evt;
        while (enet_host_service(Host, &evt, 5000) > 0)
        {
            if (evt.type == ENET_EVENT_TYPE_RECEIVE && evt.channelID == Chan_Blob)
            {
                RecvBlob(evt.peer, evt.packet);
                if (evt.packet->dataLength >= 1 && evt.packet->data[0] == 0x03)
                {
                    printf("[MC] blob done while in fast recv loop\n");
                    break;
                }
            }
            else
            {
                printf("[MC] fast recv loop aborted because evt %d ch %d\n", evt.type, evt.channelID);
                break;
            }
        }
    }
    else if (buf[0] == 0x02)
    {
        if (CurBlobType < 0 || CurBlobType > Blob_MAX) return;
        if (pkt->dataLength > (16+kChunkSize)) return;

        int type = buf[1];
        if (type != CurBlobType) return;

        u32 len = *(u32*)&buf[4];
        if (len != CurBlobLen) return;

        u32 pos = *(u32*)&buf[8];
        if (pos >= len) return;
        if ((pos + (pkt->dataLength-16)) > len) return;

        u8* dst = Blobs[type];
        if (!dst) return;
        if (BlobLens[type] != len) return;
        printf("[MC] recv blob data, type=%d pos=%08X len=%08X data=%08lX\n", type, pos, len, pkt->dataLength-16);
        memcpy(&dst[pos], &buf[16], pkt->dataLength-16);
        BlobCurrSize += pkt->dataLength-16;
        CurBlobCRC = crc32(CurBlobCRC, &buf[16], pkt->dataLength - 16);
    }
    else if (buf[0] == 0x03)
    {
        if (CurBlobType < 0 || CurBlobType > Blob_MAX) return;
        if (pkt->dataLength != 12) return;

        int type = buf[1];
        if (type != CurBlobType) return;

        u32 len = *(u32*)&buf[4];
        if (len != CurBlobLen) return;

        u32 ExpectedCRC = *(u32*)&buf[8];
        if (CurBlobCRC != ExpectedCRC) {
            printf("Blob %d CRC mismatch! expected 0x%08X got 0x%08X\n",
                   CurBlobType, ExpectedCRC, CurBlobCRC);
            return;
        }

        printf("[MC] finish blob type=%d len=%d, matching hashes! 0x%08X\n", type, len, CurBlobCRC);
        CurBlobType = -1;
        CurBlobLen = 0;
    }
    else if (buf[0] == 0x04)
    {
        if (pkt->dataLength != 2) return;

        InitGame();

        // reset
        nds->ConsoleType = buf[1];

        printf("received instruction to load save state! set console type to %d. size: 0x%08X\n", nds->ConsoleType, BlobCurrSize);

        // load initial state
        // todo can you do this on the stack?
        Savestate* state = new Savestate(Blobs[Blob_InitState], BlobLens[Blob_InitState], false);
        bool success = nds->DoSavestate(state);
        delete state;

        for (int i = 0; i < Blob_MAX; i++)
        {
            if (Blobs[i]) delete[] Blobs[i];
            Blobs[i] = nullptr;
            BlobLens[i] = 0;
        }

        if (success) {
            printf("[MC] state loaded, PC=%08X/%08X\n", nds->GetPC(0), nds->GetPC(1));
        } else {
            printf("[MC] couldn't load state\n");
        }
        ENetPacket* resp = enet_packet_create(buf, 1, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(peer, Chan_Blob, resp);
    }
}

void Netplay::SyncClients()
{
    if (!IsHost) return;

    // send initial state
    std::unique_ptr<Savestate> state = std::make_unique<Savestate>(Savestate::DEFAULT_SIZE);
    nds->DoSavestate(state.get());
    printf("[HOST] syncing clients. sending save state with size %d\n", state->Length());
    SendBlob(Blob_InitState, state->Length(), (u8 *) state->Buffer());

    u8 data[2];
    data[0] = 0x04;
    data[1] = (u8) nds->ConsoleType;
    ENetPacket* pkt = enet_packet_create(&data, 2, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, Chan_Blob, pkt);

    // wait for all clients to have caught up
    int ngood = 0;
    ENetEvent evt;
    while (enet_host_service(Host, &evt, 300000) > 0)
    {printf("EVENT %d CH %d\n", evt.type, evt.channelID);
        if (evt.type == ENET_EVENT_TYPE_RECEIVE && evt.channelID == Chan_Blob)
        {
            if (evt.packet->dataLength == 1 && evt.packet->data[0] == 0x04)
                ngood++;
        }
        else
            break;

        if (ngood >= (NumPlayers-1))
            break;
    }

    if (ngood != (NumPlayers-1))
        printf("!!! BAD!! %d %d\n", ngood, NumPlayers);

    printf("[HOST] clients synced\n");
}

void Netplay::StartGame()
{
    if (!IsHost)
    {
        printf("?????\n");
        return;
    }

    InitGame();

    if (NumPlayers > 1)
    {
        Platform::Mutex_Lock(NetworkMutex);

        SyncClients();

        // tell remote peers to start game
        u8 cmd[1] = { Cmd_StartGame };
        ENetPacket* pkt = enet_packet_create(cmd, sizeof(cmd), ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(Host, Chan_Cmd, pkt);

        Platform::Mutex_Unlock(NetworkMutex);
    }

    // start game locally
    StartLocal();
}

void Netplay::StartLocal()
{
    printf("starting netplay game\n");

    // assign local instances to players

    PlayerToInstance[MyPlayer.ID] = 0;
    InstanceToPlayer[0] = MyPlayer.ID;

    for (int p = 0, i = 1; p < 16; p++)
    {
        if (p == MyPlayer.ID) continue;

        Player* player = &Players[p];
        if (player->Status == Player_None) continue;

        PlayerToInstance[p] = i;
        InstanceToPlayer[i] = p;
        ReceivedInputThisFrame[p] = false;
        i++;
    }

    InitGame();
}


void Netplay::ProcessHost()
{
    if (!Host) return;

    Platform::Mutex_Lock(NetworkMutex);

    ENetEvent event;
    while (enet_host_service(Host, &event, 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            {
                if ((NumPlayers >= MaxPlayers) || (NumPlayers >= 16))
                {
                    // game is full, reject connection
                    enet_peer_disconnect(event.peer, 0);
                    break;
                }

                // TODO: reject connection if game is running

                // client connected; assign player number

                int id;
                for (id = 0; id < 16; id++)
                {
                    if (id >= NumPlayers) break;
                    if (Players[id].Status == Player_None) break;
                }

                if (id < 16)
                {
                    u8 cmd[11 + sizeof(NetworkSettings)];
                    cmd[0] = Cmd_ClientInit;
                    cmd[1] = (u8)kNetplayMagic;
                    cmd[2] = (u8)(kNetplayMagic >> 8);
                    cmd[3] = (u8)(kNetplayMagic >> 16);
                    cmd[4] = (u8)(kNetplayMagic >> 24);
                    cmd[5] = (u8)kProtocolVersion;
                    cmd[6] = (u8)(kProtocolVersion >> 8);
                    cmd[7] = (u8)(kProtocolVersion >> 16);
                    cmd[8] = (u8)(kProtocolVersion >> 24);
                    cmd[9] = (u8)id;
                    cmd[10] = MaxPlayers;
                    memcpy(&cmd[11], &Settings, sizeof(NetworkSettings));
                    printf("a client is connecting with id %d, from %08X, port %d\n", id, event.peer->address.host, event.peer->address.port);
                    ENetPacket* pkt = enet_packet_create(cmd, sizeof(cmd), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, Chan_Cmd, pkt);

                    Platform::Mutex_Lock(PlayersMutex);

                    Players[id].ID = id;
                    Players[id].Status = Player_Connecting;
                    Players[id].Address = event.peer->address.host;
                    event.peer->data = &Players[id];
                    NumPlayers++;

                    Platform::Mutex_Unlock(PlayersMutex);

                    SendNetworkSettings();

                    PortToPlayerIndex[event.peer->address.port] = id;
                    RemotePeers[id] = event.peer;
                }
                else
                {
                    // ???
                    enet_peer_disconnect(event.peer, 0);
                }
            }
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            {
                Player* player = (Player*)event.peer->data;
                if (!player) break;

                int id = player->ID;
                RemotePeers[id] = nullptr;

                player->ID = 0;
                player->Status = Player_None;
                NumPlayers--;

                // todo broadcast updated player list
                // HostUpdatePlayerList();

                printf("disconnected player %d\n", player->ID);
            }
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            {
                if (event.packet->dataLength < 1) break;

                if (event.channelID > Chan_Input0 && event.channelID < Chan_Cmd)
                {
                    ReceiveInputs(event);
                    break;
                }

                if (event.channelID == Chan_Blob)
                {
                    RecvBlob(event.peer, event.packet);
                    break;
                }

                u8* data = (u8*)event.packet->data;
                switch (data[0])
                {
                case Cmd_PlayerInfo: // client sending player info
                    {
                        if (event.packet->dataLength != (9+sizeof(Player))) break;

                        u32 magic = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
                        u32 version = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
                        if ((magic != kNetplayMagic) || (version != kProtocolVersion))
                        {
                            enet_peer_disconnect(event.peer, 0);
                            break;
                        }

                        Player player;
                        memcpy(&player, &data[9], sizeof(Player));
                        player.Name[31] = '\0';

                        Player* hostside = (Player*)event.peer->data;
                        if (player.ID != hostside->ID)
                        {
                            enet_peer_disconnect(event.peer, 0);
                            break;
                        }

                        Platform::Mutex_Lock(PlayersMutex);

                        player.Status = Player_Client;
                        player.Address = event.peer->address.host;
                        player.Port = event.peer->address.port;
                        memcpy(&Players[player.ID], &player, sizeof(Player));

                        Platform::Mutex_Unlock(PlayersMutex);

                        printf("new player list!\n");
                        for (int i = 0; i < NumPlayers; ++i)
                        {
                            Player& player = Players[i];
                            if (player.Status == Player_None) continue;

                            printf("%s: %d, %d, address: %d port: %d\n", player.Name, player.ID, i, player.Address, player.Port);
                        }

                        // broadcast updated player list
                        u8 cmd[2+sizeof(Players)];
                        cmd[0] = Cmd_PlayerList;
                        cmd[1] = (u8)NumPlayers;
                        memcpy(&cmd[2], Players, sizeof(Players));
                        ENetPacket* pkt = enet_packet_create(cmd, 2+sizeof(Players), ENET_PACKET_FLAG_RELIABLE);
                        enet_host_broadcast(Host, Chan_Cmd, pkt);

                        //netplayDlg->updatePlayerList(Players, NumPlayers);
                    }
                    break;
                }
            }
            break;
        }
    }
    Platform::Mutex_Unlock(NetworkMutex);
}

void Netplay::ProcessClient()
{
    if (!Host) return;

    ENetEvent event;
    while (enet_host_service(Host, &event, 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            {
                // another client is establishing a direct connection to us

                int playerid = -1;
                for (int i = 0; i < 16; i++)
                {
                    Player* player = &Players[i];
                    if (i == MyPlayer.ID) continue;
                    if (player->Status != Player_Client) continue;

                    if (player->Address == event.peer->address.host)
                    {
                        playerid = i;
                        break;
                    }
                }

                if (playerid < 0)
                {
                    enet_peer_disconnect(event.peer, 0);
                    break;
                }

                PortToPlayerIndex[event.peer->address.port] = playerid;
                RemotePeers[playerid] = event.peer;
                event.peer->data = &Players[playerid];

                printf("connected to peer %d, from %08X, port %d, event.data: %d\n", playerid, event.peer->address.host, event.peer->address.port, event.data);
            }
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            {
                // TODO
                printf("shma\n");
            }
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            {
                if (event.packet->dataLength < 1) break;

                if (event.channelID > Chan_Input0 && event.channelID < Chan_Cmd)
                {
                    ReceiveInputs(event);
                    break;
                }

                if (event.channelID == Chan_Blob)
                {
                    RecvBlob(event.peer, event.packet);
                    break;
                }

                u8* data = (u8*)event.packet->data;
                switch (data[0])
                {
                case Cmd_PlayerList: // host sending player list
                    {
                        if (event.packet->dataLength != (2+sizeof(Players))) break;
                        if (data[1] > 16) break;
                        printf("client: receive player list, %08X %d\n", event.peer->address.host, event.peer->address.port);
                        Platform::Mutex_Lock(PlayersMutex);

                        NumPlayers = data[1];
                        memcpy(Players, &data[2], sizeof(Players));
                        for (int i = 0; i < 16; i++)
                        {
                            Players[i].Name[31] = '\0';
                        }

                        Platform::Mutex_Unlock(PlayersMutex);

                        // establish connections to any new clients
                        for (int i = 0; i < 16; i++)
                        {
                            Player* player = &Players[i];
                            if (i == MyPlayer.ID) continue;
                            if (player->Status != Player_Client) continue;

                            if (!RemotePeers[i])
                            {
                                ENetAddress peeraddr;
                                peeraddr.host = player->Address;
                                peeraddr.port = player->Port;
                                ENetPeer* peer = enet_host_connect(Host, &peeraddr, Chan_Max, 0);
                                if (!peer)
                                {
                                    // TODO deal with this
                                    continue;
                                }
                            }
                        }
                    }
                    break;

                case Cmd_StartGame: // start game
                    {
                        StartLocal();
                    }
                    break;
                case Cmd_UpdateSettings:
                    {
                        memcpy(&Settings, &data[1], sizeof(NetworkSettings));
                    }
                }
            }
            break;
        }
    }
}

void Netplay::ProcessFrame(int inst)
{
    if (inst == 0)
    {
        if (IsHost)
        {
            ProcessHost();
        }
        else
        {
            ProcessClient();
        }
    }
}

Netplay::InputFrame *Netplay::GetInputFrame(u16 playerID, u32 frameNum)
{
    if (playerID >= 16) return nullptr;

    auto &playerHistory = InputHistory[playerID];

    auto it = playerHistory.find(frameNum);
    return (it != playerHistory.end()) ? &it->second : nullptr;
}

void Netplay::ReceiveInputs(ENetEvent &event)
{
    Platform::Mutex_Lock(InstanceMutex);

    int index = GetPlayerIndexFromPort(event.peer->address.port);
    if (index == -1) return;

    u8* data = (u8*)event.packet->data;
    size_t dataSize = event.packet->dataLength;

    const InputReport* report = reinterpret_cast<const InputReport*>(data);

    u8 stallFrame = report->stallFrame;
    u32 latestFrame = ntohl(report->frameIndex);
    u32 lastCompleteFrame = ntohl(report->lastCompleteFrame);

    // register the last frame this player has completed
    Platform::Mutex_Lock(PlayersMutex);
    Player &player = Players[index];
    player.LastCompletedFrame = lastCompleteFrame;
    Platform::Mutex_Unlock(PlayersMutex);

    const u8* ptr = data + sizeof(InputReport);
    size_t remaining = dataSize - sizeof(InputReport);
    size_t entryCount = remaining / sizeof(InputFrame);

    auto &playerHistory = InputHistory[1];
    playerHistory.clear();
    for (size_t i = 0; i < entryCount; ++i) {
        const InputFrame* netFrame = reinterpret_cast<const InputFrame*>(ptr);

        InputFrame frame;
        frame.FrameNum = ntohl(netFrame->FrameNum);
        frame.KeyMask  = netFrame->KeyMask;
        frame.Touching = netFrame->Touching;
        frame.TouchX   = netFrame->TouchX;
        frame.TouchY   = netFrame->TouchY;

        playerHistory[frame.FrameNum] = frame;

        ptr += sizeof(InputFrame);
    }
    if (playerHistory.find(PendingFrame.FrameNum) == playerHistory.end())
    {
        printf("got an update frame from the other player, but it didn't have the frame we wanted!\n");
    }

    // check if we have the frames we were missing before
    if (PendingFrame.Active)
    {
        auto &playerHistory = InputHistory[1];
        auto it = playerHistory.find(PendingFrame.FrameNum);
        if (it != playerHistory.end())
        {
            u32 prevWaitFrame = PendingFrame.FrameNum;
            u32 currFrame = nds->NumFrames;
            PendingFrame.Active = false;

            // load the save state
            nds->DoSavestate(&PendingFrame.SavestateBuffer[0]);
            nds->NumFrames = PendingFrame.FrameNum;
            u32 startF = nds->NumFrames;

            u32 iterations = currFrame - nds->NumFrames;

            std::clock_t start = std::clock();
            std::clock_t loopStart = std::clock();
            std::clock_t end;

            // iterate over the frames until we reach the point we were at before
            for (u32 i = nds->NumFrames; i < currFrame; ++i)
            {
                if (it != playerHistory.end())
                {
                    PendingFrame.FrameNum = i; // make sure this is our "last completed frame"
                    InputFrame& frameData = it->second;
                    nds->SetKeyMask(frameData.KeyMask);
                    if (frameData.Touching) nds->TouchScreen(frameData.TouchX, frameData.TouchY);
                    else nds->ReleaseScreen();
                    ++it;
                }
                else if (!PendingFrame.Active)
                {
                    // there's still some frames we didn't get
                    PendingFrame.Active = true;
                    PendingFrame.FrameNum = i;
                    PendingFrame.SavestateBuffer[0].~Savestate();
                    new (&PendingFrame.SavestateBuffer[0]) Savestate();
                    nds->DoSavestate(&PendingFrame.SavestateBuffer[0]);
                }
                else
                {
                    nds->SetKeyMask(0xFFF);
                    nds->ReleaseScreen();
                }
                if (i == startF) {
                    start = std::clock();
                }
                nds->RunFrame();
                if (i == startF) {
                    end = std::clock();
                }
            }

            // end = std::clock();
            double elapsed_seconds = double(end - start) / CLOCKS_PER_SEC;
            double loop_elapsed_seconds = double(end - loopStart) / CLOCKS_PER_SEC;

            printf("managed to catch up %d frames. still missing %d, seconds: %lf, loop sec: %1f the size is %d, iterations %d\n",
PendingFrame.FrameNum - prevWaitFrame, nds->NumFrames - PendingFrame.FrameNum, elapsed_seconds, loop_elapsed_seconds, playerHistory.size(), iterations);
        }
    }

    ReceivedInputThisFrame[index] = true;
    printf("got input from %d\n", index);

    Platform::Mutex_Unlock(InstanceMutex);
}

void Netplay::ProcessInput(int netplayID, NDS *nds, u32 inputMask, bool isTouching, u16 touchX, u16 touchY)
{
    StallFrame = false;

    InputFrame frame;
    frame.FrameNum = nds->NumFrames + Settings.Delay;
    frame.KeyMask = inputMask;
    frame.Touching = isTouching;
    frame.TouchX = touchX;
    frame.TouchY = touchY;

    Platform::Mutex_Lock(InstanceMutex);
    InputHistory[0][frame.FrameNum] = frame;
    Platform::Mutex_Unlock(InstanceMutex);

    // Send the inputs to other players
    {
        size_t packetSize = sizeof(InputReport) + InputHistory[0].size() * sizeof(InputFrame);

        std::vector<u8> buffer(packetSize);
        u8* ptr = buffer.data();

        InputReport report = {
            .stallFrame = false,
            .frameIndex = htonl(nds->NumFrames),
            .lastCompleteFrame = htonl(PendingFrame.FrameNum - 1),
        };

        std::memcpy(ptr, &report, sizeof(report));
        ptr += sizeof(report);

        Platform::Mutex_Lock(InstanceMutex);
        for (auto& pair : InputHistory[0]) {
            InputFrame tmp = pair.second;
            tmp.FrameNum = htonl(tmp.FrameNum);
            if (((ptr - buffer.data()) + sizeof(InputFrame)) > buffer.size()) {
                printf("did the history size change? %d, %d\n", InputHistory[0].size(), buffer.size());
                assert(false);
                break;
            }
            std::memcpy(ptr, &tmp, sizeof(InputFrame));
            ptr += sizeof(InputFrame);
        }
        Platform::Mutex_Unlock(InstanceMutex);

        ENetPacket* pkt = enet_packet_create(buffer.data(), buffer.size(), ENET_PACKET_FLAG_UNSEQUENCED);
        enet_host_broadcast(Host, 2, pkt);
    }

    // if we have no inputs, or the only inputs we have are for frames behind us
    // save a state of this frame before it's played so that we can come back here
    // later when we know what the inputs were and replay this frame.
    // also find the last completed frame of all players
    // in other words, the most recent frame that everyone has everyone's inputs for
    int lastCompletedFrame = -1;
    for (int i = 0; i < NumPlayers; ++i)
    {
        if (i == MyPlayer.ID) continue;
        Player &player = Players[i];
        if (player.Status != Player_Client && player.Status != Player_Host) continue;

        if (!ReceivedInputThisFrame[i])
        {
            StallFrame = true;
        }
        if (lastCompletedFrame == -1 || player.LastCompletedFrame < lastCompletedFrame)
        {
            lastCompletedFrame = player.LastCompletedFrame;
        }
    }

    // delete all the frames that everyone already have
    // to save memory and keep packet sizes down
    if (lastCompletedFrame != -1)
    {
        Platform::Mutex_Lock(InstanceMutex);
        auto& playerHistory = InputHistory[0];
        auto cutoff = playerHistory.lower_bound(static_cast<unsigned>(lastCompletedFrame));
        playerHistory.erase(playerHistory.begin(), cutoff);
        Platform::Mutex_Unlock(InstanceMutex);
    }

    // check if this frame has no inputs available
    InputFrame *tempFrame = GetInputFrame(1, nds->NumFrames);
    if ((StallFrame || !tempFrame) && nds->NumFrames > Settings.Delay && !PendingFrame.Active)
    {
        Platform::Mutex_Lock(InstanceMutex);
        printf("missing inputs! saving state... inputs: %d, stall: %d\n", !tempFrame, StallFrame);
        PendingFrame.Active = true;
        PendingFrame.SavestateBuffer[0].~Savestate();
        new (&PendingFrame.SavestateBuffer[0]) Savestate();
        nds->DoSavestate(&PendingFrame.SavestateBuffer[0]);
        StallFrame = false;
        Platform::Mutex_Unlock(InstanceMutex);
    }
}

void Netplay::ApplyInput(int netplayID, NDS *nds)
{
    // clear inputs in case we return
    nds->SetKeyMask(0xFFF);
    nds->ReleaseScreen();

    Platform::Mutex_Lock(InstanceMutex);
    InputFrame *frame = GetInputFrame(1, nds->NumFrames);
    if (!frame)
    {
        // This isn't an error, apply our own inputs now, we can use save states
        // to apply the missing frame later, when we receive it.
        printf("didn't get any inputs from the other player for this frame %d, but we're waiting for %d\n", nds->NumFrames, PendingFrame.FrameNum);
        frame = GetInputFrame(0, nds->NumFrames);
        if (!frame) { Platform::Mutex_Unlock(InstanceMutex); return; }
        InputFrame frameCopy = *frame;
        frame = &frameCopy;
        Platform::Mutex_Unlock(InstanceMutex);
        nds->SetKeyMask(frame->KeyMask);
        if (frame->Touching) nds->TouchScreen(frame->TouchX, frame->TouchY);
        else                 nds->ReleaseScreen();
        return;
    }
    InputFrame frameCopy = *frame;
    frame = &frameCopy;
    Platform::Mutex_Unlock(InstanceMutex);

    if (frame->FrameNum != nds->NumFrames)
    {
        return; // this is not allowed (but shouldn't be possible with the lookup)
    }

    // merge the other player's inputs with this instance inputs
    Platform::Mutex_Lock(InstanceMutex);
    InputFrame *personalFrame = GetInputFrame(0, nds->NumFrames);
    if (personalFrame && frame->KeyMask == 0xFFF && !frame->Touching) {
        frame->KeyMask = personalFrame->KeyMask;
        frame->Touching = personalFrame->Touching;
        frame->TouchX = personalFrame->TouchX;
        frame->TouchY = personalFrame->TouchY;
    }
    Platform::Mutex_Unlock(InstanceMutex);

    printf("applying client inputs %d, size of inputs: %d, frame num %d, nds num %d, delay %d, touching %d, keymask %03X, merged %d\n",
        netplayID, InputHistory[1].size(), frame->FrameNum, nds->NumFrames, Settings.Delay, frame->Touching, frame->KeyMask, personalFrame);

    PendingFrame.FrameNum = nds->NumFrames;

    // apply this input frame
    nds->SetKeyMask(frame->KeyMask);
    if (frame->Touching) nds->TouchScreen(frame->TouchX, frame->TouchY);
    else                 nds->ReleaseScreen();
}

void Netplay::Process(int inst)
{
    ProcessFrame(inst);
    LocalMP::Process(inst);

    static u8 FrameCount = 0;
    FrameCount++;
    if (FrameCount >= 60)
    {
        FrameCount = 0;

        Platform::Mutex_Lock(PlayersMutex);

        for (int i = 0; i < 16; i++)
        {
            if (Players[i].Status == Player_None) continue;
            if (i == MyPlayer.ID) continue;
            if (!RemotePeers[i]) continue;

            Players[i].Ping = RemotePeers[i]->roundTripTime;
        }

        Platform::Mutex_Unlock(PlayersMutex);
    }
}

}
