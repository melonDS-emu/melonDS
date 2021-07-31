/*
    Copyright 2016-2021 Arisotura

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

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "Config.h"
#include "NDS.h"
#include "RTC.h"


namespace RTC
{

u16 IO;

u8 Input;
u32 InputBit;
u32 InputPos;
u8 ClockInput[7];

u8 Output[8];
u32 OutputBit;
u32 OutputPos;

u8 CurCmd;

u8 StatusReg1;
u8 StatusReg2;
u8 Alarm1[3];
u8 Alarm2[3];
u8 ClockAdjust;
u8 FreeReg;

u32 TimeAtBoot;

bool Init()
{
    TimeAtBoot = Config::TimeAtBoot;
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
    memset(ClockInput, 0, sizeof(ClockInput));
    OutputPos = 0;

    CurCmd = 0;

    StatusReg1 = 0;
    StatusReg2 = 0;
    memset(Alarm1, 0, sizeof(Alarm1));
    memset(Alarm2, 0, sizeof(Alarm2));
    ClockAdjust = 0;
    FreeReg = 0;
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
    file->VarArray(Alarm1, sizeof(Alarm1));
    file->VarArray(Alarm2, sizeof(Alarm2));
    file->Var8(&ClockAdjust);
    file->Var8(&FreeReg);

    if (file->IsAtleastVersion(7, 2))
    {
        file->VarArray(ClockInput, sizeof(ClockInput));
        file->Var32(&TimeAtBoot);
    }
}


u8 BCD(u8 val)
{
    return (val % 10) | ((val / 10) << 4);
}
u8 FromBCD(u8 val)
{
    return (val & 0xF) + ((val >> 4) * 10);
}

time_t SecondsSinceBoot()
{
    // 560190 cycles per frame
    // 59.8261 frames per second (number taken from DeSmuME)
    return (time_t)((NDS::GetSysClockCycles(2) / 560190.0 + NDS::TotalFrames) / 59.8261);
}
tm GetTime()
{
    struct tm timedata;
    if (TimeAtBoot == 0)
    {
        time_t timestamp = time(NULL);
        localtime_r(&timestamp, &timedata);
    }
    else
    {
        time_t timestamp = TimeAtBoot + SecondsSinceBoot();
        gmtime_r(&timestamp, &timedata);
    }
    return timedata;
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

        if (CurCmd & 0x80)
        {
            switch (CurCmd & 0x70)
            {
            case 0x00: Output[0] = StatusReg1; break;
            case 0x40: Output[0] = StatusReg2; break;

            case 0x20:
                {
                    struct tm timedata = GetTime();

                    Output[0] = BCD(timedata.tm_year - 100);
                    Output[1] = BCD(timedata.tm_mon + 1);
                    Output[2] = BCD(timedata.tm_mday);
                    Output[3] = BCD(timedata.tm_wday);
                    Output[4] = BCD(timedata.tm_hour);
                    Output[5] = BCD(timedata.tm_min);
                    Output[6] = BCD(timedata.tm_sec);
                }
                break;

            case 0x60:
                {
                    struct tm timedata = GetTime();

                    Output[0] = BCD(timedata.tm_hour);
                    Output[1] = BCD(timedata.tm_min);
                    Output[2] = BCD(timedata.tm_sec);
                }
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
        }
        return;
    }

    switch (CurCmd & 0x70)
    {
    case 0x00:
        if (InputPos == 1) StatusReg1 = val & 0x0E;
        break;

    case 0x40:
        if (InputPos == 1) StatusReg2 = val;
        if (StatusReg2 & 0x4F) printf("RTC INTERRUPT ON: %02X\n", StatusReg2);
        break;

    case 0x20:
        ClockInput[InputPos - 1] = FromBCD(val);
        if (InputPos == 7)
        {
            tm newTime;
            memset(&newTime, 0, sizeof(newTime));
            newTime.tm_year = ClockInput[0] + 100;
            newTime.tm_mon = ClockInput[1] - 1;
            newTime.tm_mday = ClockInput[2];
            newTime.tm_wday = ClockInput[3];
            // Idk why, but whenever I set the hour to 12-23 it comes out as 52-63.
            if (ClockInput[4] > 50) ClockInput[4] -= 40;
            newTime.tm_hour = ClockInput[4];
            newTime.tm_min = ClockInput[5];
            newTime.tm_sec = ClockInput[6];
            time_t timestamp = mktime(&newTime);
            // mktime uses local time zone; get back to utc
            tm* utc = gmtime(&timestamp);
            time_t t2 = mktime(utc);
            timestamp += timestamp - t2;

            TimeAtBoot = timestamp - SecondsSinceBoot();
        }
        break;

    case 0x60:
        // same shit
        break;

    case 0x10:
        if (StatusReg2 & 0x04)
        {
            if (InputPos <= 3) Alarm1[InputPos-1] = val;
        }
        else
        {
            if (InputPos == 1) Alarm1[2] = val;
        }
        break;

    case 0x50:
        if (InputPos <= 3) Alarm2[InputPos-1] = val;
        break;

    case 0x30:
        if (InputPos == 1) ClockAdjust = val;
        break;

    case 0x70:
        if (InputPos == 1) FreeReg = val;
        break;
    }
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
