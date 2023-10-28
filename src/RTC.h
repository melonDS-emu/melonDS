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

#ifndef RTC_H
#define RTC_H

#include "types.h"
#include "Savestate.h"

namespace RTC
{

struct StateData
{
    u8 StatusReg1;
    u8 StatusReg2;
    u8 DateTime[7];
    u8 Alarm1[3];
    u8 Alarm2[3];
    u8 ClockAdjust;
    u8 FreeReg;

    u8 IRQFlag;

    // DSi registers
    u32 MinuteCount;
    u8 FOUT1;
    u8 FOUT2;
    u8 AlarmDate1[3];
    u8 AlarmDate2[3];
};

bool Init();
void DeInit();
void Reset();
void DoSavestate(Savestate* file);

void GetState(StateData& state);
void SetState(StateData& state);
void GetDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second);
void SetDateTime(int year, int month, int day, int hour, int minute, int second);
void ResetState();

void ScheduleTimer(bool first);
void ClockTimer(u32 param);

u16 Read();
void Write(u16 val, bool byte);

}

#endif
