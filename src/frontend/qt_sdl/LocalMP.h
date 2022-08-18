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

#ifndef LOCALMP_H
#define LOCALMP_H

#include "types.h"

namespace LocalMP
{

bool Init();
void DeInit();
int SendPacket(u8* data, int len);
int RecvPacket(u8* data, bool block);
bool SendSync(u16 clientmask, u16 type, u64 val);
bool WaitSync(u16 clientmask, u16* type, u64* val);
u16 WaitMultipleSyncs(u16 type, u16 clientmask, u64 curval);

}

#endif // LOCALMP_H
