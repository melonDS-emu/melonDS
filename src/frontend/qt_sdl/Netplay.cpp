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

#include "NDS.h"
#include "main.h"
#include "Netplay.h"
#include "Input.h"


extern EmuThread* emuThread;


namespace Netplay
{

bool Active;
bool IsHost;

ENetHost* Host;
ENetPeer* Peer;

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


void ProcessFrame()
{
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
            Peer = event.peer;
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            printf("client disconnected %08X %d\n", event.peer->address.host, event.peer->address.port);
            Peer = nullptr;
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
