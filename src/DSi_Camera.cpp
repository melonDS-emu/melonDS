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

#include <algorithm>
#include <stdio.h>
#include <string.h>
#include "DSi.h"
#include "DSi_Camera.h"
#include "Platform.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;


// note on camera data/etc intervals
// on hardware those are likely affected by several factors
// namely, how long cameras take to process frames
// camera IRQ is fired at roughly 15FPS with default config

const u32 DSi_CamModule::kIRQInterval = 1120000; // ~30 FPS
const u32 DSi_CamModule::kTransferStart = 60000;


DSi_CamModule::DSi_CamModule(melonDS::DSi& dsi) : DSi(dsi)
{
    DSi.RegisterEventFunc(Event_DSi_CamIRQ, 0, MemberEventFunc(DSi_CamModule, IRQ));
    DSi.RegisterEventFunc(Event_DSi_CamTransfer, 0, MemberEventFunc(DSi_CamModule, TransferScanline));

    Camera0 = DSi.I2C.GetOuterCamera();
    Camera1 = DSi.I2C.GetInnerCamera();
}

DSi_CamModule::~DSi_CamModule()
{
    Camera0 = nullptr;
    Camera1 = nullptr;

    DSi.UnregisterEventFunc(Event_DSi_CamIRQ, 0);
    DSi.UnregisterEventFunc(Event_DSi_CamTransfer, 0);
}

void DSi_CamModule::Reset()
{
    ModuleCnt = 0; // CHECKME
    Cnt = 0;

    CropStart = 0;
    CropEnd = 0;

    memset(DataBuffer, 0, 512*sizeof(u32));
    BufferReadPos = 0;
    BufferWritePos = 0;
    BufferNumLines = 0;
    CurCamera = nullptr;

    DSi.ScheduleEvent(Event_DSi_CamIRQ, false, kIRQInterval, 0, 0);
}

void DSi_CamModule::Stop()
{
    Camera0->Stop();
    Camera1->Stop();
}

void DSi_CamModule::DoSavestate(Savestate* file)
{
    file->Section("CAMi");

    file->Var16(&ModuleCnt);
    file->Var16(&Cnt);

    /*file->VarArray(FrameBuffer, sizeof(FrameBuffer));
    file->Var32(&TransferPos);
    file->Var32(&FrameLength);*/
}


void DSi_CamModule::IRQ(u32 param)
{
    DSi_Camera* activecam = nullptr;

    // TODO: cameras don't have any priority!
    // activating both together will jumble the image data together
    if      (Camera0->IsActivated()) activecam = Camera0;
    else if (Camera1->IsActivated()) activecam = Camera1;

    if (activecam)
    {
        activecam->StartTransfer();

        if (Cnt & (1<<11))
            DSi.SetIRQ(0, IRQ_DSi_Camera);

        if (Cnt & (1<<15))
        {
            BufferReadPos = 0;
            BufferWritePos = 0;
            BufferNumLines = 0;
            CurCamera = activecam;
            DSi.ScheduleEvent(Event_DSi_CamTransfer, false, kTransferStart, 0, 0);
        }
    }

    DSi.ScheduleEvent(Event_DSi_CamIRQ, true, kIRQInterval, 0, 0);
}

