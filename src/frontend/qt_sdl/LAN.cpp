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

#include <enet/enet.h>

#include <QStandardItemModel>

#include "LAN.h"
#include "Config.h"
#include "main.h"

#include "ui_LANStartHostDialog.h"
#include "ui_LANStartClientDialog.h"
#include "ui_LANDialog.h"


extern EmuThread* emuThread;
LANDialog* lanDlg;


LANStartHostDialog::LANStartHostDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANStartHostDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    // TODO: remember the last setting? so this doesn't suck massively
    // we could also remember the player name (and auto-init it from the firmware name or whatever)
    ui->sbNumPlayers->setRange(2, 16);
    ui->sbNumPlayers->setValue(16);
}

LANStartHostDialog::~LANStartHostDialog()
{
    delete ui;
}

void LANStartHostDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        std::string player = ui->txtPlayerName->text().toStdString();
        int numplayers = ui->sbNumPlayers->value();

        // TODO validate input!!

        lanDlg = LANDialog::openDlg(parentWidget());

        LAN::StartHost(player.c_str(), numplayers);
    }

    QDialog::done(r);
}


LANStartClientDialog::LANStartClientDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANStartClientDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
}

LANStartClientDialog::~LANStartClientDialog()
{
    delete ui;
}

void LANStartClientDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        std::string player = ui->txtPlayerName->text().toStdString();
        std::string host = ui->txtIPAddress->text().toStdString();

        // TODO validate input!!

        lanDlg = LANDialog::openDlg(parentWidget());

        LAN::StartClient(player.c_str(), host.c_str());
    }

    QDialog::done(r);
}


LANDialog::LANDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvPlayerList->setModel(model);

    connect(this, &LANDialog::sgUpdatePlayerList, this, &LANDialog::doUpdatePlayerList);
}

LANDialog::~LANDialog()
{
    delete ui;
}

void LANDialog::done(int r)
{
    // ???

    QDialog::done(r);
}

void LANDialog::updatePlayerList(LAN::Player* players, int num)
{
    emit sgUpdatePlayerList(players, num);
}

void LANDialog::doUpdatePlayerList(LAN::Player* players, int num)
{
    QStandardItemModel* model = (QStandardItemModel*)ui->tvPlayerList->model();

    model->clear();
    model->setRowCount(num);

    // TODO: remove IP column in final product

    const QStringList header = {"#", "Player", "Status", "Ping", "IP"};
    model->setHorizontalHeaderLabels(header);

    for (int i = 0; i < num; i++)
    {
        LAN::Player* player = &players[i];

        QString id = QString("%0").arg(player->ID+1);
        model->setItem(i, 0, new QStandardItem(id));

        QString name = player->Name;
        model->setItem(i, 1, new QStandardItem(name));

        QString status;
        switch (player->Status)
        {
        case 1: status = ""; break;
        case 2: status = "Host"; break;
        default: status = "ded"; break;
        }
        model->setItem(i, 2, new QStandardItem(status));

        // TODO: ping
        model->setItem(i, 3, new QStandardItem("x"));

        char ip[32];
        u32 addr = player->Address;
        sprintf(ip, "%d.%d.%d.%d", addr&0xFF, (addr>>8)&0xFF, (addr>>16)&0xFF, addr>>24);
        model->setItem(i, 4, new QStandardItem(ip));
    }
}


