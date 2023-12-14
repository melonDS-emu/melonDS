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

#include <string.h>
#include "NDS.h"
#include "RTC.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;




void WriteDateTime(int num, u8 val);


RTC::RTC(melonDS::NDS& nds) : NDS(nds)
{
    NDS.RegisterEventFunc(Event_RTC, 0, MemberEventFunc(RTC, ClockTimer));

    ResetState();

    // indicate the power was off
    // this will be changed if a previously saved RTC state is loaded
    State.StatusReg1 = 0x80;
}

RTC::~RTC()
{
    NDS.UnregisterEventFunc(Event_RTC, 0);
}

void RTC::Reset()
{
    Input = 0;
    InputBit = 0;
    InputPos = 0;

    memset(Output, 0, sizeof(Output));
    OutputPos = 0;

    CurCmd = 0;

    ClockCount = 0;
    ScheduleTimer(true);
}

void RTC::DoSavestate(Savestate* file)
{
    file->Section("RTC.");

    file->Var16(&IO);

    file->Var8(&Input);
    file->Var32(&InputBit);
    file->Var32(&InputPos);

    file->VarArray(Output, sizeof(Output));
    file->Var32(&OutputBit);
    file->Var32(&OutputPos);

    file->Var8(&CurCmd);

    file->VarArray(&State, sizeof(State));

    file->Var32((u32*)&TimerError);
    file->Var32(&ClockCount);
}


u8 RTC::BCD(u8 val) const
{
    return (val % 10) | ((val / 10) << 4);
}

u8 RTC::FromBCD(u8 val) const
{
    return (val & 0xF) + ((val >> 4) * 10);
}

u8 RTC::BCDIncrement(u8 val) const
{
    val++;
    if ((val & 0x0F) >= 0x0A)
        val += 0x06;
    if ((val & 0xF0) >= 0xA0)
        val += 0x60;
    return val;
}

u8 RTC::BCDSanitize(u8 val, u8 vmin, u8 vmax) const
{
    if (val < vmin || val > vmax)
        val = vmin;
    else if ((val & 0x0F) >= 0x0A)
        val = vmin;
    else if ((val & 0xF0) >= 0xA0)
        val = vmin;

    return val;
}


void RTC::GetState(StateData& state) const
{
    memcpy(&state, &State, sizeof(State));
}

void RTC::SetState(const StateData& state)
{
    memcpy(&State, &state, sizeof(State));

    // sanitize the input state

    for (int i = 0; i < 7; i++)
        WriteDateTime(i+1, State.DateTime[i]);
}

void RTC::GetDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) const
{
    year = FromBCD(State.DateTime[0]);
    year += 2000;
    month = FromBCD(State.DateTime[1] & 0x3F);
    day = FromBCD(State.DateTime[2] & 0x3F);

    hour = FromBCD(State.DateTime[4] & 0x3F);

    if (!(State.StatusReg1 & (1<<1)))
    {
        // 12-hour mode

        if (State.DateTime[4] & 0x40)
            hour += 12;
    }

    minute = FromBCD(State.DateTime[5] & 0x7F);
    second = FromBCD(State.DateTime[6] & 0x7F);
}