void DSi_CamModule::TransferScanline(u32 line)
{
    u32* dstbuf = &DataBuffer[BufferWritePos];
    int maxlen = 512 - BufferWritePos;

    u32 tmpbuf[512];
    int datalen = CurCamera->TransferScanline(tmpbuf, 512);

    // TODO: must be tweaked such that each block has enough time to transfer
    u32 delay = datalen*4 + 16;

    int copystart = 0;
    int copylen = datalen;

    if (Cnt & (1<<14))
    {
        // crop picture

        int ystart = (CropStart >> 16) & 0x1FF;
        int yend = (CropEnd >> 16) & 0x1FF;
        if (line < ystart || line > yend)
        {
            if (!CurCamera->TransferDone())
                DSi.ScheduleEvent(Event_DSi_CamTransfer, false, delay, 0, line+1);

            return;
        }

        int xstart = (CropStart >> 1) & 0x1FF;
        int xend = (CropEnd >> 1) & 0x1FF;

        copystart = xstart;
        copylen = xend+1 - xstart;

        if ((copystart + copylen) > datalen)
            copylen = datalen - copystart;
        if (copylen < 0)
            copylen = 0;
    }

    if (copylen > maxlen)
    {
        copylen = maxlen;
        Cnt |= (1<<4);
    }

    if (Cnt & (1<<13))
    {
        // convert to RGB

        for (u32 i = 0; i < copylen; i++)
        {
            u32 val = tmpbuf[copystart + i];

            int y1 = val & 0xFF;
            int u = (val >> 8) & 0xFF;
            int y2 = (val >> 16) & 0xFF;
            int v = (val >> 24) & 0xFF;

            u -= 128; v -= 128;

            int r1 = y1 + ((v * 91881) >> 16);
            int g1 = y1 - ((v * 46793) >> 16) - ((u * 22544) >> 16);
            int b1 = y1 + ((u * 116129) >> 16);

            int r2 = y2 + ((v * 91881) >> 16);
            int g2 = y2 - ((v * 46793) >> 16) - ((u * 22544) >> 16);
            int b2 = y2 + ((u * 116129) >> 16);

            r1 = std::clamp(r1, 0, 255); g1 = std::clamp(g1, 0, 255); b1 = std::clamp(b1, 0, 255);
            r2 = std::clamp(r2, 0, 255); g2 = std::clamp(g2, 0, 255); b2 = std::clamp(b2, 0, 255);

            u32 col1 = (r1 >> 3) | ((g1 >> 3) << 5) | ((b1 >> 3) << 10) | 0x8000;
            u32 col2 = (r2 >> 3) | ((g2 >> 3) << 5) | ((b2 >> 3) << 10) | 0x8000;

            dstbuf[i] = col1 | (col2 << 16);
        }
    }
    else
    {
        // return raw data

        memcpy(dstbuf, &tmpbuf[copystart], copylen*sizeof(u32));
    }

    u32 numscan = Cnt & 0x000F;
    if (BufferNumLines >= numscan)
    {
        BufferReadPos = 0; // checkme
        BufferWritePos = 0;
        BufferNumLines = 0;
        DSi.CheckNDMAs(0, 0x0B);
    }
    else
    {
        BufferWritePos += copylen;
        if (BufferWritePos > 512) BufferWritePos = 512;
        BufferNumLines++;
    }

    if (CurCamera->TransferDone())
        return;

    DSi.ScheduleEvent(Event_DSi_CamTransfer, false, delay, 0, line+1);
}


u8 DSi_CamModule::Read8(u32 addr)
{
    //

    Log(LogLevel::Debug, "unknown DSi cam read8 %08X\n", addr);
    return 0;
}

u16 DSi_CamModule::Read16(u32 addr)
{
    switch (addr)
    {
    case 0x04004200: return ModuleCnt;
    case 0x04004202: return Cnt;
    }

    Log(LogLevel::Debug, "unknown DSi cam read16 %08X\n", addr);
    return 0;
}

u32 DSi_CamModule::Read32(u32 addr)
{
    switch (addr)
    {
    case 0x04004204:
        {
            u32 ret = DataBuffer[BufferReadPos];
            if (Cnt & (1<<15))
            {
                if (BufferReadPos < 511)
                    BufferReadPos++;
                // CHECKME!!!!
                // also presumably we should set bit4 in Cnt if there's no new data to be read
            }

            return ret;
        }

    case 0x04004210: return CropStart;
    case 0x04004214: return CropEnd;
    }

    Log(LogLevel::Debug, "unknown DSi cam read32 %08X\n", addr);
    return 0;
}

void DSi_CamModule::Write8(u32 addr, u8 val)
{
    //

    Log(LogLevel::Debug, "unknown DSi cam write8 %08X %02X\n", addr, val);
}

