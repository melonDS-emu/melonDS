/*
    Copyright 2016-2020 Arisotura

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

#include <stdio.h>
#include <string.h>
#include "DSi_Camera.h"


DSi_Camera* DSi_Camera0; // 78 / facing outside
DSi_Camera* DSi_Camera1; // 7A / selfie cam


bool DSi_Camera::Init()
{
    DSi_Camera0 = new DSi_Camera(0);
    DSi_Camera1 = new DSi_Camera(1);

    return true;
}

void DSi_Camera::DeInit()
{
    delete DSi_Camera0;
    delete DSi_Camera1;
}

void DSi_Camera::Reset()
{
    DSi_Camera0->ResetCam();
    DSi_Camera1->ResetCam();
}


DSi_Camera::DSi_Camera(u32 num)
{
    Num = num;
}

DSi_Camera::~DSi_Camera()
{
    //
}

void DSi_Camera::ResetCam()
{
    DataPos = 0;
    RegAddr = 0;
    RegData = 0;

    PLLCnt = 0;
    StandbyCnt = 0x4029; // checkme
}


void DSi_Camera::Start()
{
}

u8 DSi_Camera::Read(bool last)
{
    u8 ret;

    if (DataPos < 2)
    {
        printf("DSi_Camera: WHAT??\n");
        ret = 0;
    }
    else
    {
        if (DataPos & 0x1)
        {
            ret = RegData & 0xFF;
            RegAddr += 2; // checkme
        }
        else
        {
            RegData = ReadReg(RegAddr);
            ret = RegData >> 8;
        }
    }

    if (last) DataPos = 0;
    else      DataPos++;

    return ret;
}

void DSi_Camera::Write(u8 val, bool last)
{
    if (DataPos < 2)
    {
        if (DataPos == 0)
            RegAddr = val << 8;
        else
            RegAddr |= val;

        if (RegAddr & 0x1) printf("DSi_Camera: !! UNALIGNED REG ADDRESS %04X\n", RegAddr);
    }
    else
    {
        if (DataPos & 0x1)
        {
            RegData |= val;
            WriteReg(RegAddr, RegData);
            RegAddr += 2; // checkme
        }
        else
        {
            RegData = val << 8;
        }
    }

    if (last) DataPos = 0;
    else      DataPos++;
}

u16 DSi_Camera::ReadReg(u16 addr)
{
    switch (addr)
    {
    case 0x0000: return 0x2280; // chip ID
    case 0x0014: return PLLCnt;
    case 0x0018: return StandbyCnt;

    case 0x301A: return ((~StandbyCnt) & 0x4000) >> 12;
    }

    //printf("DSi_Camera%d: unknown read %04X\n", Num, addr);
    return 0;
}

void DSi_Camera::WriteReg(u16 addr, u16 val)
{
    switch (addr)
    {
    case 0x0014:
        // shouldn't be instant either?
        val &= 0x7FFF;
        val |= ((val & 0x0002) << 14);
        PLLCnt = val;
        return;
    case 0x0018:
        // TODO: this shouldn't be instant, but uh
        val &= 0x003F;
        val |= ((val & 0x0001) << 14);
        StandbyCnt = val;
        return;
    }

    //printf("DSi_Camera%d: unknown write %04X %04X\n", Num, addr, val);
}
