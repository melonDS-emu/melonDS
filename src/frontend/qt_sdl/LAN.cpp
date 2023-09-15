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
#include <vector>

#ifdef __WIN32__
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #define socket_t    SOCKET
    #define sockaddr_t  SOCKADDR
    #define sockaddr_in_t  SOCKADDR_IN
#else
    #include <unistd.h>
    #include <netinet/in.h>
    #include <sys/select.h>
    #include <sys/socket.h>

    #define socket_t    int
    #define sockaddr_t  struct sockaddr
    #define sockaddr_in_t  struct sockaddr_in
    #define closesocket close
#endif

#ifndef INVALID_SOCKET
    #define INVALID_SOCKET  (socket_t)-1
#endif

#include <enet/enet.h>
#include <SDL2/SDL.h>

#include <QStandardItemModel>
#include <QPushButton>
#include <QInputDialog>
#include <QMessageBox>

#include "LAN.h"
#include "Config.h"
#include "main.h"

#include "ui_LANStartHostDialog.h"
#include "ui_LANStartClientDialog.h"
#include "ui_LANDialog.h"


extern EmuThread* emuThread;
LANStartClientDialog* lanClientDlg = nullptr;
LANDialog* lanDlg = nullptr;


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

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvAvailableGames->setModel(model);
    const QStringList listheader = {"Name", "Players", "Status", "Host IP"};
    model->setHorizontalHeaderLabels(listheader);

    connect(ui->tvAvailableGames->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this, SLOT(onGameSelectionChanged(const QItemSelection&, const QItemSelection&)));

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText("Connect");
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    QPushButton* btn = ui->buttonBox->addButton("Direct connect...", QDialogButtonBox::ActionRole);
    connect(btn, SIGNAL(clicked()), this, SLOT(onDirectConnect()));

    connect(this, &LANStartClientDialog::sgUpdateDiscoveryList, this, &LANStartClientDialog::doUpdateDiscoveryList);

    lanClientDlg = this;
    LAN::StartDiscovery();
}

LANStartClientDialog::~LANStartClientDialog()
{
    lanClientDlg = nullptr;
    delete ui;
}

