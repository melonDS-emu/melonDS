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

#include "RichPresence.h"
#include "discord-rpc/include/discord_rpc.h"
#include <cstddef>

namespace melonDS
{

    RichPresence::RichPresence()
    {

    }

    RichPresence::~RichPresence()
    {
        Discord_Shutdown();
    }

    void RichPresence::Init() 
    {
        DiscordEventHandlers handlers{};
        Discord_Initialize(ApplicationID, &handlers, 1, NULL);
    }

    void RichPresence::Update(std::string state, std::string details)
    {
        DiscordRichPresence discordPresence{};
        discordPresence.details = details.c_str();
        discordPresence.state = state.c_str();
        discordPresence.startTimestamp = StartTime;
        Discord_UpdatePresence(&discordPresence);
    }

    void RichPresence::RunCallbacks()
    {
        Discord_RunCallbacks();
    }

}