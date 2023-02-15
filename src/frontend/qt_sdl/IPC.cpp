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
#include <QSharedMemory>

#include "IPC.h"
#include "Config.h"
//#include "Input.h"


namespace Input
{
void ExtHotkeyPress(int id);
}


namespace IPC
{

QSharedMemory* Buffer = nullptr;
int InstanceID;

struct BufferHeader
{
    u16 NumInstances;
    u16 InstanceBitmask;  // bitmask of all instances present
    u32 CommandWriteOffset;
};

struct CommandHeader
{
    u32 Magic;
    u16 SenderID;
    u16 Recipients;
    u16 Command;
    u16 Length;
};

u32 CommandReadOffset;

const u32 kBufferSize = 0x4000;
const u32 kMaxCommandSize = 0x800;
const u32 kCommandStart = sizeof(BufferHeader);
const u32 kCommandEnd = kBufferSize;


void Init()
{
    InstanceID = 0;

    Buffer = new QSharedMemory("melonIPC");

    if (!Buffer->attach())
    {
        printf("IPC sharedmem doesn't exist. creating\n");
        if (!Buffer->create(kBufferSize))
        {
            printf("IPC sharedmem create failed :(\n");
            delete Buffer;
            Buffer = nullptr;
            return;
        }

        Buffer->lock();
        memset(Buffer->data(), 0, Buffer->size());
        Buffer->unlock();
    }

    Buffer->lock();
    u8* data = (u8*)Buffer->data();
    BufferHeader* header = (BufferHeader*)&data[0];

    if (header->NumInstances == 0)
    {
        // initialize the FIFO
        header->CommandWriteOffset = kCommandStart;
    }

    CommandReadOffset = header->CommandWriteOffset;

    u16 mask = header->InstanceBitmask;
    for (int i = 0; i < 16; i++)
    {
        if (!(mask & (1<<i)))
        {
            InstanceID = i;
            header->InstanceBitmask |= (1<<i);
            header->NumInstances++;
            break;
        }
    }
    Buffer->unlock();

    printf("IPC: instance ID %d\n", InstanceID);
}

void DeInit()
{
    if (Buffer)
    {
        Buffer->lock();
        u8* data = (u8*)Buffer->data();
        BufferHeader* header = (BufferHeader*)&data[0];
        header->InstanceBitmask &= ~(1<<InstanceID);
        header->NumInstances--;
        Buffer->unlock();

        Buffer->detach();
        delete Buffer;
    }
    Buffer = nullptr;
}


void FIFORead(void* buf, int len)
{
    u8* data = (u8*)Buffer->data();

    u32 offset, start, end;

    offset = CommandReadOffset;
    start = kCommandStart;
    end = kCommandEnd;

    if ((offset + len) >= end)
    {
        u32 part1 = end - offset;
        memcpy(buf, &data[offset], part1);
        memcpy(&((u8*)buf)[part1], &data[start], len - part1);
        offset = start + len - part1;
    }
    else
    {
        memcpy(buf, &data[offset], len);
        offset += len;
    }

    CommandReadOffset = offset;
}

void FIFOWrite(void* buf, int len)
{
    u8* data = (u8*)Buffer->data();
    BufferHeader* header = (BufferHeader*)&data[0];

    u32 offset, start, end;

    offset = header->CommandWriteOffset;
    start = kCommandStart;
    end = kCommandEnd;

    if ((offset + len) >= end)
    {
        u32 part1 = end - offset;
        memcpy(&data[offset], buf, part1);
        memcpy(&data[start], &((u8*)buf)[part1], len - part1);
        offset = start + len - part1;
    }
    else
    {
        memcpy(&data[offset], buf, len);
        offset += len;
    }

    header->CommandWriteOffset = offset;
}

void Process()
{
    Buffer->lock();
    u8* data = (u8*)Buffer->data();
    BufferHeader* header = (BufferHeader*)&data[0];

    // check if we got new commands
    while (CommandReadOffset != header->CommandWriteOffset)
    {
        CommandHeader cmdheader;
        u8 cmddata[kMaxCommandSize];

        FIFORead(&cmdheader, sizeof(cmdheader));

        if ((cmdheader.Magic != 0x4D434C4D) || (cmdheader.Length > kMaxCommandSize))
        {
            printf("IPC: !!! COMMAND BUFFER IS FUCKED. RESETTING\n");
            CommandReadOffset = header->CommandWriteOffset;
            Buffer->unlock();
            return;
        }

        if (cmdheader.Length)
            FIFORead(cmddata, cmdheader.Length);

        if (!(cmdheader.Recipients & (1<<InstanceID)))
            continue;

        // handle this command
        switch (cmdheader.Command)
        {
        case Cmd_Pause:
            Input::ExtHotkeyPress(HK_Pause);
            break;
        }
    }

    Buffer->unlock();
}

bool SendCommand(u16 recipients, u16 command, u16 len, void* cmddata)
{
    Buffer->lock();
    u8* data = (u8*)Buffer->data();
    BufferHeader* header = (BufferHeader*)&data[0];

    recipients &= header->InstanceBitmask;
    recipients &= ~(1<<InstanceID);
    if (!recipients)
    {
        Buffer->unlock();
        return false;
    }

    if (len && cmddata==nullptr)
    {
        printf("IPC: ????? sending command with NULL buffer\n");
        Buffer->unlock();
        return false;
    }
    if (len > kMaxCommandSize)
    {
        printf("IPC: command too long\n");
        Buffer->unlock();
        return false;
    }

    CommandHeader cmdheader;
    cmdheader.Magic = 0x4D434C4D;
    cmdheader.SenderID = InstanceID;
    cmdheader.Recipients = recipients;
    cmdheader.Command = command;
    cmdheader.Length = len;
    FIFOWrite(&cmdheader, sizeof(cmdheader));
    if (len)
        FIFOWrite(cmddata, len);

    Buffer->unlock();
    return true;
}

}
