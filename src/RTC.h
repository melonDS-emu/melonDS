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
class NDS;

class RTC
{
public:
    struct DateTime
    {
        int Year;
        int Month;
        int Day;
        int Hour;
        int Minute;
        int Second;
    };

    struct StateData
    {
        u8 StatusReg1 = 0;
        u8 StatusReg2 = 0;
        std::array<u8, 7> DateTime;
        std::array<u8, 3> Alarm1;
        std::array<u8, 3> Alarm2;
        u8 ClockAdjust = 0;
        u8 FreeReg = 0;

        u8 IRQFlag {};

        // DSi registers
        u32 MinuteCount = 0;
        u8 FOUT1 = 0;
        u8 FOUT2 = 0;
        std::array<u8, 3> AlarmDate1 {};
        std::array<u8, 3> AlarmDate2 {};
    };

    explicit RTC(melonDS::NDS& nds);
    ~RTC();

    void Reset();

    void DoSavestate(Savestate* file);

    void GetState(StateData& state) const noexcept;
    [[nodiscard]] StateData GetState() const noexcept { return State; }
    void SetState(StateData& state) noexcept;
    void GetDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) const noexcept;
    [[nodiscard]] DateTime GetDateTime() const noexcept
    {
        int year, month, day, hour, minute, second;
        GetDateTime(year, month, day, hour, minute, second);
        return {year, month, day, hour, minute, second};
    }
    void SetDateTime(int year, int month, int day, int hour, int minute, int second) noexcept;
    void SetDateTime(const DateTime& datetime) noexcept
    {
        SetDateTime(datetime.Year, datetime.Month, datetime.Day, datetime.Hour, datetime.Minute, datetime.Second);
    }

    void ClockTimer(u32 param);

    u16 Read() const noexcept;
    void Write(u16 val, bool byte);

private:
    melonDS::NDS& NDS;

    u16 IO = 0;
    u8 Input = 0;
    u32 InputBit = 0;
    u32 InputPos = 0;

    u8 Output[8] {};
    u32 OutputBit = 0;
    u32 OutputPos = 0;

    u8 CurCmd = 0;

    StateData State {};

    s32 TimerError = 0;
    u32 ClockCount = 0;

    void ResetState();
    void ScheduleTimer(bool first);

    void SetIRQ(u8 irq);
    void ClearIRQ(u8 irq);
    void ProcessIRQ(int type);

    u8 DaysInMonth() const noexcept;
    void CountYear();
    void CountMonth();
    void CheckEndOfMonth();
    void CountDay();
    void CountHour();
    void CountMinute();
    void CountSecond();

    void WriteDateTime(int num, u8 val);
    void SaveDateTime() const noexcept;

    void CmdRead();
    void CmdWrite(u8 val);
    void ByteIn(u8 val);
};

constexpr u8 BCD(u8 val) noexcept
{
    return (val % 10) | ((val / 10) << 4);
}

constexpr u8 FromBCD(u8 val) noexcept
{
    return (val & 0xF) + ((val >> 4) * 10);
}

constexpr u8 BCDIncrement(u8 val) noexcept
{
    val++;
    if ((val & 0x0F) >= 0x0A)
        val += 0x06;
    if ((val & 0xF0) >= 0xA0)
        val += 0x60;
    return val;
}

constexpr u8 BCDSanitize(u8 val, u8 vmin, u8 vmax) noexcept
{
    if (val < vmin || val > vmax)
        val = vmin;
    else if ((val & 0x0F) >= 0x0A)
        val = vmin;
    else if ((val & 0xF0) >= 0xA0)
        val = vmin;

    return val;
}

}
#endif
