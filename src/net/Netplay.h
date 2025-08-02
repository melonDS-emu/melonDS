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

#ifndef NETPLAY_H
#define NETPLAY_H

#include "types.h"

namespace Netplay
{

struct Player
{
    int ID;
    char Name[32];
    int Status; // 0=no player 1=normal 2=host 3=connecting
    melonDS::u32 Address;
};


extern bool Active;

bool Init();
void DeInit();

void StartHost(const char* player, int port);
void StartClient(const char* player, const char* host, int port);
void StartMirror(const Player* player);

melonDS::u32 PlayerAddress(int id);

void StartGame();
void StartLocal();

void StartGame();

void ProcessFrame();
void ProcessInput();

}

#endif // NETPLAY_H
