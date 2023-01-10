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


namespace IPC
{

QSharedMemory* Buffer = nullptr;
int InstanceID;


void Init()
{
    InstanceID = 0;

    Buffer = new QSharedMemory("melonIPC");

    if (!Buffer->attach())
    {
        printf("IPC sharedmem doesn't exist. creating\n");
        if (!Buffer->create(1024))
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
    u16 mask = *(u16*)&data[0];
    for (int i = 0; i < 16; i++)
    {
        if (!(mask & (1<<i)))
        {
            InstanceID = i;
            *(u16*)&data[0] |= (1<<i);
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
        *(u16*)&data[0] &= ~(1<<InstanceID);
        Buffer->unlock();

        Buffer->detach();
        delete Buffer;
    }
    Buffer = nullptr;
}

}
