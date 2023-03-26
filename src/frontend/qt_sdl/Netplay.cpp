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
#include <QProcess>

#include "NDS.h"
#include "main.h"
#include "IPC.h"
#include "Netplay.h"
#include "Input.h"
#include "ROMManager.h"

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

    // TODO: remove IP column in final product

    const QStringList header = {"#", "Player", "Status", "Ping", "IP"};
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

        char ip[32];
        u32 addr = player->Address;
        sprintf(ip, "%d.%d.%d.%d", addr&0xFF, (addr>>8)&0xFF, (addr>>16)&0xFF, addr>>24);
        model->setItem(i, 4, new QStandardItem(ip));
    }
}


namespace Netplay
{

bool Active;
bool IsHost;
bool IsMirror;

ENetHost* Host;
ENetHost* MirrorHost;

Player Players[16];
int NumPlayers;

Player MyPlayer;
u32 HostAddress;
bool Lag;

struct InputFrame
{
    u32 FrameNum;
    u32 KeyMask;
};

std::queue<InputFrame> InputQueue;


bool Init()
{
    Active = false;
    IsHost = false;
    IsMirror = false;
    Host = nullptr;
    MirrorHost = nullptr;
    Lag = false;

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
    // TODO: cleanup resources properly!!

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
    player->Address = 0x0100007F;
    NumPlayers = 1;
    memcpy(&MyPlayer, player, sizeof(Player));

    HostAddress = 0x0100007F;

    ENetAddress mirroraddr;
    mirroraddr.host = ENET_HOST_ANY;
    mirroraddr.port = port + 1;
printf("host mirror host connecting to %08X:%d\n", mirroraddr.host, mirroraddr.port);
    MirrorHost = enet_host_create(&mirroraddr, 16, 1, 0, 0);
    if (!MirrorHost)
    {
        printf("mirror host shat itself :(\n");
        return;
    }

    Active = true;
    IsHost = true;
    IsMirror = false;

    netplayDlg->updatePlayerList(Players, NumPlayers);
}

void StartClient(const char* playername, const char* host, int port)
{
    Host = enet_host_create(nullptr, 1, 1, 0, 0);
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

    Player* player = &MyPlayer;
    memset(player, 0, sizeof(Player));
    player->ID = 0;
    strncpy(player->Name, playername, 31);
    player->Status = 3;

    HostAddress = addr.host;

    Active = true;
    IsHost = false;
    IsMirror = false;
}

void StartMirror(const Player* player)
{
    MirrorHost = enet_host_create(nullptr, 1, 1, 0, 0);
    if (!MirrorHost)
    {
        printf("mirror shat itself :(\n");
        return;
    }

    printf("mirror created, connecting\n");

    ENetAddress addr;
    addr.host = player->Address;
    addr.port = 8064+1 + player->ID; // FIXME!!!!!!!!!!
    printf("mirror client connecting to %08X:%d\n", addr.host, addr.port);
    ENetPeer* peer = enet_host_connect(MirrorHost, &addr, 1, 0);
    if (!peer)
    {
        printf("connect shat itself :(\n");
        return;
    }

    ENetEvent event;
    bool conn = false;
    if (enet_host_service(MirrorHost, &event, 5000) > 0)
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

    memcpy(&MyPlayer, player, sizeof(Player));

    HostAddress = addr.host;

    Active = true;
    IsHost = false;
    IsMirror = true;
}


u32 PlayerAddress(int id)
{
    if (id < 0 || id > 16) return 0;

    u32 ret = Players[id].Address;
    if (ret == 0x0100007F) ret = HostAddress;
    return ret;
}


bool SpawnMirrorInstance(Player player)
{
    u16 curmask = IPC::GetInstanceBitmask();

    QProcess newinst;
    newinst.setProgram(QApplication::applicationFilePath());
    newinst.setArguments(QApplication::arguments().mid(1, QApplication::arguments().length()-1));

#ifdef __WIN32__
    newinst.setCreateProcessArgumentsModifier([] (QProcess::CreateProcessArguments *args)
    {
        args->flags |= CREATE_NEW_CONSOLE;
    });
#endif

    if (!newinst.startDetached())
        return false;

    // try to determine the ID of the new instance

    int newid = -1;
    for (int tries = 0; tries < 10; tries++)
    {
        QThread::usleep(100 * 1000);

        u16 newmask = IPC::GetInstanceBitmask();
        if (newmask == curmask) continue;

        newmask &= ~curmask;
        for (int id = 0; id < 32; id++)
        {
            if (newmask & (1 << id))
            {
                newid = id;
                break;
            }
        }
    }

    if (newid == -1) return false;

    // setup that instance
    printf("netplay: spawned mirror instance for player %d with ID %d, configuring\n", player.ID, newid);

    std::string rompath = ROMManager::FullROMPath.join('|').toStdString();
    IPC::SendCommandStr(1<<newid, IPC::Cmd_LoadROM, rompath);

    if (player.Address == 0x0100007F) player.Address = HostAddress;
    IPC::SendCommand(1<<newid, IPC::Cmd_SetupNetplayMirror, sizeof(Player), &player);

    return true;
}

void StartGame()
{
    if (!IsHost)
    {
        printf("?????\n");
        return;
    }

    // spawn mirror instances as needed
    for (int i = 1; i < NumPlayers; i++)
    {
        SpawnMirrorInstance(Players[i]);
    }

    // tell remote peers to start game
    u8 cmd[1] = {0x04};
    ENetPacket* pkt = enet_packet_create(cmd, sizeof(cmd), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, 0, pkt);

    // tell other mirror instances to start the game
    IPC::SendCommand(0xFFFF, IPC::Cmd_Start, 0, nullptr);

    // start game locally
    NDS::Start();
    emuThread->emuRun();
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
                    Players[id].Address = event.peer->address.host;
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
                        player.Address = event.peer->address.host;
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
    if (!Host) return;

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

                        // create mirror host
                        ENetAddress mirroraddr;
                        mirroraddr.host = ENET_HOST_ANY;
                        mirroraddr.port = 8064+1 + data[1]; // FIXME!!!!
printf("client mirror host connecting to %08X:%d\n", mirroraddr.host, mirroraddr.port);
                        MirrorHost = enet_host_create(&mirroraddr, 16, 1, 0, 0);
                        if (!MirrorHost)
                        {
                            printf("mirror host shat itself :(\n");
                            break;
                        }

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

                        netplayDlg->updatePlayerList(Players, NumPlayers);
                    }
                    break;