void RTC::SetDateTime(int year, int month, int day, int hour, int minute, int second)
{
    int monthdays[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // the year range of the DS RTC is limited to 2000-2099
    year %= 100;
    if (year < 0) year = 0;

    if (!(year & 3)) monthdays[2] = 29;

    if (month < 1 || month > 12) month = 1;
    if (day < 1 || day > monthdays[month]) day = 1;
    if (hour < 0 || hour > 23) hour = 0;
    if (minute < 0 || minute > 59) minute = 0;
    if (second < 0 || second > 59) second = 0;

    // note on day-of-week value
    // that RTC register is a simple incrementing counter and the assignation is defined by software
    // DS/DSi firmware counts from 0=Sunday

    int numdays = (year * 365) + ((year+3) / 4); // account for leap years

    for (int m = 1; m < month; m++)
    {
        numdays += monthdays[m];
    }
    numdays += (day-1);

    // 01/01/2000 is a Saturday, so the starting value is 6
    int dayofweek = (6 + numdays) % 7;

    int pm = (hour >= 12) ? 0x40 : 0;
    if (!(State.StatusReg1 & (1<<1)))
    {
        // 12-hour mode

        if (pm) hour -= 12;
    }

    State.DateTime[0] = BCD(year);
    State.DateTime[1] = BCD(month);
    State.DateTime[2] = BCD(day);
    State.DateTime[3] = dayofweek;
    State.DateTime[4] = BCD(hour) | pm;
    State.DateTime[5] = BCD(minute);
    State.DateTime[6] = BCD(second);

    State.StatusReg1 &= ~0x80;
}

void RTC::ResetState()
{
    memset(&State, 0, sizeof(State));
    State.DateTime[1] = 1;
    State.DateTime[2] = 1;
}


void RTC::SetIRQ(u8 irq)
{
    u8 oldstat = State.IRQFlag;
    State.IRQFlag |= irq;
    State.StatusReg1 |= irq;

    if ((!(oldstat & 0x30)) && (State.IRQFlag & 0x30))
    {
        if ((NDS.RCnt & 0xC100) == 0x8100)
        {
            // CHECKME: is the IRQ status readable in RCNT?
            NDS.SetIRQ(1, IRQ_RTC);
        }
    }
}

void RTC::ClearIRQ(u8 irq)
{
    State.IRQFlag &= ~irq;
}

void RTC::ProcessIRQ(int type) // 0=minute carry 1=periodic 2=status reg write
{
    // INT1

    switch (State.StatusReg2 & 0x0F)
    {
    case 0b0000: // none
        if (type == 2)
        {
            ClearIRQ(0x10);
        }
        break;

    case 0b0001:
    case 0b0101: // selected frequency steady interrupt
        if ((type == 1 && (!(ClockCount & 0x3FF))) || (type == 2))
        {
            u32 mask = 0;
            if (State.Alarm1[2] & (1<<0)) mask |= 0x4000;
            if (State.Alarm1[2] & (1<<1)) mask |= 0x2000;
            if (State.Alarm1[2] & (1<<2)) mask |= 0x1000;
            if (State.Alarm1[2] & (1<<3)) mask |= 0x0800;
            if (State.Alarm1[2] & (1<<4)) mask |= 0x0400;

            if (mask && ((ClockCount & mask) != mask))
                SetIRQ(0x10);
            else
                ClearIRQ(0x10);
        }
        break;

    case 0b0010:
    case 0b0110: // per-minute edge interrupt
        if ((type == 0) || (type == 2 && (State.IRQFlag & 0x01)))
        {
            SetIRQ(0x10);
        }
        break;

    case 0b0011: // per-minute steady interrupt 1 (duty 30s)
        if ((type == 0) || (type == 2 && (State.IRQFlag & 0x01)))
        {
            SetIRQ(0x10);
        }
        else if ((type == 1) && (State.DateTime[6] == 0x30) && ((ClockCount & 0x7FFF) == 0))
        {
            ClearIRQ(0x10);
        }
        break;

    case 0b0111: // per-minute steady interrupt 2 (duty 256 cycles)
        if ((type == 0) || (type == 2 && (State.IRQFlag & 0x01)))
        {
            SetIRQ(0x10);
        }
        else if ((type == 1) && (State.DateTime[6] == 0x00) && ((ClockCount & 0x7FFF) == 256))
        {
            ClearIRQ(0x10);
        }
        break;

    case 0b0100: // alarm interrupt
        if (type == 0)
        {
            bool cond = true;
            if (State.Alarm1[0] & (1<<7))
                cond = cond && ((State.Alarm1[0] & 0x07) == State.DateTime[3]);
            if (State.Alarm1[1] & (1<<7))
                cond = cond && ((State.Alarm1[1] & 0x7F) == State.DateTime[4]);
            if (State.Alarm1[2] & (1<<7))
                cond = cond && ((State.Alarm1[2] & 0x7F) == State.DateTime[5]);

            if (NDS.ConsoleType == 1)
            {
                if (State.AlarmDate1[1] & (1<<6))
                    cond = cond && (State.AlarmDate1[0] == State.DateTime[0]);
                if (State.AlarmDate1[1] & (1<<7))
                    cond = cond && ((State.AlarmDate1[1] & 0x1F) == State.DateTime[1]);
                if (State.AlarmDate1[2] & (1<<7))
                    cond = cond && ((State.AlarmDate1[2] & 0x3F) == State.DateTime[2]);
            }

            if (cond)
                SetIRQ(0x10);
            else
                ClearIRQ(0x10);
        }
        break;

    default: // 32KHz output
        if (type == 1)
        {
            SetIRQ(0x10);
            ClearIRQ(0x10);
        }
        break;
    }

    // INT2

    if (State.StatusReg2 & (1<<6))
    {
        // alarm interrupt

        if (type == 0)
        {
            bool cond = true;
            if (State.Alarm2[0] & (1<<7))
                cond = cond && ((State.Alarm2[0] & 0x07) == State.DateTime[3]);
            if (State.Alarm2[1] & (1<<7))
                cond = cond && ((State.Alarm2[1] & 0x7F) == State.DateTime[4]);
            if (State.Alarm2[2] & (1<<7))
                cond = cond && ((State.Alarm2[2] & 0x7F) == State.DateTime[5]);

            if (NDS.ConsoleType == 1)
            {
                if (State.AlarmDate2[1] & (1<<6))
                    cond = cond && (State.AlarmDate2[0] == State.DateTime[0]);
                if (State.AlarmDate2[1] & (1<<7))
                    cond = cond && ((State.AlarmDate2[1] & 0x1F) == State.DateTime[1]);
                if (State.AlarmDate2[2] & (1<<7))
                    cond = cond && ((State.AlarmDate2[2] & 0x3F) == State.DateTime[2]);
            }

            if (cond)
                SetIRQ(0x20);
            else
                ClearIRQ(0x20);
        }
    }
    else
    {
        if (type == 2)
        {
            ClearIRQ(0x20);
        }
    }
}


u8 RTC::DaysInMonth() const
{
    u8 numdays;

    switch (State.DateTime[1])
    {
    case 0x01: // Jan
    case 0x03: // Mar
    case 0x05: // May
    case 0x07: // Jul
    case 0x08: // Aug
    case 0x10: // Oct
    case 0x12: // Dec
        numdays = 0x31;
        break;

    case 0x04: // Apr
    case 0x06: // Jun
    case 0x09: // Sep
    case 0x11: // Nov
        numdays = 0x30;
        break;

    case 0x02: // Feb
        {
            numdays = 0x28;

            // leap year: if year divisible by 4 and not divisible by 100 unless divisible by 400
            // the limited year range (2000-2099) simplifies this
            int year = State.DateTime[0];
            year = (year & 0xF) + ((year >> 4) * 10);
            if (!(year & 3))
                numdays = 0x29;
        }
        break;

    default: // ???
        return 0;
    }

    return numdays;
}

void RTC::CountYear()
{
    State.DateTime[0] = BCDIncrement(State.DateTime[0]);
}

void RTC::CountMonth()
{
    State.DateTime[1] = BCDIncrement(State.DateTime[1]);
    if (State.DateTime[1] > 0x12)
    {
        State.DateTime[1] = 1;
        CountYear();
    }
}

void RTC::CheckEndOfMonth()
{
    if (State.DateTime[2] > DaysInMonth())
    {
        State.DateTime[2] = 1;
        CountMonth();
    }
}

void RTC::CountDay()
{
    // day-of-week counter
    State.DateTime[3]++;
    if (State.DateTime[3] >= 7)
        State.DateTime[3] = 0;

    // day counter
    State.DateTime[2] = BCDIncrement(State.DateTime[2]);
    CheckEndOfMonth();
}

void RTC::CountHour()
{
    u8 hour = BCDIncrement(State.DateTime[4] & 0x3F);
    u8 pm = State.DateTime[4] & 0x40;

    if (State.StatusReg1 & (1<<1))
    {
        // 24-hour mode

        if (hour >= 0x24)
        {
            hour = 0;
            CountDay();
        }

        pm = (hour >= 0x12) ? 0x40 : 0;
    }
    else
    {
        // 12-hour mode

        if (hour >= 0x12)
        {
            hour = 0;
            if (pm) CountDay();
            pm ^= 0x40;
        }
    }

    State.DateTime[4] = hour | pm;
}

void RTC::CountMinute()
{
    State.MinuteCount++;
    State.DateTime[5] = BCDIncrement(State.DateTime[5]);
    if (State.DateTime[5] >= 0x60)
    {
        State.DateTime[5] = 0;
        CountHour();
    }

    State.IRQFlag |= 0x01;  // store minute carry flag
    ProcessIRQ(0);
}

void RTC::CountSecond()
{
    State.DateTime[6] = BCDIncrement(State.DateTime[6]);
    if (State.DateTime[6] >= 0x60)
    {
        State.DateTime[6] = 0;
        CountMinute();
    }
}


void RTC::ScheduleTimer(bool first)
{
    if (first) TimerError = 0;

    // the RTC clock runs at 32768Hz
    // cycles = 33513982 / 32768
    s32 sysclock = 33513982 + TimerError;
    s32 delay = sysclock >> 15;
    TimerError = sysclock & 0x7FFF;

    NDS.ScheduleEvent(Event_RTC, !first, delay, 0, 0);
}

void RTC::ClockTimer(u32 param)
{
    ClockCount++;

    if (!(ClockCount & 0x7FFF))
    {
        // count up one second
        CountSecond();
    }
    else if ((ClockCount & 0x7FFF) == 4)
    {
        // minute-carry flag lasts 4 cycles
        State.IRQFlag &= ~0x01;
    }

    ProcessIRQ(1);

    ScheduleTimer(false);
}


void RTC::WriteDateTime(int num, u8 val)
{
    switch (num)
    {
    case 1: // year
        State.DateTime[0] = BCDSanitize(val, 0x00, 0x99);
        break;

    case 2: // month
        State.DateTime[1] = BCDSanitize(val & 0x1F, 0x01, 0x12);
        break;

    case 3: // day
        State.DateTime[2] = BCDSanitize(val & 0x3F, 0x01, 0x31);
        CheckEndOfMonth();
        break;

    case 4: // day of week
        State.DateTime[3] = BCDSanitize(val & 0x07, 0x00, 0x06);
        break;

    case 5: // hour
        {
            u8 hour = val & 0x3F;
            u8 pm = val & 0x40;

            if (State.StatusReg1 & (1<<1))
            {
                // 24-hour mode

                hour = BCDSanitize(hour, 0x00, 0x23);
                pm = (hour >= 0x12) ? 0x40 : 0;
            }
            else
            {
                // 12-hour mode

                hour = BCDSanitize(hour, 0x00, 0x11);
            }

            State.DateTime[4] = hour | pm;
        }
        break;

    case 6: // minute
        State.DateTime[5] = BCDSanitize(val & 0x7F, 0x00, 0x59);
        break;

    case 7: // second
        State.DateTime[6] = BCDSanitize(val & 0x7F, 0x00, 0x59);
        break;
    }
}

void RTC::SaveDateTime()
{
    int y, m, d, h, i, s;
    GetDateTime(y, m, d, h, i, s);
    Platform::WriteDateTime(y, m, d, h, i, s);
}

void RTC::CmdRead()
{
    if ((CurCmd & 0x0F) == 0x06)
    {
        switch (CurCmd & 0x70)
        {
        case 0x00:
            Output[0] = State.StatusReg1;
            State.StatusReg1 &= 0x0F; // clear auto-clearing bit4-7
            break;

        case 0x40:
            Output[0] = State.StatusReg2;
            break;

        case 0x20:
            memcpy(Output, &State.DateTime[0], 7);
            break;

        case 0x60:
            memcpy(Output, &State.DateTime[4], 3);
            break;

        case 0x10:
            if (State.StatusReg2 & 0x04)
                memcpy(Output, &State.Alarm1[0], 3);
            else
                Output[0] = State.Alarm1[2];
            break;

        case 0x50:
            memcpy(Output, &State.Alarm2[0], 3);
            break;

        case 0x30: Output[0] = State.ClockAdjust; break;
        case 0x70: Output[0] = State.FreeReg; break;
        }

        return;
    }
    else if ((CurCmd & 0x0F) == 0x0E)
    {
        if (NDS.ConsoleType != 1)
        {
            Log(LogLevel::Debug, "RTC: unknown read command %02X\n", CurCmd);
            return;
        }

        switch (CurCmd & 0x70)
        {
        case 0x00:
            Output[0] = (State.MinuteCount >> 16) & 0xFF;
            Output[1] = (State.MinuteCount >> 8) & 0xFF;
            Output[2] = State.MinuteCount & 0xFF;
            break;

        case 0x40: Output[0] = State.FOUT1; break;
        case 0x20: Output[0] = State.FOUT2; break;

        case 0x10:
            memcpy(Output, &State.AlarmDate1[0], 3);
            break;

        case 0x50:
            memcpy(Output, &State.AlarmDate2[0], 3);
            break;

        default:
            Log(LogLevel::Debug, "RTC: unknown read command %02X\n", CurCmd);
            break;
        }

        return;
    }

    Log(LogLevel::Debug, "RTC: unknown read command %02X\n", CurCmd);
}

void RTC::CmdWrite(u8 val)
{
    if ((CurCmd & 0x0F) == 0x06)
    {
        switch (CurCmd & 0x70)
        {
        case 0x00:
            if (InputPos == 1)
            {
                u8 oldval = State.StatusReg1;

                if (val & (1<<0)) // reset
                    ResetState();

                State.StatusReg1 = (State.StatusReg1 & 0xF0) | (val & 0x0E);

                if ((State.StatusReg1 ^ oldval) & (1<<1))
                {
                    // AM/PM changed

                    u8 hour = State.DateTime[4] & 0x3F;
                    u8 pm = State.DateTime[4] & 0x40;

                    if (State.StatusReg1 & (1<<1))
                    {
                        // 24-hour mode

                        if (pm)
                        {
                            hour += 0x12;
                            if ((hour & 0x0F) >= 0x0A)
                                hour += 0x06;
                        }

                        hour = BCDSanitize(hour, 0x00, 0x23);
                    }
                    else
                    {
                        // 12-hour mode

                        if (hour >= 0x12)
                        {
                            pm = 0x40;

                            hour -= 0x12;
                            if ((hour & 0x0F) >= 0x0A)
                                hour -= 0x06;
                        }
                        else
                            pm = 0;

                        hour = BCDSanitize(hour, 0x00, 0x11);
                    }

                    State.DateTime[4] = hour | pm;
                }
            }
            break;

        case 0x40:
            if (InputPos == 1)
            {
                State.StatusReg2 = val;
                ProcessIRQ(2);
            }
            break;

        case 0x20:
            if (InputPos <= 7)
                WriteDateTime(InputPos, val);
            if (InputPos == 7)
                SaveDateTime();
            break;

        case 0x60:
            if (InputPos <= 3)
                WriteDateTime(InputPos+4, val);
            if (InputPos == 3)
                SaveDateTime();
            break;

        case 0x10:
            if (State.StatusReg2 & 0x04)
            {
                if (InputPos <= 3)
                    State.Alarm1[InputPos-1] = val;
            }
            else
            {
                if (InputPos == 1)
                    State.Alarm1[2] = val;
            }
            break;

        case 0x50:
            if (InputPos <= 3)
                State.Alarm2[InputPos-1] = val;
            break;

        case 0x30:
            if (InputPos == 1)
                State.ClockAdjust = val;
            break;

        case 0x70:
            if (InputPos == 1)
                State.FreeReg = val;
            break;
        }

        return;
    }
    else if ((CurCmd & 0x0F) == 0x0E)
    {
        if (NDS.ConsoleType != 1)
        {
            Log(LogLevel::Debug, "RTC: unknown write command %02X\n", CurCmd);
            return;
        }

        switch (CurCmd & 0x70)
        {
        case 0x00:
            Log(LogLevel::Debug, "RTC: trying to write read-only minute counter\n");
            break;

        case 0x40:
            if (InputPos == 1)
                State.FOUT1 = val;
            break;

        case 0x20:
            if (InputPos == 1)
                State.FOUT2 = val;
            break;

        case 0x10:
            if (InputPos <= 3)
                State.AlarmDate1[InputPos-1] = val;
            break;

        case 0x50:
            if (InputPos <= 3)
                State.AlarmDate2[InputPos-1] = val;
            break;

        default:
            Log(LogLevel::Debug, "RTC: unknown write command %02X\n", CurCmd);
            break;
        }

        return;
    }

    Log(LogLevel::Debug, "RTC: unknown write command %02X\n", CurCmd);
}

void RTC::ByteIn(u8 val)
{
    if (InputPos == 0)
    {
        if ((val & 0xF0) == 0x60)
        {
            u8 rev[16] = {0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6};
            CurCmd = rev[val & 0xF];
        }
        else
            CurCmd = val;

        if (NDS.ConsoleType == 1)
        {
            // for DSi: handle extra commands

            if (((CurCmd & 0xF0) == 0x70) && ((CurCmd & 0xFE) != 0x76))
            {
                u8 rev[16] = {0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE};
                CurCmd = rev[CurCmd & 0xF];
            }
        }

        if (CurCmd & 0x80)
        {
            CmdRead();
        }
        return;
    }

    CmdWrite(val);
}


u16 RTC::Read()
{
    //printf("RTC READ %04X\n", IO);
    return IO;
}

void RTC::Write(u16 val, bool byte)
{
    if (byte) val |= (IO & 0xFF00);

    //printf("RTC WRITE %04X\n", val);
    if (val & 0x0004)
    {
        if (!(IO & 0x0004))
        {
            // start transfer
            Input = 0;
            InputBit = 0;
            InputPos = 0;

            memset(Output, 0, sizeof(Output));
            OutputBit = 0;
            OutputPos = 0;
        }
        else
        {
            if (!(val & 0x0002)) // clock low
            {
                if (val & 0x0010)
                {
                    // write
                    if (val & 0x0001)
                        Input |= (1<<InputBit);

                    InputBit++;
                    if (InputBit >= 8)
                    {
                        InputBit = 0;
                        ByteIn(Input);
                        Input = 0;
                        InputPos++;
                    }
                }
                else
                {
                    // read
                    if (Output[OutputPos] & (1<<OutputBit))
                        IO |= 0x0001;
                    else
                        IO &= 0xFFFE;

                    OutputBit++;
                    if (OutputBit >= 8)
                    {
                        OutputBit = 0;
                        if (OutputPos < 7)
                            OutputPos++;
                    }
                }
            }
        }
    }

    if (val & 0x0010)
        IO = val;
    else
        IO = (IO & 0x0001) | (val & 0xFFFE);
}

}