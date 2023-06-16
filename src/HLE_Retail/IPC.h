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

#ifndef HLE_RETAIL_IPC_H
#define HLE_RETAIL_IPC_H

#include "../types.h"

namespace HLE
{
namespace Retail
{

void Reset();

void SendIPCReply(u32 service, u32 data, u32 flag = 0);

void StartScanline(u32 line);
void PollInput(u32 param);
void SoundProcess(u32 param);
void SoundPeach(u32 param);
void SoundAlarm(u32 num);

void OnIPCSync();
void OnIPCRequest();

}
}

#endif // HLE_RETAIL_IPC_H