void DSi_CamModule::Write16(u32 addr, u16 val)
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
            // TODO: during a transfer, clearing bit15 does not reflect immediately
            // maybe it needs to finish the trasnfer or atleast the current block

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
            if (val & (1<<5))
            {
                Cnt &= ~(1<<4);
                BufferReadPos = 0;
                BufferWritePos = 0;
            }

            if ((val & (1<<15)) && !(Cnt & (1<<15)))
            {
                // start transfer
                //DSi::CheckNDMAs(0, 0x0B);
            }
        }
        return;

    case 0x04004210:
        if (Cnt & (1<<15)) return;
        CropStart = (CropStart & 0x01FF0000) | (val & 0x03FE);
        return;
    case 0x04004212:
        if (Cnt & (1<<15)) return;
        CropStart = (CropStart & 0x03FE) | ((val & 0x01FF) << 16);
        return;
    case 0x04004214:
        if (Cnt & (1<<15)) return;
        CropEnd = (CropEnd & 0x01FF0000) | (val & 0x03FE);
        return;
    case 0x04004216:
        if (Cnt & (1<<15)) return;
        CropEnd = (CropEnd & 0x03FE) | ((val & 0x01FF) << 16);
        return;
    }

    Log(LogLevel::Debug, "unknown DSi cam write16 %08X %04X\n", addr, val);
}

void DSi_CamModule::Write32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x04004210:
        if (Cnt & (1<<15)) return;
        CropStart = val & 0x01FF03FE;
        return;
    case 0x04004214:
        if (Cnt & (1<<15)) return;
        CropEnd = val & 0x01FF03FE;
        return;
    }

    Log(LogLevel::Debug, "unknown DSi cam write32 %08X %08X\n", addr, val);
}



DSi_Camera::DSi_Camera(melonDS::DSi& dsi, DSi_I2CHost* host, u32 num) : DSi_I2CDevice(dsi, host), Num(num)
{
}

DSi_Camera::~DSi_Camera()
{
}

void DSi_Camera::DoSavestate(Savestate* file)
{
    char magic[5] = "CAMx";
    magic[3] = '0' + Num;
    file->Section(magic);

    file->Var32(&DataPos);
    file->Var32(&RegAddr);
    file->Var16(&RegData);

    file->Var16(&PLLDiv);
    file->Var16(&PLLPDiv);
    file->Var16(&PLLCnt);
    file->Var16(&ClocksCnt);
    file->Var16(&StandbyCnt);
    file->Var16(&MiscCnt);

    file->Var16(&MCUAddr);
    file->VarArray(MCURegs, 0x8000);
}

void DSi_Camera::Reset()
{
    Platform::Camera_Stop(Num);

    DataPos = 0;
    RegAddr = 0;
    RegData = 0;

    PLLDiv = 0x0366;
    PLLPDiv = 0x00F5;
    PLLCnt = 0x21F9;
    ClocksCnt = 0;
    StandbyCnt = 0x4029; // checkme
    MiscCnt = 0;

    MCUAddr = 0;
    memset(MCURegs, 0, 0x8000);

    // default state is preview mode (checkme)
    MCURegs[0x2104] = 3;

    TransferY = 0;
    memset(FrameBuffer, 0, (640*480/2)*sizeof(u32));
}

void DSi_Camera::Stop()
{
    Platform::Camera_Stop(Num);
}

bool DSi_Camera::IsActivated() const
{
    if (StandbyCnt & (1<<14)) return false; // standby
    if (!(MiscCnt & (1<<9))) return false; // data transfer not enabled

    return true;
}


void DSi_Camera::StartTransfer()
{
    TransferY = 0;

    u8 state = MCURegs[0x2104];
    if (state == 3) // preview
    {
        FrameWidth = *(u16*)&MCURegs[0x2703];
        FrameHeight = *(u16*)&MCURegs[0x2705];
        FrameReadMode = *(u16*)&MCURegs[0x2717];
        FrameFormat = *(u16*)&MCURegs[0x2755];
    }
    else if (state == 7) // capture
    {
        FrameWidth = *(u16*)&MCURegs[0x2707];
        FrameHeight = *(u16*)&MCURegs[0x2709];
        FrameReadMode = *(u16*)&MCURegs[0x272D];
        FrameFormat = *(u16*)&MCURegs[0x2757];
    }
    else
    {
        FrameWidth = 0;
        FrameHeight = 0;
        FrameReadMode = 0;
        FrameFormat = 0;
    }

    Platform::Camera_CaptureFrame(Num, FrameBuffer, 640, 480, true);
}