                case 0x04: // start game
                    {
                        // spawn mirror instances as needed
                        for (int i = 0; i < NumPlayers; i++)
                        {
                            if (i != MyPlayer.ID)
                                SpawnMirrorInstance(Players[i]);
                        }
printf("bourf\n");
                        // tell other mirror instances to start the game
                        IPC::SendCommand(0xFFFF, IPC::Cmd_Start, 0, nullptr);
printf("birf\n");
                        // start game locally
                        NDS::Start();printf("barf\n");
                        emuThread->emuRun();printf("burf\n");
                    }
                    break;
                }
            }
            break;
        }
    }
}

void ProcessMirrorHost()
{
    if (!MirrorHost) return;

    bool block = false;
    ENetEvent event;
    while (enet_host_service(MirrorHost, &event, block ? 5000 : 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            printf("schmz\n");
            event.peer->data = (void*)0;
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            {
                // TODO
                printf("shmtt\n");
            }
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            {
                if (event.packet->dataLength != 4) break;
                /*u8* data = (u8*)event.packet->data;

                if (data[0])
                {
                    event.peer->data = (void*)1;
                    block = true;
                }
                else
                {
                    event.peer->data = (void*)0;
                    block = false;

                    for (int i = 0; i < MirrorHost->peerCount; i++)
                    {
                        ENetPeer* peer = &(MirrorHost->peers[i]);
                        if (peer->state != ENET_PEER_STATE_CONNECTED) continue;
                        if (peer->data != (void*)0)
                        {
                            block = true;
                            break;
                        }
                    }
                }*/
                u32 clientframes = *(u32*)event.packet->data;
printf("[SYNC] HOST=%d CLIENT=%d\n", NDS::NumFrames, clientframes);
                if (clientframes < (NDS::NumFrames - 4))
                {
                    event.peer->data = (void*)1;
                    block = true;
                }
                else
                {
                    event.peer->data = (void*)0;
                    block = false;

                    for (int i = 0; i < MirrorHost->peerCount; i++)
                    {
                        ENetPeer* peer = &(MirrorHost->peers[i]);
                        if (peer->state != ENET_PEER_STATE_CONNECTED) continue;
                        if (peer->data != (void*)0)
                        {
                            block = true;
                            break;
                        }
                    }
                }
            }
            break;
        }
    }
}

