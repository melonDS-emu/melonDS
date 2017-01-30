/*
    Copyright 2016-2017 StapleButter

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

#ifndef CP15_H
#define CP15_H

namespace CP15
{

void Reset();

void UpdateDTCMSetting();
void UpdateITCMSetting();

void Write(u32 id, u32 val);
u32 Read(u32 id);

bool HandleCodeRead16(u32 addr, u16* val);
bool HandleCodeRead32(u32 addr, u32* val);
bool HandleDataRead8(u32 addr, u8* val, u32 forceuser=0);
bool HandleDataRead16(u32 addr, u16* val, u32 forceuser=0);
bool HandleDataRead32(u32 addr, u32* val, u32 forceuser=0);
bool HandleDataWrite8(u32 addr, u8 val, u32 forceuser=0);
bool HandleDataWrite16(u32 addr, u16 val, u32 forceuser=0);
bool HandleDataWrite32(u32 addr, u32 val, u32 forceuser=0);

}

#endif
