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

#include "types.h"

namespace IPC
{

enum
{
    Cmd_Pause = 1,
};

extern int InstanceID;

void Init();
void DeInit();

void Process();

bool SendCommand(u16 recipients, u16 command, u16 len, void* data);

}

#endif // IPC_H