void LANStartClientDialog::onGameSelectionChanged(const QItemSelection& cur, const QItemSelection& prev)
{
    QModelIndexList indlist = cur.indexes();
    if (indlist.count() == 0)
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    else
    {
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
}

void LANStartClientDialog::on_tvAvailableGames_doubleClicked(QModelIndex index)
{
    done(QDialog::Accepted);
}

void LANStartClientDialog::onDirectConnect()
{
    if (ui->txtPlayerName->text().trimmed().isEmpty())
    {
        QMessageBox::warning(this, "melonDS", "Please enter a player name before connecting.");
        return;
    }

    QString host = QInputDialog::getText(this, "Direct connect", "Host address:");
    if (host.isEmpty()) return;

    std::string hostname = host.toStdString();
    std::string player = ui->txtPlayerName->text().toStdString();

    setEnabled(false);
    LAN::EndDiscovery();
    if (!LAN::StartClient(player.c_str(), hostname.c_str()))
    {
        QString msg = QString("Failed to connect to the host %0.").arg(QString::fromStdString(hostname));
        QMessageBox::warning(this, "melonDS", msg);
        setEnabled(true);
        LAN::StartDiscovery();
        return;
    }

    setEnabled(true);
    lanDlg = LANDialog::openDlg(parentWidget());
    QDialog::done(QDialog::Accepted);
}

void LANStartClientDialog::done(int r)
{
    if (r == QDialog::Accepted)
    {
        if (ui->txtPlayerName->text().trimmed().isEmpty())
        {
            QMessageBox::warning(this, "melonDS", "Please enter a player name before connecting.");
            return;
        }

        QModelIndexList indlist = ui->tvAvailableGames->selectionModel()->selectedRows();
        if (indlist.count() == 0) return;

        QStandardItemModel* model = (QStandardItemModel*)ui->tvAvailableGames->model();
        QStandardItem* item = model->item(indlist[0].row());
        u32 addr = item->data().toUInt();
        char hostname[16];
        snprintf(hostname, 16, "%d.%d.%d.%d", (addr>>24), ((addr>>16)&0xFF), ((addr>>8)&0xFF), (addr&0xFF));

        std::string player = ui->txtPlayerName->text().toStdString();

        setEnabled(false);
        LAN::EndDiscovery();
        if (!LAN::StartClient(player.c_str(), hostname))
        {
            QString msg = QString("Failed to connect to the host %0.").arg(QString(hostname));
            QMessageBox::warning(this, "melonDS", msg);
            setEnabled(true);
            LAN::StartDiscovery();
            return;
        }

        setEnabled(true);
        lanDlg = LANDialog::openDlg(parentWidget());
    }
    else
    {
        LAN::EndDiscovery();
    }

    QDialog::done(r);
}

void LANStartClientDialog::updateDiscoveryList()
{
    emit sgUpdateDiscoveryList();
}

void LANStartClientDialog::doUpdateDiscoveryList()
{
    LAN::DiscoveryMutex.lock();

    QStandardItemModel* model = (QStandardItemModel*)ui->tvAvailableGames->model();
    int curcount = model->rowCount();
    int newcount = LAN::DiscoveryList.size();
    if (curcount > newcount)
    {
        model->removeRows(newcount, curcount-newcount);
    }
    else if (curcount < newcount)
    {
        for (int i = curcount; i < newcount; i++)
        {
            QList<QStandardItem*> row;
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            model->appendRow(row);
        }
    }

    int i = 0;
    for (const auto& [key, data] : LAN::DiscoveryList)
    {
        model->item(i, 0)->setText(data.SessionName);
        model->item(i, 0)->setData(QVariant(key));

        QString plcount = QString("%0/%1").arg(data.NumPlayers).arg(data.MaxPlayers);
        model->item(i, 1)->setText(plcount);

        QString status;
        switch (data.Status)
        {
        case 0: status = "Idle"; break;
        case 1: status = "Playing"; break;
        }
        model->item(i, 2)->setText(status);

        QString ip = QString("%0.%1.%2.%3").arg(key>>24).arg((key>>16)&0xFF).arg((key>>8)&0xFF).arg(key&0xFF);
        model->item(i, 3)->setText(ip);

        i++;
    }

    LAN::DiscoveryMutex.unlock();
}


LANDialog::LANDialog(QWidget* parent) : QDialog(parent), ui(new Ui::LANDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);

    QStandardItemModel* model = new QStandardItemModel();
    ui->tvPlayerList->setModel(model);
    const QStringList header = {"#", "Player", "Status", "Ping", "IP"};
    model->setHorizontalHeaderLabels(header);

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

void LANDialog::updatePlayerList()
{
    playerListMutex.lock();
    memcpy(playerList, LAN::Players, sizeof(playerList));
    memcpy(playerPing, LAN::PlayerPing, sizeof(playerPing));
    numPlayers = LAN::NumPlayers;
    maxPlayers = LAN::MaxPlayers;
    myPlayerID = LAN::MyPlayer.ID;
    hostAddress = LAN::HostAddress;
    playerListMutex.unlock();

    emit sgUpdatePlayerList();
}

void LANDialog::doUpdatePlayerList()
{
    playerListMutex.lock();

    QStandardItemModel* model = (QStandardItemModel*)ui->tvPlayerList->model();
    int curcount = model->rowCount();
    int newcount = numPlayers;
    if (curcount > newcount)
    {
        model->removeRows(newcount, curcount-newcount);
    }
    else if (curcount < newcount)
    {
        for (int i = curcount; i < newcount; i++)
        {
            QList<QStandardItem*> row;
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            row.append(new QStandardItem());
            model->appendRow(row);
        }
    }

    for (int i = 0; i < 16; i++)
    {
        LAN::Player* player = &playerList[i];
        if (player->Status == 0) break;

        QString id = QString("%0/%1").arg(player->ID+1).arg(maxPlayers);
        model->item(i, 0)->setText(id);

        QString name = player->Name;
        model->item(i, 1)->setText(name);

        QString status;
        switch (player->Status)
        {
        case 1: status = "Ready"; break;
        case 2: status = "Host"; break;
        case 3: status = "Connecting"; break;
        }
        model->item(i, 2)->setText(status);

        if (i == myPlayerID)
        {
            model->item(i, 3)->setText("-");
            model->item(i, 4)->setText("(local)");
        }
        else
        {
            QString ping = QString("%0 ms").arg(playerPing[i]);
            model->item(i, 3)->setText(ping);

            // note on the player IP display
            // * we make an exception for the host -- the player list is issued by the host, so the host IP would be 127.0.0.1
            // * for the same reason, the host can't know its own IP, so for the current player we force it to 127.0.0.1
            u32 ip;
            if (player->Status == 2)
                ip = hostAddress;
            else
                ip = player->Address;

            QString ips = QString("%0.%1.%2.%3").arg(ip&0xFF).arg((ip>>8)&0xFF).arg((ip>>16)&0xFF).arg(ip>>24);
            model->item(i, 4)->setText(ips);
        }
    }

    playerListMutex.unlock();
}


namespace LAN
{

const u32 kDiscoveryMagic = 0x444E414C; // LAND
const u32 kLANMagic = 0x504E414C; // LANP
const u32 kPacketMagic = 0x4946494E; // NIFI

const u32 kProtocolVersion = 1;

struct MPPacketHeader
{
    u32 Magic;
    u32 SenderID;
    u32 Type;       // 0=regular 1=CMD 2=reply 3=ack
    u32 Length;
    u64 Timestamp;
};

const int kDiscoveryPort = 7063;
const int kLANPort = 7064;

socket_t DiscoverySocket;
u32 DiscoveryLastTick;
std::map<u32, DiscoveryData> DiscoveryList;
QMutex DiscoveryMutex;

bool Active;
bool IsHost;

ENetHost* Host;
ENetPeer* RemotePeers[16];

Player Players[16];
u32 PlayerPing[16];
int NumPlayers;
int MaxPlayers;

u16 ConnectedBitmask;

Player MyPlayer;
u32 HostAddress;
bool Lag;

int MPRecvTimeout;
int LastHostID;
ENetPeer* LastHostPeer;
std::queue<ENetPacket*> RXQueue;

u32 FrameCount;


bool Init()
{
    DiscoverySocket = INVALID_SOCKET;
    DiscoveryLastTick = 0;

    Active = false;
    IsHost = false;
    Host = nullptr;
    Lag = false;

    memset(RemotePeers, 0, sizeof(RemotePeers));
    memset(Players, 0, sizeof(Players));
    memset(PlayerPing, 0, sizeof(PlayerPing));
    NumPlayers = 0;
    MaxPlayers = 0;

    ConnectedBitmask = 0;

    MPRecvTimeout = 25;
    LastHostID = -1;
    LastHostPeer = nullptr;

    FrameCount = 0;

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
    if (DiscoverySocket)
    {
        closesocket(DiscoverySocket);
        DiscoverySocket = INVALID_SOCKET;
    }

    while (!RXQueue.empty())
    {
        ENetPacket* packet = RXQueue.front();
        RXQueue.pop();
        enet_packet_destroy(packet);
    }

    enet_host_destroy(Host);
    Host = nullptr;

    enet_deinitialize();
}


bool StartDiscovery()
{
    int res;

    DiscoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (DiscoverySocket < 0)
    {
        DiscoverySocket = INVALID_SOCKET;
        return false;
    }

    sockaddr_in_t saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(kDiscoveryPort);
    res = bind(DiscoverySocket, (const sockaddr_t*)&saddr, sizeof(saddr));
    if (res < 0)
    {
        closesocket(DiscoverySocket);
        DiscoverySocket = INVALID_SOCKET;
        return false;
    }

    int opt_true = 1;
    res = setsockopt(DiscoverySocket, SOL_SOCKET, SO_BROADCAST, (const char*)&opt_true, sizeof(int));
    if (res < 0)
    {
        closesocket(DiscoverySocket);
        DiscoverySocket = INVALID_SOCKET;
        return false;
    }

    DiscoveryLastTick = SDL_GetTicks();
    DiscoveryList.clear();

    Active = true;
    return true;
}

void EndDiscovery()
{
    if (DiscoverySocket != INVALID_SOCKET)
    {
        closesocket(DiscoverySocket);
        DiscoverySocket = INVALID_SOCKET;
    }

    if (!IsHost)
        Active = false;
}

bool StartHost(const char* playername, int numplayers)
{
    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = kLANPort;

    Host = enet_host_create(&addr, 16, 2, 0, 0);
    if (!Host)
    {
        return false;
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
    LastHostID = -1;
    LastHostPeer = nullptr;

    Active = true;
    IsHost = true;

    if (lanDlg)
        lanDlg->updatePlayerList();

    StartDiscovery();
    return true;
}

bool StartClient(const char* playername, const char* host)
{
    Host = enet_host_create(nullptr, 16, 2, 0, 0);
    if (!Host)
    {
        return false;
    }

    ENetAddress addr;
    enet_address_set_host(&addr, host);
    addr.port = kLANPort;
    ENetPeer* peer = enet_host_connect(Host, &addr, 2, 0);
    if (!peer)
    {
        enet_host_destroy(Host);
        Host = nullptr;
        return false;
    }

    Player* player = &MyPlayer;
    memset(player, 0, sizeof(Player));
    player->ID = 0;
    strncpy(player->Name, playername, 31);
    player->Status = 3;

    ENetEvent event;
    int conn = 0;
    u32 starttick = SDL_GetTicks();
    const int conntimeout = 5000;
    for (;;)
    {
        u32 curtick = SDL_GetTicks();
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
                if (event.channelID != 0) continue;
                if (data[0] != 0x01) continue;
                if (event.packet->dataLength != 11) continue;

                u32 magic = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
                u32 version = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
                if (magic != kLANMagic) continue;
                if (version != kProtocolVersion) continue;

                MaxPlayers = data[10];

                // send player information
                MyPlayer.ID = data[9];
                u8 cmd[9+sizeof(Player)];
                cmd[0] = 0x02;
                cmd[1] = (u8)kLANMagic;
                cmd[2] = (u8)(kLANMagic >> 8);
                cmd[3] = (u8)(kLANMagic >> 16);
                cmd[4] = (u8)(kLANMagic >> 24);
                cmd[5] = (u8)kProtocolVersion;
                cmd[6] = (u8)(kProtocolVersion >> 8);
                cmd[7] = (u8)(kProtocolVersion >> 16);
                cmd[8] = (u8)(kProtocolVersion >> 24);
                memcpy(&cmd[9], &MyPlayer, sizeof(Player));
                ENetPacket* pkt = enet_packet_create(cmd, 9+sizeof(Player), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(event.peer, 0, pkt);

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
    LastHostID = -1;
    LastHostPeer = nullptr;
    RemotePeers[0] = peer;

    Active = true;
    IsHost = false;
    return true;
}


void ProcessDiscovery()
{
    if (DiscoverySocket == INVALID_SOCKET)
        return;

    u32 tick = SDL_GetTicks();
    if ((tick - DiscoveryLastTick) < 1000)
        return;

    DiscoveryLastTick = tick;

    if (IsHost)
    {
        // advertise this LAN session over the network

        DiscoveryData beacon;
        memset(&beacon, 0, sizeof(beacon));
        beacon.Magic = kDiscoveryMagic;
        beacon.Version = kProtocolVersion;
        beacon.Tick = tick;
        snprintf(beacon.SessionName, 64, "%s's game", MyPlayer.Name);
        beacon.NumPlayers = NumPlayers;
        beacon.MaxPlayers = MaxPlayers;
        beacon.Status = 0; // TODO

        sockaddr_in_t saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        saddr.sin_port = htons(kDiscoveryPort);

        sendto(DiscoverySocket, (const char*)&beacon, sizeof(beacon), 0, (const sockaddr_t*)&saddr, sizeof(saddr));
    }
    else
    {
        DiscoveryMutex.lock();

        // listen for LAN sessions

        fd_set fd;
        struct timeval tv;
        for (;;)
        {
            FD_ZERO(&fd); FD_SET(DiscoverySocket, &fd);
            tv.tv_sec = 0; tv.tv_usec = 0;
            if (!select(DiscoverySocket+1, &fd, nullptr, nullptr, &tv))
                break;

            DiscoveryData beacon;
            sockaddr_in_t raddr;
            socklen_t ralen = sizeof(raddr);

            int rlen = recvfrom(DiscoverySocket, (char*)&beacon, sizeof(beacon), 0, (sockaddr_t*)&raddr, &ralen);
            if (rlen < sizeof(beacon)) continue;
            if (beacon.Magic != kDiscoveryMagic) continue;
            if (beacon.Version != kProtocolVersion) continue;
            if (beacon.MaxPlayers > 16) continue;
            if (beacon.NumPlayers > beacon.MaxPlayers) continue;

            u32 key = ntohl(raddr.sin_addr.s_addr);

            if (DiscoveryList.find(key) != DiscoveryList.end())
            {
                if (beacon.Tick <= DiscoveryList[key].Tick)
                    continue;
            }

            beacon.Magic = tick;
            beacon.SessionName[63] = '\0';
            DiscoveryList[key] = beacon;
        }

        // cleanup: remove hosts that haven't given a sign of life in the last 5 seconds

        std::vector<u32> deletelist;

        for (const auto& [key, data] : DiscoveryList)
        {
            u32 age = tick - data.Magic;
            if (age < 5000) continue;

            deletelist.push_back(key);
        }

        for (const auto& key : deletelist)
        {
            DiscoveryList.erase(key);
        }

        DiscoveryMutex.unlock();

        // update the list in the connect dialog if needed

        if (lanClientDlg)
            lanClientDlg->updateDiscoveryList();
    }
}

void HostUpdatePlayerList()
{
    u8 cmd[2+sizeof(Players)];
    cmd[0] = 0x03;
    cmd[1] = (u8)NumPlayers;
    memcpy(&cmd[2], Players, sizeof(Players));
    ENetPacket* pkt = enet_packet_create(cmd, 2+sizeof(Players), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, 0, pkt);

    if (lanDlg)
        lanDlg->updatePlayerList();
}

void ProcessHostEvent(ENetEvent& event)
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
                u8 cmd[11];
                cmd[0] = 0x01;
                cmd[1] = (u8)kLANMagic;
                cmd[2] = (u8)(kLANMagic >> 8);
                cmd[3] = (u8)(kLANMagic >> 16);
                cmd[4] = (u8)(kLANMagic >> 24);
                cmd[5] = (u8)kProtocolVersion;
                cmd[6] = (u8)(kProtocolVersion >> 8);
                cmd[7] = (u8)(kProtocolVersion >> 16);
                cmd[8] = (u8)(kProtocolVersion >> 24);
                cmd[9] = (u8)id;
                cmd[10] = MaxPlayers;
                ENetPacket* pkt = enet_packet_create(cmd, 11, ENET_PACKET_FLAG_RELIABLE);
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
            Player* player = (Player*)event.peer->data;
            if (!player) break;

            int id = player->ID;
            RemotePeers[id] = nullptr;

            player->ID = 0;
            player->Status = 0;
            NumPlayers--;

            // broadcast updated player list
            HostUpdatePlayerList();
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
                    if (event.packet->dataLength != (9+sizeof(Player))) break;

                    u32 magic = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
                    u32 version = data[5] | (data[6] << 8) | (data[7] << 16) | (data[8] << 24);
                    if ((magic != kLANMagic) || (version != kProtocolVersion))
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

                    player.Status = 1;
                    player.Address = event.peer->address.host;
                    memcpy(hostside, &player, sizeof(Player));

                    // broadcast updated player list
                    HostUpdatePlayerList();
                }
                break;

            case 0x04: // player connected
                {
                    if (event.packet->dataLength != 1) break;
                    Player* player = (Player*)event.peer->data;
                    if (!player) break;

                    ConnectedBitmask |= (1 << player->ID);
                }
                break;

            case 0x05: // player disconnected
                {
                    if (event.packet->dataLength != 1) break;
                    Player* player = (Player*)event.peer->data;
                    if (!player) break;

                    ConnectedBitmask &= ~(1 << player->ID);
                }
                break;
            }

            enet_packet_destroy(event.packet);
        }
        break;
    }
}

