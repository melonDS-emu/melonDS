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

#include <stdio.h>
#include <string.h>
#include "DSi.h"
#include "DSi_Camera.h"


DSi_Camera* DSi_Camera0; // 78 / facing outside
DSi_Camera* DSi_Camera1; // 7A / selfie cam

u16 DSi_Camera::ModuleCnt;
u16 DSi_Camera::Cnt;

u8 DSi_Camera::FrameBuffer[640*480*4];
u32 DSi_Camera::FrameLength;
u32 DSi_Camera::TransferPos;

// note on camera data/etc intervals
// on hardware those are likely affected by several factors
// namely, how long cameras take to process frames
// camera IRQ is fired at roughly 15FPS with default config

const u32 kIRQInterval = 1120000; // ~30 FPS
const u32 kTransferStart = 60000;


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

    ModuleCnt = 0; // CHECKME
    Cnt = 0;

    memset(FrameBuffer, 0, 640*480*4);
    TransferPos = 0;
    FrameLength = 256*192*2; // TODO: make it check frame size, data type, etc

    NDS::ScheduleEvent(NDS::Event_DSi_CamIRQ, true, kIRQInterval, IRQ, 0);
}


void DSi_Camera::IRQ(u32 param)
{
    DSi_Camera* activecam = nullptr;

    // TODO: check which camera has priority if both are activated
    // (or does it just jumble both data sources together, like it
    // does for, say, overlapping VRAM?)
    if      (DSi_Camera0->IsActivated()) activecam = DSi_Camera0;
    else if (DSi_Camera1->IsActivated()) activecam = DSi_Camera1;

    if (activecam)
    {
        RequestFrame(activecam->Num);

        if (Cnt & (1<<11))
            NDS::SetIRQ(0, NDS::IRQ_DSi_Camera);

        if (Cnt & (1<<15))
            NDS::ScheduleEvent(NDS::Event_DSi_CamTransfer, false, kTransferStart, Transfer, 0);
    }

    NDS::ScheduleEvent(NDS::Event_DSi_CamIRQ, true, kIRQInterval, IRQ, 0);
}

void DSi_Camera::RequestFrame(u32 cam)
{
    if (!(Cnt & (1<<13))) printf("CAMERA: !! REQUESTING YUV FRAME\n");

    // TODO: picture size, data type, cropping, etc
    // generate test pattern
    // TODO: get picture from platform (actual camera, video file, whatever source)
    for (u32 y = 0; y < 192; y++)
    {
        for (u32 x = 0; x < 256; x++)
        {
            u16* px = (u16*)&FrameBuffer[((y*256) + x) * 2];

            if ((x & 0x8) ^ (y & 0x8))
                *px = 0x8000;
            else
                *px = 0xFC00 | ((y >> 3) << 5);
        }
    }
}

void DSi_Camera::Transfer(u32 pos)
{
    u32 numscan = (Cnt & 0x000F) + 1;
    u32 numpix = numscan * 256; // CHECKME

    // TODO: present data
    //printf("CAM TRANSFER POS=%d/%d\n", pos, 0x6000*2);

    DSi::CheckNDMAs(0, 0x0B);

    pos += numpix;
    if (pos >= 0x6000*2) // HACK
    {
        // transfer done
    }
    else
    {
        // keep going

        // TODO: must be tweaked such that each block has enough time to transfer
        u32 delay = numpix*2 + 16;

        NDS::ScheduleEvent(NDS::Event_DSi_CamTransfer, false, delay, Transfer, pos);
    }
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

    PLLDiv = 0x0366;
    PLLPDiv = 0x00F5;
    PLLCnt = 0x21F9;
    ClocksCnt = 0;
    StandbyCnt = 0x4029; // checkme
    MiscCnt = 0;
}

bool DSi_Camera::IsActivated()
{
    if (StandbyCnt & (1<<14)) return false; // standby
    if (!(MiscCnt & (1<<9))) return false; // data transfer not enabled

    return true;
}


void DSi_Camera::I2C_Start()
{
}

u8 DSi_Camera::I2C_Read(bool last)
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
            RegData = I2C_ReadReg(RegAddr);
            ret = RegData >> 8;
        }
    }

    if (last) DataPos = 0;
    else      DataPos++;

    return ret;
}

void DSi_Camera::I2C_Write(u8 val, bool last)
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
            I2C_WriteReg(RegAddr, RegData);
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

