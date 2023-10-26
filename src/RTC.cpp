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

// Required by MinGW to enable localtime_r in time.h
#define _POSIX_THREAD_SAFE_FUNCTIONS

#include <string.h>
#include <time.h>
#include "NDS.h"
#include "RTC.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace RTC
{

/// This value represents the Nintendo DS IO register,
/// \em not the value of the system's clock.
/// The actual system time is taken directly from the host.
u16 IO;

u8 Input;
u32 InputBit;
u32 InputPos;

u8 Output[8];
u32 OutputBit;
u32 OutputPos;

u8 CurCmd;

u8 StatusReg1;
u8 StatusReg2;
u8 DateTime[7];
u8 Alarm1[3];
u8 Alarm2[3];
u8 ClockAdjust;
u8 FreeReg;

// DSi registers
u32 MinuteCount;
u8 FOUT1;
u8 FOUT2;
u8 AlarmDate1[3];
u8 AlarmDate2[3];

s32 TimerError;
u32 ClockCount;


bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    Input = 0;
    InputBit = 0;
    InputPos = 0;

    memset(Output, 0, sizeof(Output));
    OutputPos = 0;

    CurCmd = 0;

    MinuteCount = 0;
    ResetRegisters();

    ClockCount = 0;
    ScheduleTimer(true);
}

void DoSavestate(Savestate* file)
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

    file->Var8(&StatusReg1);
    file->Var8(&StatusReg2);
    file->VarArray(DateTime, sizeof(DateTime));
    file->VarArray(Alarm1, sizeof(Alarm1));
    file->VarArray(Alarm2, sizeof(Alarm2));
    file->Var8(&ClockAdjust);
    file->Var8(&FreeReg);

    file->Var32(&MinuteCount);
    file->Var8(&FOUT1);
    file->Var8(&FOUT2);
    file->VarArray(AlarmDate1, sizeof(AlarmDate1));
    file->VarArray(AlarmDate2, sizeof(AlarmDate2));

    file->Var32((u32*)&TimerError);
    file->Var32(&ClockCount);
}


void ResetRegisters()
{
    StatusReg1 = 0;
    StatusReg2 = 0;
    DateTime[0] = 0; DateTime[1] = 1; DateTime[2] = 1; DateTime[3] = 0;
    DateTime[4] = 0; DateTime[5] = 0; DateTime[6] = 0;
    memset(Alarm1, 0, sizeof(Alarm1));
    memset(Alarm2, 0, sizeof(Alarm2));
    ClockAdjust = 0;
    FreeReg = 0;

    FOUT1 = 0;
    FOUT2 = 0;
    memset(AlarmDate1, 0, sizeof(AlarmDate1));
    memset(AlarmDate2, 0, sizeof(AlarmDate2));
}


u8 BCD(u8 val)
{
    return (val % 10) | ((val / 10) << 4);
}

u8 BCDIncrement(u8 val)
{
    val++;
    if ((val & 0x0F) >= 0x0A)
        val += 0x06;
    if ((val & 0xF0) >= 0xA0)
        val += 0x60;
    return val;
}

u8 BCDSanitize(u8 val, u8 vmin, u8 vmax)
{
    if (val < vmin || val > vmax)
        val = vmin;
    else if ((val & 0x0F) >= 0x0A)
        val = vmin;
    else if ((val & 0xF0) >= 0xA0)
        val = vmin;

    return val;
}

