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

#include "Netplay.h"


namespace Netplay
{

bool Active;
bool IsHost;

ENetHost* Host;
ENetPeer* Peer;


bool Init()
{
    Active = false;
    IsHost = false;
    Host = nullptr;

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


void StartHost()
{
    ENetAddress addr;
    addr.host = ENET_HOST_ANY;
    addr.port = 8064;

    // TODO determine proper number of clients/channels
    Host = enet_host_create(&addr, 16, 16, 0, 0);
    if (!Host)
    {
        printf("host shat itself :(\n");
        return;
    }

    Active = true;
    IsHost = true;
}

void StartClient()
{
    // TODO determine proper number of clients/channels
    Host = enet_host_create(nullptr, 16, 16, 0, 0);
    if (!Host)
    {
        printf("client shat itself :(\n");
        return;
    }

    printf("client created, connecting\n");

    ENetAddress addr;
    enet_address_set_host(&addr, "127.0.0.1"); // TODO!!!!
    addr.port = 8064;
    Peer = enet_host_connect(Host, &addr, 2, 0);
    if (!Peer)
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
        enet_peer_reset(Peer);
    }

    Active = true;
    IsHost = false;
}


void ProcessFrame()
{
    ENetEvent event;
    while (enet_host_service(Host, &event, 0) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            printf("client connected %08X %d\n", event.peer->address.host, event.peer->address.port);
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            printf("client disconnected %08X %d\n", event.peer->address.host, event.peer->address.port);
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            printf("received shit\n");
            break;
        }
    }
}

}
