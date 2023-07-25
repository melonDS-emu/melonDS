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

#ifndef DISCORDRPC_H
#define DISCORDRPC_H

#include "types.h"

class DiscordRPC
{
public:
    DiscordRPC();
    ~DiscordRPC();

    void Initialize();
    void ShutDown();
    void Update();
    void SetPresence(const bool isGameActive, const char* title, const char* gameId);
    bool IsConnected();
    
private:
    bool Connected = false;
    s64  StartTime = 0;
};

#endif
