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

#include <discord_rpc.h>
#include <NDSCart.h>

using namespace Platform;

const char* APP_ID          = "1094484548437422182";

const char* DETAILS_IDLE    = "Not in-game";

static void DiscordReady(const DiscordUser* user)
{
    Log(LogLevel::Info, "Connected to Discord, %s#%s (%s)\n", user->username, user->discriminator, user->userId);
}

static void DiscordDisconnected(const int reason, const char* message)
{
    Log(LogLevel::Info, "Disconnected from Discord with reason %d; \"%s\"\n", reason, message);
}

static void DiscordError(const int code, const char* message)
{
    Log(LogLevel::Error, "Caught an error from Discord with code %d; \"%s\"\n", code, message);
}

static void DiscordPartyEventWithSecret(const char* secret)
{} // ignored

static void DiscordPartyEventWithUser(const DiscordUser* user)
{
    Discord_Respond(user->userId, DISCORD_REPLY_IGNORE);
}

DiscordRPC::DiscordRPC()
{}

DiscordRPC::~DiscordRPC()
{
    ShutDown();
}

void DiscordRPC::Initialize()
{
    if (Platform::InstanceID() > 0)
    {
        return;
    }

    StartTime = time(0);

    DiscordEventHandlers event_handler =
    {
        .ready        = DiscordReady,
        .disconnected = DiscordDisconnected,
        .errored      = DiscordError,
        .joinGame     = DiscordPartyEventWithSecret,
        .spectateGame = DiscordPartyEventWithSecret,
        .joinRequest  = DiscordPartyEventWithUser
    };

    Discord_Initialize(APP_ID, &event_handler);

    Update();

    Connected = true;
}

void DiscordRPC::ShutDown()
{
    Discord_ClearPresence();
    Update();

    Discord_Shutdown();

    Connected = false;
}

void DiscordRPC::Update()
{
#ifdef DISCORD_DISABLE_IO_THREAD
    Discord_UpdateConnection();
#endif
    Discord_RunCallbacks();
}

void DiscordRPC::SetPresence(const bool isGameActive, const char* title)
{
    DiscordRichPresence presence =
    {
        .state          = NULL,
        .details        = DETAILS_IDLE,

        .startTimestamp = StartTime,
        .endTimestamp   = 0,

        .largeImageKey  = "melonds",
        .largeImageText = "melonDS - DS emulator, sorta",

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
        presence.state = title; // State shows game title
        presence.largeImageKey = strlwr(NDSCart::Header.GameCode); //Hack and a half to add support for game box art to discord rpc using lowercase gameids. Discord doe not convert to uppercase keys.
        presence.largeImageText = NDSCart::Header.GameCode; // Show game ID
        presence.smallImageKey = "melonds";
        presence.smallImageText = "melonDS";

    }

    if (!Config::Discord_TrackTime)
    {
        presence.startTimestamp = 0;
    }

    Discord_UpdatePresence(&presence);

    Update();
}

bool DiscordRPC::IsConnected()
{
    return Connected;
}