bool DSi_Camera::TransferDone() const
{
    return TransferY >= FrameHeight;
}

int DSi_Camera::TransferScanline(u32* buffer, int maxlen)
{
    if (TransferY >= FrameHeight)
        return 0;

    if (FrameWidth > 640 || FrameHeight > 480 ||
        FrameWidth < 2 || FrameHeight < 2 ||
        (FrameWidth & 1))
    {
        // TODO work out something for these cases?
        Log(LogLevel::Warn, "CAM%d: invalid resolution %dx%d\n", Num, FrameWidth, FrameHeight);
        //memset(buffer, 0, width*height*sizeof(u16));
        return 0;
    }

    // TODO: non-YUV pixel formats and all

    int retlen = FrameWidth >> 1;
    int sy = (TransferY * 480) / FrameHeight;
    if (FrameReadMode & (1<<1))
        sy = 479 - sy;

    if (FrameReadMode & (1<<0))
    {
        for (int dx = 0; dx < retlen; dx++)
        {
            if (dx >= maxlen) break;

            int sx = (dx * 640) / FrameWidth;

            u32 val = FrameBuffer[sy*320 + sx];
            buffer[dx] = val;
        }
    }
    else
    {
        for (int dx = 0; dx < retlen; dx++)
        {
            if (dx >= maxlen) break;

            int sx = 319 - ((dx * 640) / FrameWidth);

            u32 val = FrameBuffer[sy*320 + sx];
            buffer[dx] = (val & 0xFF00FF00) | ((val >> 16) & 0xFF) | ((val & 0xFF) << 16);
        }
    }

    TransferY++;

    return retlen;
}


void DSi_Camera::Acquire()
{
    DataPos = 0;
}

u8 DSi_Camera::Read(bool last)
{
    u8 ret;

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

        if (RegAddr & 0x1) Log(LogLevel::Warn, "DSi_Camera: !! UNALIGNED REG ADDRESS %04X\n", RegAddr);
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

u16 DSi_Camera::I2C_ReadReg(u16 addr) const
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

    case 0x098C: return MCUAddr;
    case 0x0990:
    case 0x0992:
    case 0x0994:
    case 0x0996:
    case 0x0998:
    case 0x099A:
    case 0x099C:
    case 0x099E:
        {
            addr -= 0x0990;
            u16 ret = MCU_Read((MCUAddr & 0x7FFF) + addr);
            if (!(MCUAddr & (1<<15)))
                ret |= (MCU_Read((MCUAddr & 0x7FFF) + addr+1) << 8);
            return ret;
        }

    case 0x301A: return ((~StandbyCnt) & 0x4000) >> 12;
    }

    if(Num==1) Log(LogLevel::Debug, "DSi_Camera%d: unknown read %04X\n", Num, addr);
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
        //printf("ClocksCnt=%04X\n", val);
        return;
    case 0x0018:
        {
            bool wasactive = IsActivated();
            // TODO: this shouldn't be instant, but uh
            val &= 0x003F;
            val |= ((val & 0x0001) << 14);
            StandbyCnt = val;
            //printf("CAM%d STBCNT=%04X (%04X)\n", Num, StandbyCnt, val);
            bool isactive = IsActivated();
            if (isactive && !wasactive)      Platform::Camera_Start(Num);
            else if (wasactive && !isactive) Platform::Camera_Stop(Num);
        }
        return;
    case 0x001A:
        {
            bool wasactive = IsActivated();
            MiscCnt = val & 0x0B7B;
            //printf("CAM%d MISCCNT=%04X (%04X)\n", Num, MiscCnt, val);
            bool isactive = IsActivated();
            if (isactive && !wasactive)      Platform::Camera_Start(Num);
            else if (wasactive && !isactive) Platform::Camera_Stop(Num);
        }
        return;

    case 0x098C:
        MCUAddr = val;
        return;
    case 0x0990:
    case 0x0992:
    case 0x0994:
    case 0x0996:
    case 0x0998:
    case 0x099A:
    case 0x099C:
    case 0x099E:
        addr -= 0x0990;
        MCU_Write((MCUAddr & 0x7FFF) + addr, val&0xFF);
        if (!(MCUAddr & (1<<15)))
            MCU_Write((MCUAddr & 0x7FFF) + addr+1, val>>8);
        return;
    }

    if(Num==1) Log(LogLevel::Debug, "DSi_Camera%d: unknown write %04X %04X\n", Num, addr, val);
}