u16 DSi_Camera::I2C_ReadReg(u16 addr)
{
    switch (addr)
    {
    case 0x0000: return 0x2280; // chip ID
    case 0x0010: return PLLDiv;
    case 0x0012: return PLLPDiv;
    case 0x0014: return PLLCnt;
    case 0x0016: return ClocksCnt;
    case 0x0018: return StandbyCnt;
    case 0x001A: return MiscCnt;

    case 0x301A: return ((~StandbyCnt) & 0x4000) >> 12;
    }

    if(Num==1)printf("DSi_Camera%d: unknown read %04X\n", Num, addr);
    return 0;
}

void DSi_Camera::I2C_WriteReg(u16 addr, u16 val)
{
    switch (addr)
    {
    case 0x0010:
        PLLDiv = val & 0x3FFF;
        return;
    case 0x0012:
        PLLPDiv = val & 0xBFFF;
        return;
    case 0x0014:
        // shouldn't be instant either?
        val &= 0x7FFF;
        val |= ((val & 0x0002) << 14);
        PLLCnt = val;
        return;
    case 0x0016:
        ClocksCnt = val;
        printf("ClocksCnt=%04X\n", val);
        return;
    case 0x0018:
        // TODO: this shouldn't be instant, but uh
        val &= 0x003F;
        val |= ((val & 0x0001) << 14);
        StandbyCnt = val;
        printf("CAM%d STBCNT=%04X (%04X)\n", Num, StandbyCnt, val);
        return;
    case 0x001A:
        MiscCnt = val & 0x0B7B;
        printf("CAM%d MISCCNT=%04X (%04X)\n", Num, MiscCnt, val);
        return;
    }

    if(Num==1)printf("DSi_Camera%d: unknown write %04X %04X\n", Num, addr, val);
}


u8 DSi_Camera::Read8(u32 addr)
{
    //

    printf("unknown DSi cam read8 %08X\n", addr);
    return 0;
}

u16 DSi_Camera::Read16(u32 addr)
{
    switch (addr)
    {
    case 0x04004200: return ModuleCnt;
    case 0x04004202: return Cnt;
    }

    printf("unknown DSi cam read16 %08X\n", addr);
    return 0;
}

u32 DSi_Camera::Read32(u32 addr)
{
    switch (addr)
    {
    case 0x04004204:
        {
            // TODO
            return 0xFC00801F;
            /*if (!(Cnt & (1<<15))) return 0; // CHECKME
            u32 ret = *(u32*)&FrameBuffer[TransferPos];
            TransferPos += 4;
            if (TransferPos >= FrameLength) TransferPos = 0;
            dorp += 4;
            //if (dorp >= (256*4*2))
            if (TransferPos == 0)
            {
                dorp = 0;
                Cnt &= ~(1<<4);
            }
            return ret;*/
        }
    }

    printf("unknown DSi cam read32 %08X\n", addr);
    return 0;
}

void DSi_Camera::Write8(u32 addr, u8 val)
{
    //

    printf("unknown DSi cam write8 %08X %02X\n", addr, val);
}

void DSi_Camera::Write16(u32 addr, u16 val)
{
    switch (addr)
    {
    case 0x04004200:
        {
            u16 oldcnt = ModuleCnt;
            ModuleCnt = val;

            if ((ModuleCnt & (1<<1)) && !(oldcnt & (1<<1)))
            {
                // reset shit to zero
                // CHECKME

                Cnt = 0;
            }

            if ((ModuleCnt & (1<<5)) && !(oldcnt & (1<<5)))
            {
                // TODO: reset I2C??
            }
        }
        return;

    case 0x04004202:
        {
            // checkme
            u16 oldmask;
            if (Cnt & 0x8000)
            {
                val &= 0x8F20;
                oldmask = 0x601F;
            }
            else
            {
                val &= 0xEF2F;
                oldmask = 0x0010;
            }

            Cnt = (Cnt & oldmask) | (val & ~0x0020);
            if (val & (1<<5)) Cnt &= ~(1<<4);

            if ((val & (1<<15)) && !(Cnt & (1<<15)))
            {
                // start transfer
                //DSi::CheckNDMAs(0, 0x0B);
            }
        }
        return;
    }

    printf("unknown DSi cam write16 %08X %04X\n", addr, val);
}

void DSi_Camera::Write32(u32 addr, u32 val)
{
    //

    printf("unknown DSi cam write32 %08X %08X\n", addr, val);
}