void ProcessMirrorClient()
{
    if (!MirrorHost) return;

    bool block = false;
    if (emuThread->emuIsRunning() && NDS::NumFrames > 4)
    {
        if (InputQueue.empty())
            block = true;
    }

    ENetEvent event;
    while (enet_host_service(MirrorHost, &event, block ? 5000 : 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            printf("schmu\n");
            Lag = false;
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            {
                // TODO
                printf("shmz\n");
            }
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            {
                if (event.packet->dataLength != sizeof(InputFrame)) break;

                u8* data = (u8*)event.packet->data;
                InputFrame frame;
                memcpy(&frame, data, sizeof(InputFrame));
                InputQueue.push(frame);

                /*bool lag = (InputQueue.size() > 4*2);
                if (lag != Lag)
                {
                    // let the mirror host know they are running too fast for us
printf("mirror client lag notify: %d\n", lag);
                    u8 data = lag ? 1 : 0;
                    ENetPacket* pkt = enet_packet_create(&data, 1, ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, pkt);

                    Lag = lag;
                }*/
                {
                    ENetPacket* pkt = enet_packet_create(&NDS::NumFrames, 4, ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, pkt);
                    enet_host_flush(MirrorHost);
                }
            }
            break;
        }

        if (block) break;
    }
}

void ProcessFrame()
{
    if (IsMirror)
    {
        ProcessMirrorClient();
    }
    else
    {
        if (IsHost)
        {
            ProcessHost();
        }
        else
        {
            ProcessClient();
        }

        ProcessMirrorHost();
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

    if (!IsMirror)
    {
        u32 lag = 4; // TODO: make configurable!!

        InputFrame frame;
        frame.FrameNum = NDS::NumFrames + lag;
        frame.KeyMask = Input::InputMask;
        // TODO: touchscreen input and other shit!

        InputQueue.push(frame);

        u8 cmd[sizeof(InputFrame)];
        memcpy(cmd, &frame, sizeof(InputFrame));
        ENetPacket* pkt = enet_packet_create(cmd, sizeof(cmd), ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(MirrorHost, 0, pkt);
        enet_host_flush(MirrorHost);
    }

    if (InputQueue.empty())
    {
        if (NDS::NumFrames > 4)
            printf("Netplay: BAD! INPUT QUEUE EMPTY\n");
        return;
    }

    InputFrame& frame = InputQueue.front();

    if (frame.FrameNum < NDS::NumFrames)
    {
        // TODO: this situation is a desync
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
    if (frame.KeyMask != 0xFFF) printf("[%08d] INPUT=%08X (%08d) (backlog=%d)\n", NDS::NumFrames, frame.KeyMask, frame.FrameNum, InputQueue.size());
    NDS::SetKeyMask(frame.KeyMask);
    InputQueue.pop();
}

}