void ProcessClientEvent(ENetEvent& event)
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
            event.peer->data = &Players[playerid];
        }
        break;

    case ENET_EVENT_TYPE_DISCONNECT:
        {
            Player* player = (Player*)event.peer->data;
            if (!player) break;

            int id = player->ID;
            RemotePeers[id] = nullptr;
        }
        break;

    case ENET_EVENT_TYPE_RECEIVE:
        {
            if (event.packet->dataLength < 1) break;

            u8* data = (u8*)event.packet->data;
            switch (data[0])
            {
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

                    if (lanDlg)
                        lanDlg->updatePlayerList();

                    // establish connections to any new clients
                    for (int i = 0; i < 16; i++)
                    {
                        Player* player = &Players[i];
                        if (i == MyPlayer.ID) continue;
                        if (player->Status != 1) continue;

                        if (!RemotePeers[i])
                        {
                            ENetAddress peeraddr;
                            peeraddr.host = player->Address;
                            peeraddr.port = kLANPort;
                            ENetPeer* peer = enet_host_connect(Host, &peeraddr, 2, 0);
                            if (!peer)
                            {
                                // TODO deal with this
                                continue;
                            }
                        }
                    }
                }
                break;

            case 0x04: // player connected
                {
                    if (event.packet->dataLength != 1) break;
                    Player* player = (Player*)event.peer->data;
                    if (!player) break;

                    ConnectedBitmask |= (1 << player->ID);
                }
                break;

            case 0x05: // player disconnected
                {
                    if (event.packet->dataLength != 1) break;
                    Player* player = (Player*)event.peer->data;
                    if (!player) break;

                    ConnectedBitmask &= ~(1 << player->ID);
                }
                break;
            }

            enet_packet_destroy(event.packet);
        }
        break;
    }
}

