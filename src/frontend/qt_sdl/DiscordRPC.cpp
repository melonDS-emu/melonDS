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

#include "DiscordRPC.h"

#include "Config.h"
#include "Platform.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

const char* APP_ID          = "1049597419152744458";

const char* DETAILS_IDLE    = "Not playing";
const char* DETAILS_PLAYING = "Currently in game";

void Ready(const DiscordUser* user)
{
    printf("Connected to Discord, %s#%s (%s)\n", user->username, user->discriminator, user->userId);
}

void Disconnected(const int reason, const char* message)
{
    printf("Disconnected from Discord with reason %d; \"%s\"\n", reason, message);
}

void Error(const int code, const char* message)
{
    printf("Caught an error from Discord with code %d; \"%s\"\n", code, message);
}

void PartyEventWithSecret(const char* secret)
{
    // ignored
}

void PartyEventWithUser(const DiscordUser* user)
{
    Discord_Respond(user->userId, DISCORD_REPLY_IGNORE);
}

DiscordRPC::DiscordRPC()
{
    if (Platform::InstanceID() != 0)
    {
        return;
    }

    StartTime = time(0);

    DiscordEventHandlers event_handler =
    {
        .ready        = Ready,
        .disconnected = Disconnected,
        .errored      = Error,
        .joinGame     = PartyEventWithSecret,
        .spectateGame = PartyEventWithSecret,
        .joinRequest  = PartyEventWithUser
    };

    Discord_Initialize(APP_ID, &event_handler, false, NULL);
}

DiscordRPC::~DiscordRPC()
{
    if (Platform::InstanceID() != 0)
    {
        return;
    }

    Discord_ClearPresence();
    Discord_RunCallbacks();

    Discord_Shutdown();
}

void DiscordRPC::Update(const bool isGameActive, const char* title)
{
    if (Platform::InstanceID() != 0)
    {
        return;
    }

    DiscordRichPresence presence =
    {
        .state          = NULL,
        .details        = DETAILS_IDLE,

        .startTimestamp = StartTime,
        .endTimestamp   = 0,

        .largeImageKey  = "melonds",
        .largeImageText = "DS emulator, sorta",

        .smallImageKey  = NULL,
        .smallImageText = NULL,

        .partyId        = 0,
        .partySize      = 0,
        .partyMax       = 0,
        .partyPrivacy   = DISCORD_PARTY_PRIVATE,

        .matchSecret    = NULL,
        .joinSecret     = NULL,
        .spectateSecret = NULL,

        .instance       = 0
    };

    if (isGameActive)
    {
        presence.state   = title;
        presence.details = DETAILS_PLAYING;
    }

    if (!Config::DiscordTrackTime)
    {
        presence.startTimestamp = 0;
    }

    Discord_UpdatePresence(&presence);

    Discord_RunCallbacks();
}
