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
#include <queue>

#include <enet/enet.h>

#include <QStandardItemModel>

#include "NDS.h"
#include "main.h"
#include "Netplay.h"
#include "Input.h"

#include "ui_NetplayStartHostDialog.h"
#include "ui_NetplayStartClientDialog.h"
#include "ui_NetplayDialog.h"


extern EmuThread* emuThread;
NetplayDialog* netplayDlg;


NetplayStartHostDialog::NetplayStartHostDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NetplayStartHostDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->txtPort->setText("8064");
}

NetplayStartHostDialog::~NetplayStartHostDialog()
{
    delete ui;
}

void NetplayStartHostDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        std::string player = ui->txtPlayerName->text().toStdString();
        int port = ui->txtPort->text().toInt();

        // TODO validate input!!

        netplayDlg = NetplayDialog::openDlg(parentWidget());

        Netplay::StartHost(player.c_str(), port);
    }

    QDialog::done(r);
}


NetplayStartClientDialog::NetplayStartClientDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NetplayStartClientDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->txtPort->setText("8064");
}

NetplayStartClientDialog::~NetplayStartClientDialog()
{
    delete ui;
}

void NetplayStartClientDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        std::string player = ui->txtPlayerName->text().toStdString();
        std::string host = ui->txtIPAddress->text().toStdString();
        int port = ui->txtPort->text().toInt();

        // TODO validate input!!

        netplayDlg = NetplayDialog::openDlg(parentWidget());

        Netplay::StartClient(player.c_str(), host.c_str(), port);
    }

    QDialog::done(r);
}


NetplayDialog::NetplayDialog(QWidget* parent) : QDialog(parent), ui(new Ui::NetplayDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvPlayerList->setModel(model);

    connect(this, &NetplayDialog::sgUpdatePlayerList, this, &NetplayDialog::doUpdatePlayerList);
}

NetplayDialog::~NetplayDialog()
{
    delete ui;
}

void NetplayDialog::done(int r)
{
    // ???

    QDialog::done(r);
}

void NetplayDialog::updatePlayerList(Netplay::Player* players, int num)
{
    emit sgUpdatePlayerList(players, num);
}

void NetplayDialog::doUpdatePlayerList(Netplay::Player* players, int num)
{
    QStandardItemModel* model = (QStandardItemModel*)ui->tvPlayerList->model();

    model->clear();
    model->setRowCount(num);

    const QStringList header = {"#", "Player", "Status", "Ping"};
    model->setHorizontalHeaderLabels(header);

    for (int i = 0; i < num; i++)
    {
        Netplay::Player* player = &players[i];

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
    }
}