void ProcessEvent(ENetEvent& event)
{
    if (IsHost)
        ProcessHostEvent(event);
    else
        ProcessClientEvent(event);
}

void Process(bool block)
{
    if (!Host) return;

    if (!RXQueue.empty()) block = false;

    int timeout = block ? MPRecvTimeout : 0;
    u32 time_last = SDL_GetTicks();

    ENetEvent event;
    while (enet_host_service(Host, &event, timeout) > 0)
    {
        if (event.type == ENET_EVENT_TYPE_RECEIVE && event.channelID == 1)
        {
            event.packet->userData = event.peer;
            RXQueue.push(event.packet);
            if (block) return;
        }
        else
        {
            ProcessEvent(event);
            if (block)
            {
                u32 time = SDL_GetTicks();
                if (time < time_last) return;
                timeout -= (int)(time - time_last);
                if (timeout <= 0) return;
                time_last = time;
            }
        }
    }
}

void ProcessFrame()
{
    ProcessDiscovery();
    Process(false);

    FrameCount++;
    if (FrameCount >= 60)
    {
        FrameCount = 0;

        for (int i = 0; i < 16; i++)
        {
            if (Players[i].Status == 0) continue;
            if (i == MyPlayer.ID) continue;
            if (!RemotePeers[i]) continue;

            PlayerPing[i] = RemotePeers[i]->roundTripTime;
        }

        if (lanDlg)
            lanDlg->updatePlayerList();
    }
}