u8 DaysInMonth()
{
    u8 numdays;

    switch (DateTime[1])
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
            int year = DateTime[0];
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

void CountYear()
{
    DateTime[0] = BCDIncrement(DateTime[0]);
}

void CountMonth()
{
    DateTime[1] = BCDIncrement(DateTime[1]);
    if (DateTime[1] > 0x12)
    {
        DateTime[1] = 1;
        CountYear();
    }
}

void CheckEndOfMonth()
{
    if (DateTime[2] > DaysInMonth())
    {
        DateTime[2] = 1;
        CountMonth();
    }
}

void CountDay()
{
    // day-of-week counter
    DateTime[3]++;
    if (DateTime[3] >= 7) DateTime[3] = 0;

    // day counter
    DateTime[2] = BCDIncrement(DateTime[2]);
    CheckEndOfMonth();
}

void CountHour()
{
    u8 hour = BCDIncrement(DateTime[4] & 0x3F);
    u8 pm = DateTime[4] & 0x40;

    if (StatusReg1 & (1<<1))
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

    DateTime[4] = hour | pm;
}

void CountMinute()
{
    MinuteCount++;
    DateTime[5] = BCDIncrement(DateTime[5]);
    if (DateTime[5] >= 0x60)
    {
        DateTime[5] = 0;
        CountHour();
    }
}

void CountSecond()
{
    DateTime[6] = BCDIncrement(DateTime[6]);
    if (DateTime[6] >= 0x60)
    {
        DateTime[6] = 0;
        CountMinute();
    }
}


void ScheduleTimer(bool first)
{
    if (first) TimerError = 0;

    // the RTC clock runs at 32768Hz
    // cycles = 33513982 / 32768
    s32 sysclock = 33513982 + TimerError;
    s32 delay = sysclock >> 15;
    TimerError = sysclock & 0x7FFF;

    NDS::ScheduleEvent(NDS::Event_RTC, !first, delay, ClockTimer, 0);
}

void ClockTimer(u32 param)
{
    ClockCount++;

    if (!(ClockCount & 0x7FFF))
    {
        // count up one second
        CountSecond();
    }

    ScheduleTimer(false);
}


void WriteDateTime(int num, u8 val)
{
    switch (num)
    {
    case 1: // year
        DateTime[0] = BCDSanitize(val, 0x00, 0x99);
        break;

    case 2: // month
        DateTime[1] = BCDSanitize(val & 0x1F, 0x01, 0x12);
        break;

    case 3: // day
        DateTime[2] = BCDSanitize(val & 0x3F, 0x01, 0x31);
        CheckEndOfMonth();
        break;

    case 4: // day of week
        DateTime[3] = BCDSanitize(val & 0x07, 0x00, 0x06);
        break;

    case 5: // hour
        {
            u8 hour = val & 0x3F;
            u8 pm = val & 0x40;

            if (StatusReg1 & (1<<1))
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

            DateTime[4] = hour | pm;
        }
        break;

    case 6: // minute
        DateTime[5] = BCDSanitize(val & 0x7F, 0x00, 0x59);
        break;

    case 7: // second
        DateTime[6] = BCDSanitize(val & 0x7F, 0x00, 0x59);
        break;
    }
}

void CmdRead()
{
    if ((CurCmd & 0x0F) == 0x06)
    {
        switch (CurCmd & 0x70)
        {
        case 0x00: Output[0] = StatusReg1; break;
        case 0x40: Output[0] = StatusReg2; break;

        case 0x20:
            Output[0] = DateTime[0];
            Output[1] = DateTime[1];
            Output[2] = DateTime[2];
            Output[3] = DateTime[3];
            Output[4] = DateTime[4];
            Output[5] = DateTime[5];
            Output[6] = DateTime[6];
            break;

        case 0x60:
            Output[0] = DateTime[4];
            Output[1] = DateTime[5];
            Output[2] = DateTime[6];
            break;

        case 0x10:
            if (StatusReg2 & 0x04)
            {
                Output[0] = Alarm1[0];
                Output[1] = Alarm1[1];
                Output[2] = Alarm1[2];
            }
            else
                Output[0] = Alarm1[2];
            break;

        case 0x50:
            Output[0] = Alarm2[0];
            Output[1] = Alarm2[1];
            Output[2] = Alarm2[2];
            break;

        case 0x30: Output[0] = ClockAdjust; break;
        case 0x70: Output[0] = FreeReg; break;
        }

        return;
    }
    else if ((CurCmd & 0x0F) == 0x0E)
    {
        if (NDS::ConsoleType != 1)
        {
            Log(LogLevel::Debug, "RTC: unknown read command %02X\n", CurCmd);
            return;
        }

        switch (CurCmd & 0x70)
        {
        case 0x00:
            Output[0] = (MinuteCount >> 16) & 0xFF;
            Output[1] = (MinuteCount >> 8) & 0xFF;
            Output[2] = MinuteCount & 0xFF;
            break;

        case 0x40: Output[0] = FOUT1; break;
        case 0x20: Output[0] = FOUT2; break;

        case 0x10:
            Output[0] = AlarmDate1[0];
            Output[1] = AlarmDate1[1];
            Output[2] = AlarmDate1[2];
            break;

        case 0x50:
            Output[0] = AlarmDate2[0];
            Output[1] = AlarmDate2[1];
            Output[2] = AlarmDate2[2];
            break;

        default:
            Log(LogLevel::Debug, "RTC: unknown read command %02X\n", CurCmd);
            break;
        }

        return;
    }

    Log(LogLevel::Debug, "RTC: unknown read command %02X\n", CurCmd);
}

void CmdWrite(u8 val)
{
    if ((CurCmd & 0x0F) == 0x06)
    {
        switch (CurCmd & 0x70)
        {
        case 0x00:
            if (InputPos == 1)
            {
                u8 oldval = StatusReg1;

                if (val & (1<<0)) // reset
                    ResetRegisters();

                StatusReg1 = (StatusReg1 & 0xF0) | (val & 0x0E);

                if ((StatusReg1 ^ oldval) & (1<<1)) // AM/PM changed
                    WriteDateTime(5, DateTime[4]);
            }
            break;

        case 0x40:
            if (InputPos == 1)
            {
                StatusReg2 = val;
                if (StatusReg2 & 0x4F) Log(LogLevel::Debug, "RTC INTERRUPT ON: %02X\n", StatusReg2);
            }
            break;

        case 0x20:
            if (InputPos <= 7)
                WriteDateTime(InputPos, val);
            break;

        case 0x60:
            if (InputPos <= 3)
                WriteDateTime(InputPos+4, val);
            break;

        case 0x10:
            if (StatusReg2 & 0x04)
            {
                if (InputPos <= 3)
                    Alarm1[InputPos-1] = val;
            }
            else
            {
                if (InputPos == 1)
                    Alarm1[2] = val;
            }
            break;

        case 0x50:
            if (InputPos <= 3)
                Alarm2[InputPos-1] = val;
            break;

        case 0x30:
            if (InputPos == 1)
            {
                ClockAdjust = val;
                Log(LogLevel::Debug, "RTC: CLOCK ADJUST = %02X\n", val);
            }
            break;

        case 0x70:
            if (InputPos == 1)
                FreeReg = val;
            break;
        }

        return;
    }
    else if ((CurCmd & 0x0F) == 0x0E)
    {
        if (NDS::ConsoleType != 1)
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
                FOUT1 = val;
            break;

        case 0x20:
            if (InputPos == 1)
                FOUT2 = val;
            break;

        case 0x10:
            if (InputPos <= 3)
                AlarmDate1[InputPos-1] = val;
            break;

        case 0x50:
            if (InputPos <= 3)
                AlarmDate2[InputPos-1] = val;
            break;

        default:
            Log(LogLevel::Debug, "RTC: unknown write command %02X\n", CurCmd);
            break;
        }

        return;
    }

    Log(LogLevel::Debug, "RTC: unknown write command %02X\n", CurCmd);
}

void ByteIn(u8 val)
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

        if (NDS::ConsoleType == 1)
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


u16 Read()
{
    //printf("RTC READ %04X\n", IO);
    return IO;
}

void Write(u16 val, bool byte)
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
