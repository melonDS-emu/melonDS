/*
    Copyright 2019 Arisotura, Buenia0

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

#ifndef SLOT2CART_H
#define SLOT2CART_H

#include <string>
#include "types.h"
#include "Savestate.h"


namespace Slot2Cart_RumblePak
{

extern bool RumblePakEnabled;
extern u16 RumbleState;

u16 ReadRumble(u32 addr);
void WriteRumble(u32 addr, u16 val);

}


enum class GuitarKeys : int
{
    Green = 0x40,
    Red = 0x20,
    Yellow = 0x10,
    Blue = 0x08,
};

namespace Slot2Cart_GuitarGrip
{

extern bool GuitarGripEnabled;
extern u8 GuitarKeyStatus;

u8 ReadGrip8(u32 addr);
u16 ReadGrip16(u32 addr);

void SetGripKey(GuitarKeys key, bool val);

}

namespace Slot2Cart_MemExpansionPak
{

extern bool MemPakEnabled;
extern u8 MemPakHeader[];
extern u8 MemPakMemory[0x800000];
extern bool MemPakRAMLock;

u8 ReadMemPak8(u32 addr);
void WriteMemPak8(u32 addr, u8 val);

u16 ReadMemPak16(u32 addr);
void WriteMemPak16(u32 addr, u16 val);

u32 ReadMemPak32(u32 addr);
void WriteMemPak32(u32 addr, u32 val);

void DoSavestate(Savestate* file);

}

namespace Slot2Cart_SegaCardReader
{

extern bool Enabled;
extern u8 HCVControl;

u8 Read8(u32 addr);
void Write8(u32 addr, u8 val);

u16 Read16(u32 addr);

u32 Read32(u32 addr);

void Load(std::string path);

}

#endif // SLOT2CART_H
