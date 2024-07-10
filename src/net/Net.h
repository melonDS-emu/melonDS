/*
    Copyright 2016-2024 melonDS team

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

#ifndef NET_H
#define NET_H

#include "types.h"

namespace Net
{
using namespace melonDS;

bool Init();
void DeInit();

void RegisterInstance(int inst);
void UnregisterInstance(int inst);

void RXEnqueue(const void* buf, int len);

int SendPacket(u8* data, int len, int inst);
int RecvPacket(u8* data, int inst);

}

#endif // NET_H
