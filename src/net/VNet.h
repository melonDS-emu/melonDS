/*
    Copyright 2016-2026 melonDS team

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

#ifndef VNET_H
#define VNET_H

#include <string>

// VNet: virtual LAN over a UDP relay server.
//
// While VNet is active, every UDP socket created through the ENet socket API
// (enet_socket_*) becomes a virtual socket: datagrams are tunneled through
// the relay server, which forwards them to the other members of the same room.
// Each member gets a virtual IPv4 address (10.77.0.x), and broadcasts reach
// everyone in the room. This lets the regular LAN multiplayer code work over
// the internet without port forwarding, since every player only makes
// outgoing connections to the relay.

namespace VNet
{

// server: "host" or "host:port" (default port 7070)
bool Start(const char* server, const char* room, std::string& error);
void Stop();

bool IsActive();

// virtual IP, in network byte order
unsigned int GetVirtualIP();
std::string GetVirtualIPString();

// round-trip time to the relay server, in milliseconds (-1 if unknown)
int GetRelayPing();

}

#endif // VNET_H
