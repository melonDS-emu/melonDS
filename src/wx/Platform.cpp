/*
    Copyright 2016-2017 StapleButter

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
#include "../Platform.h"

#ifdef __WXMSW__
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define socket_t    SOCKET
	#define sockaddr_t  SOCKADDR
	#define pcap_dev_name description
#else
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <sys/socket.h>
	#define socket_t    int
	#define sockaddr_t  struct sockaddr
	#define closesocket close
	#define pcap_dev_name name
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET  (socket_t)-1
#endif


namespace Platform
{


socket_t MPSocket;
sockaddr_t MPSendAddr;


bool MP_Init()
{
    BOOL opt_true = TRUE;
    int res;

    MPSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (MPSocket < 0)
	{
		return false;
	}

	res = setsockopt(MPSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt_true, sizeof(BOOL));
	if (res < 0)
	{
		closesocket(MPSocket);
		MPSocket = INVALID_SOCKET;
		return false;
	}

	sockaddr_t saddr;
	saddr.sa_family = AF_INET;
	*(u32*)&saddr.sa_data[2] = htonl(INADDR_ANY);
	*(u16*)&saddr.sa_data[0] = htons(7064);
	res = bind(MPSocket, &saddr, sizeof(sockaddr_t));
	if (res < 0)
	{
		closesocket(MPSocket);
		MPSocket = INVALID_SOCKET;
		return false;
	}

	res = setsockopt(MPSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&opt_true, sizeof(BOOL));
	if (res < 0)
	{
		closesocket(MPSocket);
		MPSocket = INVALID_SOCKET;
		return false;
	}

	MPSendAddr.sa_family = AF_INET;
	*(u32*)&MPSendAddr.sa_data[2] = htonl(INADDR_BROADCAST);
	*(u16*)&MPSendAddr.sa_data[0] = htons(7064);

	return true;
}

void MP_DeInit()
{
    if (MPSocket >= 0)
        closesocket(MPSocket);
}


}