namespace LAN
{

const int kLANPort = 7064;

bool Active;
bool IsHost;

ENetHost* Host;
ENetPeer* RemotePeers[16];

Player Players[16];
int NumPlayers;
int MaxPlayers;

Player MyPlayer;
u32 HostAddress;
bool Lag;


bool Init()
{
    Active = false;
    IsHost = false;
    Host = nullptr;
    Lag = false;

    memset(RemotePeers, 0, sizeof(RemotePeers));
    memset(Players, 0, sizeof(Players));
    NumPlayers = 0;
    MaxPlayers = 0;

    // TODO we init enet here but also in Netplay
    // that is redundant
    if (enet_initialize() != 0)
    {
        printf("enet shat itself :(\n");
        return false;
    }

    printf("enet init OK\n");
    return true;
}

void DeInit()
{
    // TODO: cleanup resources properly!!

    enet_deinitialize();
}


void StartHost(const char* playername, int numplayers)
{
    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = kLANPort;

    Host = enet_host_create(&addr, 16, 1, 0, 0);
    if (!Host)
    {
        // TODO handle this gracefully
        printf("host shat itself :(\n");
        return;
    }

    Player* player = &Players[0];
    memset(player, 0, sizeof(Player));
    player->ID = 0;
    strncpy(player->Name, playername, 31);
    player->Status = 2;
    player->Address = 0x0100007F;
    NumPlayers = 1;
    MaxPlayers = numplayers;
    memcpy(&MyPlayer, player, sizeof(Player));

    HostAddress = 0x0100007F;

    Active = true;
    IsHost = true;

    lanDlg->updatePlayerList(Players, NumPlayers);
}

void StartClient(const char* playername, const char* host)
{
    Host = enet_host_create(nullptr, 16, 1, 0, 0);
    if (!Host)
    {
        // TODO handle this gracefully
        printf("client shat itself :(\n");
        return;
    }

    printf("client created, connecting (%s, %s:%d)\n", playername, host, kLANPort);

    ENetAddress addr;
    enet_address_set_host(&addr, host);
    addr.port = kLANPort;
    ENetPeer* peer = enet_host_connect(Host, &addr, 1, 0);
    if (!peer)
    {
        printf("connect shat itself :(\n");
        return;
    }

    ENetEvent event;
    bool conn = false;
    if (enet_host_service(Host, &event, 5000) > 0)
    {
        if (event.type == ENET_EVENT_TYPE_CONNECT)
        {
            printf("connected!\n");
            conn = true;
        }
    }

    if (!conn)
    {
        printf("connection failed\n");
        enet_peer_reset(peer);
        return;
    }

    Player* player = &MyPlayer;
    memset(player, 0, sizeof(Player));
    player->ID = 0;
    strncpy(player->Name, playername, 31);
    player->Status = 3;

    HostAddress = addr.host;

    Active = true;
    IsHost = false;
}


void ProcessHost()
{
    if (!Host) return;

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

                // client connected; assign player number

                int id;
                for (id = 0; id < 16; id++)
                {
                    if (id >= NumPlayers) break;
                    if (Players[id].Status == 0) break;
                }

                if (id < 16)
                {
                    u8 cmd[3];
                    cmd[0] = 0x01;
                    cmd[1] = (u8)id;
                    cmd[2] = MaxPlayers;
                    ENetPacket* pkt = enet_packet_create(cmd, 3, ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, pkt);

                    Players[id].ID = id;
                    Players[id].Status = 3;
                    Players[id].Address = event.peer->address.host;
                    event.peer->data = &Players[id];
                    NumPlayers++;

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
                // TODO
                printf("disco\n");
            }
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            {
                if (event.packet->dataLength < 1) break;

                u8* data = (u8*)event.packet->data;
                switch (data[0])
                {
                case 0x02: // client sending player info
                    {
                        if (event.packet->dataLength != (1+sizeof(Player))) break;

                        Player player;
                        memcpy(&player, &data[1], sizeof(Player));
                        player.Name[31] = '\0';

                        Player* hostside = (Player*)event.peer->data;
                        if (player.ID != hostside->ID)
                        {
                            printf("what??? %d =/= %d\n", player.ID, hostside->ID);
                            // TODO: disconnect
                            break;
                        }

                        player.Status = 1;
                        player.Address = event.peer->address.host;
                        memcpy(hostside, &player, sizeof(Player));

                        // broadcast updated player list
                        u8 cmd[2+sizeof(Players)];
                        cmd[0] = 0x03;
                        cmd[1] = (u8)NumPlayers;
                        memcpy(&cmd[2], Players, sizeof(Players));
                        ENetPacket* pkt = enet_packet_create(cmd, 2+sizeof(Players), ENET_PACKET_FLAG_RELIABLE);
                        enet_host_broadcast(Host, 0, pkt);

                        lanDlg->updatePlayerList(Players, NumPlayers);
                    }
                    break;
                }
            }
            break;
        }
    }
}

void ProcessClient()
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
                    if (player->ID == MyPlayer.ID) continue;
                    if (player->Status != 1) continue;

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

                RemotePeers[playerid] = event.peer;
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

                u8* data = (u8*)event.packet->data;
                switch (data[0])
                {
                case 0x01: // host sending player ID
                    {
                        if (event.packet->dataLength != 3) break;

                        MaxPlayers = data[2];

                        // send player information
                        MyPlayer.ID = data[1];
                        u8 cmd[1+sizeof(Player)];
                        cmd[0] = 0x02;
                        memcpy(&cmd[1], &MyPlayer, sizeof(Player));
                        ENetPacket* pkt = enet_packet_create(cmd, 1+sizeof(Player), ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, 0, pkt);
                    }
                    break;

                case 0x03: // host sending player list
                    {
                        if (event.packet->dataLength != (2+sizeof(Players))) break;
                        if (data[1] > 16) break;

                        NumPlayers = data[1];
                        memcpy(Players, &data[2], sizeof(Players));
                        for (int i = 0; i < 16; i++)
                        {
                            Players[i].Name[31] = '\0';
                        }

                        lanDlg->updatePlayerList(Players, NumPlayers);

                        // establish connections to any new clients
                        for (int i = 0; i < 16; i++)
                        {
                            Player* player = &Players[i];
                            if (player->ID == MyPlayer.ID) continue;
                            if (player->Status != 1) continue;

                            if (!RemotePeers[i])
                            {
                                ENetAddress peeraddr;
                                peeraddr.host = player->Address;
                                peeraddr.port = kLANPort;
                                ENetPeer* peer = enet_host_connect(Host, &peeraddr, 1, 0);
                                if (!peer)
                                {
                                    // TODO deal with this
                                    continue;
                                }
                            }
                        }
                    }
                    break;

                case 0x04: // start game
                    {
                        //
                    }
                    break;
                }
            }
            break;
        }
    }
}

void Process()
{
    if (IsHost)
        ProcessHost();
    else
        ProcessClient();
}


void SetMPRecvTimeout(int timeout)
{
    //MPRecvTimeout = timeout;
}

void MPBegin()
{
    //
}

void MPEnd()
{
    //
}

void SetActive(bool active)
{
    //
}

u16 GetInstanceBitmask()
{
    //
    return 0;
}


int SendMPPacket(u8* packet, int len, u64 timestamp)
{
    return 0;
}

int RecvMPPacket(u8* packet, u64* timestamp)
{
    return 0;
}


int SendMPCmd(u8* packet, int len, u64 timestamp)
{
    return 0;
}

int SendMPReply(u8* packet, int len, u64 timestamp, u16 aid)
{
    return 0;
}

int SendMPAck(u8* packet, int len, u64 timestamp)
{
    return 0;
}

int RecvMPHostPacket(u8* packet, u64* timestamp)
{
    return 0;
}

u16 RecvMPReplies(u8* packets, u64 timestamp, u16 aidmask)
{
    return 0;
}

}