void SetMPRecvTimeout(int timeout)
{
    MPRecvTimeout = timeout;
}

void MPBegin()
{
    if (!Host) return;

    ConnectedBitmask |= (1 << MyPlayer.ID);
    LastHostID = -1;
    LastHostPeer = nullptr;

    u8 cmd = 0x04;
    ENetPacket* pkt = enet_packet_create(&cmd, 1, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, 0, pkt);
}

void MPEnd()
{
    if (!Host) return;

    ConnectedBitmask &= ~(1 << MyPlayer.ID);

    u8 cmd = 0x05;
    ENetPacket* pkt = enet_packet_create(&cmd, 1, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(Host, 0, pkt);
}


int SendMPPacketGeneric(u32 type, u8* packet, int len, u64 timestamp)
{
    if (!Host) return 0;

    // TODO make the reliable part optional?
    //u32 flags = ENET_PACKET_FLAG_RELIABLE;
    u32 flags = ENET_PACKET_FLAG_UNSEQUENCED;

    ENetPacket* enetpacket = enet_packet_create(nullptr, sizeof(MPPacketHeader)+len, flags);

    MPPacketHeader pktheader;
    pktheader.Magic = 0x4946494E;
    pktheader.SenderID = MyPlayer.ID;
    pktheader.Type = type;
    pktheader.Length = len;
    pktheader.Timestamp = timestamp;
    memcpy(&enetpacket->data[0], &pktheader, sizeof(MPPacketHeader));
    if (len)
        memcpy(&enetpacket->data[sizeof(MPPacketHeader)], packet, len);

    if (((type & 0xFFFF) == 2) && LastHostPeer)
        enet_peer_send(LastHostPeer, 1, enetpacket);
    else
        enet_host_broadcast(Host, 1, enetpacket);
    enet_host_flush(Host);

    return len;
}

int RecvMPPacketGeneric(u8* packet, bool block, u64* timestamp)
{
    if (!Host) return 0;

    Process(block);
    if (RXQueue.empty()) return 0;

    ENetPacket* enetpacket = RXQueue.front();
    RXQueue.pop();
    MPPacketHeader* header = (MPPacketHeader*)&enetpacket->data[0];
    bool good = true;
    if (enetpacket->dataLength < sizeof(MPPacketHeader))
        good = false;
    else if (header->Magic != 0x4946494E)
        good = false;
    else if (header->SenderID == MyPlayer.ID)
        good = false;

    if (!good)
    {
        enet_packet_destroy(enetpacket);
        return 0;
    }

    u32 len = header->Length;
    if (len)
    {
        if (len > 2048) len = 2048;

        memcpy(packet, &enetpacket->data[sizeof(MPPacketHeader)], len);

        if (header->Type == 1)
        {
            LastHostID = header->SenderID;
            LastHostPeer = (ENetPeer*)enetpacket->userData;
        }
    }

    if (timestamp) *timestamp = header->Timestamp;
    enet_packet_destroy(enetpacket);
    return len;
}


int SendMPPacket(u8* packet, int len, u64 timestamp)
{
    return SendMPPacketGeneric(0, packet, len, timestamp);
}

int RecvMPPacket(u8* packet, u64* timestamp)
{
    return RecvMPPacketGeneric(packet, false, timestamp);
}


int SendMPCmd(u8* packet, int len, u64 timestamp)
{
    return SendMPPacketGeneric(1, packet, len, timestamp);
}

int SendMPReply(u8* packet, int len, u64 timestamp, u16 aid)
{
    return SendMPPacketGeneric(2 | (aid<<16), packet, len, timestamp);
}

int SendMPAck(u8* packet, int len, u64 timestamp)
{
    return SendMPPacketGeneric(3, packet, len, timestamp);
}

int RecvMPHostPacket(u8* packet, u64* timestamp)
{
    if (LastHostID != -1)
    {
        // check if the host is still connected

        if (!(ConnectedBitmask & (1<<LastHostID)))
            return -1;
    }

    return RecvMPPacketGeneric(packet, true, timestamp);
}

u16 RecvMPReplies(u8* packets, u64 timestamp, u16 aidmask)
{
    if (!Host) return 0;

    u16 ret = 0;
    u16 myinstmask = 1 << MyPlayer.ID;

    if ((myinstmask & ConnectedBitmask) == ConnectedBitmask)
        return 0;

    for (;;)
    {
        Process(true);
        if (RXQueue.empty())
        {
            // no more replies available
            printf("RecvMPReplies timeout, ret=%04X myinstmask=%04X conn=%04X aidmask=%04X\n", ret, myinstmask, ConnectedBitmask, aidmask);
            return ret;
        }

        ENetPacket* enetpacket = RXQueue.front();
        RXQueue.pop();
        MPPacketHeader* header = (MPPacketHeader*)&enetpacket->data[0];
        bool good = true;
        if (enetpacket->dataLength < sizeof(MPPacketHeader))
            good = false;
        else if (header->Magic != 0x4946494E)
            good = false;
        else if (header->SenderID == MyPlayer.ID)
            good = false;
        else if ((header->Type & 0xFFFF) != 2)
            good = false;
        else if (header->Timestamp < (timestamp - 32))
            good = false;

        if (good)
        {
            u32 len = header->Length;
            if (len)
            {
                if (len > 1024) len = 1024;

                u32 aid = header->Type >> 16;
                memcpy(&packets[(aid-1)*1024], &enetpacket->data[sizeof(MPPacketHeader)], len);

                ret |= (1<<aid);
            }

            myinstmask |= (1<<header->SenderID);
            if (((myinstmask & ConnectedBitmask) == ConnectedBitmask) ||
                ((ret & aidmask) == aidmask))
            {
                // all the clients have sent their reply
                enet_packet_destroy(enetpacket);
                return ret;
            }
        }
        else printf("RecvMPReplies received frame but bad (type=%08X ts=%016llX/%016llX)\n", header->Type, header->Timestamp, timestamp);

        enet_packet_destroy(enetpacket);
    }
}

}
