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

#ifndef IPC_H
#define IPC_H

#include <string>

#include "types.h"

namespace IPC
{

enum
{
    Cmd_Pause = 1,

    Cmd_LoadROM,
    Cmd_SetupNetplayMirror,
    Cmd_Start,

    Cmd_MAX
};

extern int InstanceID;

bool Init();
bool InitSema();
void DeInit();
void DeInitSema();

void SetMPRecvTimeout(int timeout);
void MPBegin();
void MPEnd();

void SetActive(bool active);

u16 GetInstanceBitmask();

void ProcessCommands();
bool SendCommand(u16 recipients, u16 command, u16 len, void* data);
bool SendCommandU8(u16 recipients, u16 command, u8 arg);
bool SendCommandStr(u16 recipients, u16 command, std::string str);

int SendMPPacket(u8* data, int len, u64 timestamp);
int RecvMPPacket(u8* data, u64* timestamp);
int SendMPCmd(u8* data, int len, u64 timestamp);
int SendMPReply(u8* data, int len, u64 timestamp, u16 aid);
int SendMPAck(u8* data, int len, u64 timestamp);
int RecvMPHostPacket(u8* data, u64* timestamp);
u16 RecvMPReplies(u8* data, u64 timestamp, u16 aidmask);

}

#endif // IPC_H