// TODO: not sure at all what is the accessible range
// or if there is any overlap in the address range

u8 DSi_Camera::MCU_Read(u16 addr) const
{
    addr &= 0x7FFF;

    return MCURegs[addr];
}

void DSi_Camera::MCU_Write(u16 addr, u8 val)
{
    addr &= 0x7FFF;

    switch (addr)
    {
    case 0x2103: // SEQ_CMD
        MCURegs[addr] = 0;
        if      (val == 2) MCURegs[0x2104] = 7; // capture mode
        else if (val == 1) MCURegs[0x2104] = 3; // preview mode
        else if (val != 5 && val != 6)
            Log(LogLevel::Debug, "CAM%d: atypical SEQ_CMD %04X\n", Num, val);
        return;

    case 0x2104: // SEQ_STATE, read-only
        return;
    }

    MCURegs[addr] = val;
}


void DSi_Camera::InputFrame(const u32* data, int width, int height, bool rgb)
{
    // TODO: double-buffering?

    if (width == 640 && height == 480 && !rgb)
    {
        memcpy(FrameBuffer, data, (640*480/2)*sizeof(u32));
        return;
    }

    if (rgb)
    {
        for (int dy = 0; dy < 480; dy++)
        {
            int sy = (dy * height) / 480;

            for (int dx = 0; dx < 640; dx+=2)
            {
                int sx;

                sx = (dx * width) / 640;
                u32 pixel1 = data[sy*width + sx];

                sx = ((dx+1) * width) / 640;
                u32 pixel2 = data[sy*width + sx];

                int r1 = (pixel1 >> 16) & 0xFF;
                int g1 = (pixel1 >> 8) & 0xFF;
                int b1 = pixel1 & 0xFF;

                int r2 = (pixel2 >> 16) & 0xFF;
                int g2 = (pixel2 >> 8) & 0xFF;
                int b2 = pixel2 & 0xFF;

                int y1 = ((r1 * 19595) + (g1 * 38470) + (b1 * 7471)) >> 16;
                int u1 = ((b1 - y1) * 32244) >> 16;
                int v1 = ((r1 - y1) * 57475) >> 16;

                int y2 = ((r2 * 19595) + (g2 * 38470) + (b2 * 7471)) >> 16;
                int u2 = ((b2 - y2) * 32244) >> 16;
                int v2 = ((r2 - y2) * 57475) >> 16;

                u1 += 128; v1 += 128;
                u2 += 128; v2 += 128;

                y1 = std::clamp(y1, 0, 255); u1 = std::clamp(u1, 0, 255); v1 = std::clamp(v1, 0, 255);
                y2 = std::clamp(y2, 0, 255); u2 = std::clamp(u2, 0, 255); v2 = std::clamp(v2, 0, 255);

                // huh
                u1 = (u1 + u2) >> 1;
                v1 = (v1 + v2) >> 1;

                FrameBuffer[(dy*640 + dx) / 2] = y1 | (u1 << 8) | (y2 << 16) | (v1 << 24);
            }
        }
    }
    else
    {
        for (int dy = 0; dy < 480; dy++)
        {
            int sy = (dy * height) / 480;

            for (int dx = 0; dx < 640; dx+=2)
            {
                int sx = (dx * width) / 640;

                FrameBuffer[(dy*640 + dx) / 2] = data[(sy*width + sx) / 2];
            }
        }
    }
}

}