namespace Netplay
{

bool Active;
bool IsHost;

ENetHost* Host;

Player Players[16];
int NumPlayers;

Player TempPlayer; // holds client player info temporarily

struct InputFrame
{
    u32 FrameNum;
    u32 KeyMask;
};

std::queue<InputFrame> InputQueue;


//


bool Init()
{
    Active = false;
    IsHost = false;
    Host = nullptr;

    memset(Players, 0, sizeof(Players));
    NumPlayers = 0;

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
    enet_deinitialize();
}


void StartHost(const char* playername, int port)
{
    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = port;

    Host = enet_host_create(&addr, 16, 1, 0, 0);
    if (!Host)
    {
        printf("host shat itself :(\n");
        return;
    }

    Player* player = &Players[0];
    memset(player, 0, sizeof(Player));
    player->ID = 0;
    strncpy(player->Name, playername, 31);
    player->Status = 2;
    NumPlayers = 1;

    Active = true;
    IsHost = true;

    netplayDlg->updatePlayerList(Players, NumPlayers);
}

void StartClient(const char* playername, const char* host, int port)
{
    Host = enet_host_create(nullptr, 16, 1, 0, 0);
    if (!Host)
    {
        printf("client shat itself :(\n");
        return;
    }

    printf("client created, connecting (%s, %s:%d)\n", playername, host, port);

    ENetAddress addr;
    enet_address_set_host(&addr, host);
    addr.port = port;
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

    Player* player = &TempPlayer;
    memset(player, 0, sizeof(Player));
    player->ID = 0;
    strncpy(player->Name, playername, 31);
    player->Status = 3;

    Active = true;
    IsHost = false;
}


void StartGame()
{
    if (!IsHost)
    {
        printf("?????\n");
        return;
    }

    // tell remote peers to start game
    u8 cmd[1] = {0x01};
    ENetPacket* pkt = enet_packet_create(cmd, sizeof(cmd), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, 0, pkt);

    // start game locally
    NDS::Start();
    emuThread->emuRun();
}


void ProcessHost()
{
    ENetEvent event;
    while (enet_host_service(Host, &event, 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            {
                // client connected; assign player number

                int id;
                for (id = 0; id < 16; id++)
                {
                    if (id >= NumPlayers) break;
                    if (Players[id].Status == 0) break;
                }

                if (id < 16)
                {
                    u8 cmd[2];
                    cmd[0] = 0x01;
                    cmd[1] = (u8)id;
                    ENetPacket* pkt = enet_packet_create(cmd, 2, ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, pkt);

                    Players[id].ID = id;
                    Players[id].Status = 3;
                    event.peer->data = &Players[id];
                    NumPlayers++;
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
                        memcpy(hostside, &player, sizeof(Player));

                        // broadcast updated player list
                        u8 cmd[2+sizeof(Players)];
                        cmd[0] = 0x03;
                        cmd[1] = (u8)NumPlayers;
                        memcpy(&cmd[2], Players, sizeof(Players));
                        ENetPacket* pkt = enet_packet_create(cmd, 2+sizeof(Players), ENET_PACKET_FLAG_RELIABLE);
                        enet_host_broadcast(Host, 0, pkt);

                        netplayDlg->updatePlayerList(Players, NumPlayers);
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
    ENetEvent event;
    while (enet_host_service(Host, &event, 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            printf("schmo\n");
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
                        if (event.packet->dataLength != 2) break;

                        // send player information
                        TempPlayer.ID = data[1];printf("host assigned ID: %d\n", data[1]);
                        u8 cmd[1+sizeof(Player)];
                        cmd[0] = 0x02;
                        memcpy(&cmd[1], &TempPlayer, sizeof(Player));
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

                        netplayDlg->updatePlayerList(Players, NumPlayers);
                    }
                    break;
                }
            }
            break;
        }
    }
}

void ProcessFrame()
{
    if (IsHost)
    {
        ProcessHost();
    }
    else
    {
        ProcessClient();
    }

    return;
    bool block = false;
    if (emuThread->emuIsRunning())
    {
        if (IsHost)
        {
            // TODO: prevent the clients from running too far behind
        }
        else
        {
            // block if we ran out of input frames
            // TODO: in this situation, make sure we do receive an input frame
            // or if we don't after X time, handle it gracefully

            if (InputQueue.empty())
                block = true;
        }
    }
    block=false;

    ENetEvent event;
    while (enet_host_service(Host, &event, block ? 5000 : 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            printf("client connected %08X %d\n", event.peer->address.host, event.peer->address.port);
            //Peer = event.peer;
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            printf("client disconnected %08X %d\n", event.peer->address.host, event.peer->address.port);
            //Peer = nullptr;
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            {
                if (event.packet->dataLength < 1)
                {
                    printf("?????\n");
                    break;
                }

                u8* data = (u8*)event.packet->data;
                switch (data[0])
                {
                case 0x01: // start game
                    NDS::Start();
                    emuThread->emuRun();
                    break;

                case 0x02: // input frame
                    {
                        if (event.packet->dataLength != (sizeof(InputFrame)+1))
                            break;

                        InputFrame frame;
                        memcpy(&frame, &data[1], sizeof(InputFrame));
                        InputQueue.push(frame);
                        break;
                    }
                }
            }
            break;
        }
    }
}

void ProcessInput()
{
    // netplay input processing
    //
    // N = current frame #
    // L = amount of lag frames
    //
    // host side:
    // we take the current input (normally meant for frame N)
    // and delay it to frame N+L
    //
    // client side:
    // we receive input from the host
    // apply each input to the frame it's assigned to
    // before running a frame, we need to wait to have received input for it
    // TODO: alert host if we are running too far behind

    if (IsHost)
    {
        u32 lag = 4; // TODO: make configurable!!

        InputFrame frame;
        frame.FrameNum = NDS::NumFrames + lag;
        frame.KeyMask = Input::InputMask;
        // TODO: touchscreen input and other shit!

        InputQueue.push(frame);

        u8 cmd[1+sizeof(InputFrame)];
        cmd[0] = 0x02;
        memcpy(&cmd[1], &frame, sizeof(InputFrame));
        ENetPacket* pkt = enet_packet_create(cmd, sizeof(cmd), ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(Host, 0, pkt);
    }

    if (InputQueue.empty())
    {
        printf("Netplay: BAD! INPUT QUEUE EMPTY\n");
        return;
    }

    InputFrame& frame = InputQueue.front();

    if (frame.FrameNum < NDS::NumFrames)
    {
        printf("Netplay: BAD! LAGGING BEHIND\n");
        while (frame.FrameNum < NDS::NumFrames)
        {
            if (InputQueue.size() < 2) break;
            InputQueue.pop();
            frame = InputQueue.front();
        }
    }

    if (frame.FrameNum > NDS::NumFrames)
    {
        // frame in the future, ignore
        return;
    }

    // apply this input frame
    printf("[%08d] INPUT=%08X (%08d) (backlog=%d)\n", NDS::NumFrames, frame.KeyMask, frame.FrameNum, InputQueue.size());
    NDS::SetKeyMask(frame.KeyMask);
    InputQueue.pop();
}

}
