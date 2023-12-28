/*
    Copyright 2016-2023 melonDS team

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

namespace melonDS
{
class RTC
{
public:

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

    RTC(melonDS::NDS& nds);
    ~RTC();

    void Reset();

    void DoSavestate(Savestate* file);

    void GetState(StateData& state) const;
    void SetState(const StateData& state);
    void GetDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) const;
    void SetDateTime(int year, int month, int day, int hour, int minute, int second);

    void ClockTimer(u32 param);

    u16 Read();
    void Write(u16 val, bool byte);

private:
    melonDS::NDS& NDS;
    u16 IO;

    u8 Input;
    u32 InputBit;
    u32 InputPos;

    u8 Output[8];
    u32 OutputBit;
    u32 OutputPos;

    u8 CurCmd;

    StateData State;

    s32 TimerError;
    u32 ClockCount;

    void ResetState();
    void ScheduleTimer(bool first);

    u8 BCD(u8 val) const;
    u8 FromBCD(u8 val) const;
    u8 BCDIncrement(u8 val) const;
    u8 BCDSanitize(u8 val, u8 vmin, u8 vmax) const;

    void SetIRQ(u8 irq);
    void ClearIRQ(u8 irq);
    void ProcessIRQ(int type);

    u8 DaysInMonth() const;
    void CountYear();
    void CountMonth();
    void CheckEndOfMonth();
    void CountDay();
    void CountHour();
    void CountMinute();
    void CountSecond();

    void WriteDateTime(int num, u8 val);
    void SaveDateTime();

    void CmdRead();
    void CmdWrite(u8 val);
    void ByteIn(u8 val);
};

}
#endif
