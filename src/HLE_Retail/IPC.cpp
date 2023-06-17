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

#include "../NDS.h"
#include "../NDSCart.h"
#include "../HLE.h"
#include "../FIFO.h"
#include "../SPU.h"

#include "IPC.h"
#include "Sound_Nitro.h"
#include "Sound_Peach.h"
#include "Wifi.h"


namespace NDS
{
extern u16 IPCSync9, IPCSync7;
extern u16 IPCFIFOCnt9, IPCFIFOCnt7;
extern FIFO<u32, 16> IPCFIFO9; // FIFO in which the ARM9 writes
extern FIFO<u32, 16> IPCFIFO7;
}

namespace SPI_Firmware
{
extern u8* Firmware;
extern u32 FirmwareMask;
u8 Read();
void Write(u8 val, u32 hold);
}

namespace SPI_Powerman
{
extern u8 Registers[8];
extern u8 RegMasks[8];
u8 Read();
void Write(u8 val, u32 hold);
}

namespace SPI_TSC
{
extern u16 TouchX, TouchY;
}

namespace RTC
{
extern u32 InputPos;
extern u8 Output[8];
void ByteIn(u8 val);
}


namespace HLE
{
namespace Retail
{

u16 FW_Data[16];

int TS_Status;
u16 TS_Data[16];
u16 TS_NumSamples;
u16 TS_SamplePos[4];

int Sound_Engine;

u16 PM_Data[16];

u16 Mic_Data[16];

u32 SM_Command;
u32 SM_DataPos;
u32 SM_Buffer;


void SendIPCSync(u8 val)
{
    NDS::IPCSync9 = (NDS::IPCSync9 & 0xFFF0) | (val & 0xF);
}

void SendIPCReply(u32 service, u32 data, u32 flag)
{
    u32 val = (service & 0x1F) | (data << 6) | ((flag & 0x1) << 5);

    if (NDS::IPCFIFO7.IsFull())
        printf("!!!! IPC FIFO FULL\n");
    else
    {
        bool wasempty = NDS::IPCFIFO7.IsEmpty();
        NDS::IPCFIFO7.Write(val);
        if ((NDS::IPCFIFOCnt9 & 0x0400) && wasempty)
            NDS::SetIRQ(0, NDS::IRQ_IPCRecv);
    }
}


void Reset()
{
    memset(FW_Data, 0, sizeof(FW_Data));

    TS_Status = 0;
    memset(TS_Data, 0, sizeof(TS_Data));
    TS_NumSamples = 0;
    memset(TS_SamplePos, 0, sizeof(TS_SamplePos));

    Sound_Engine = -1;

    memset(PM_Data, 0, sizeof(PM_Data));

    memset(Mic_Data, 0, sizeof(Mic_Data));

    SM_Command = 0;
    SM_DataPos = 0;
    SM_Buffer = 0;

    Wifi::Reset();

    NDS::ScheduleEvent(NDS::Event_HLE_PollInput, true, 134016, PollInput, 0);
}


void Touchscreen_Sample()
{
    u32 ts = NDS::ARM7Read16(0x027FFFAA) | (NDS::ARM7Read16(0x027FFFAC) << 16);

    if (SPI_TSC::TouchY == 0xFFF)
    {
        ts &= 0xFE000000;
        ts |= 0x06000000;
    }
    else
    {
        ts &= 0xF9000000;
        ts |= (SPI_TSC::TouchX & 0xFFF);
        ts |= ((SPI_TSC::TouchY & 0xFFF) << 12);
        ts |= 0x01000000;
    }

    NDS::ARM7Write16(0x027FFFAA, ts & 0xFFFF);
    NDS::ARM7Write16(0x027FFFAC, ts >> 16);
}

void StartScanline(u32 line)
{
    for (int i = 0; i < TS_NumSamples; i++)
    {
        if (line == TS_SamplePos[i])
        {
            Touchscreen_Sample();
            SendIPCReply(0x6, 0x03009000 | i);
            break;
        }
    }
}

void PollInput(u32 param)
{
    u16 input = (NDS::KeyInput >> 6) & 0x2C00;
    if (NDS::IsLidClosed()) input |= (1<<15);

    NDS::ARM7Write16(0x027FFFA8, input);

    NDS::ScheduleEvent(NDS::Event_HLE_PollInput, true, 134016, PollInput, 0);
}


void OnIPCSync()
{
    u8 val = NDS::IPCSync7 & 0xF;

    if (val < 5)
    {
        SendIPCSync(val+1);
    }
    else if (val == 5)
    {
        SendIPCSync(0);

        // presumably ARM7-side ready flags for each IPC service
        NDS::ARM7Write32(0x027FFF8C, 0x0000FFF0);
    }
}


void OnIPCRequest_Firmware(u32 data)
{
    if (data & (1<<25))
    {
        memset(FW_Data, 0, sizeof(FW_Data));
    }

    FW_Data[(data >> 16) & 0xF] = data & 0xFFFF;

    if (!(data & (1<<24))) return;

    u32 cmd = (FW_Data[0] >> 8) - 0x20;
    switch (cmd)
    {
    case 0: // write enable
        SPI_Firmware::Write(0x06, false);
        SendIPCReply(0x4, 0x0300A000);
        break;

    case 1: // write disable
        SPI_Firmware::Write(0x04, false);
        SendIPCReply(0x4, 0x0300A100);
        break;

    case 2: // read status register
        {
            u32 addr = ((FW_Data[0] & 0xFF) << 24) | (FW_Data[1] << 8) | ((FW_Data[2] >> 8) & 0xFF);
            if (addr < 0x02000000 || addr >= 0x02800000)
            {
                SendIPCReply(0x4, 0x0300A202);
                break;
            }

            SPI_Firmware::Write(0x05, true);
            SPI_Firmware::Write(0, false);
            u8 ret = SPI_Firmware::Read();
            NDS::ARM7Write8(addr, ret);

            SendIPCReply(0x4, 0x0300A200);
        }
        break;

    case 3: // firmware read
        {
            u32 addr = (FW_Data[4] << 16) | FW_Data[5];
            if (addr < 0x02000000 || addr >= 0x02800000)
            {
                SendIPCReply(0x4, 0x0300A302);
                break;
            }

            u32 src = ((FW_Data[0] & 0xFF) << 16) | FW_Data[1];
            u32 len = (FW_Data[2] << 16) | FW_Data[3];

            for (u32 i = 0; i < len; i++)
            {
                u8 val = SPI_Firmware::Firmware[src & SPI_Firmware::FirmwareMask];
                NDS::ARM7Write8(addr, val);
                src++;
                addr++;
            }

            SendIPCReply(0x4, 0x0300A300);
        }
        break;

    case 5: // firmware write
        {
            u32 addr = (FW_Data[3] << 16) | FW_Data[4];
            if (addr < 0x02000000 || addr >= 0x02800000)
            {
                SendIPCReply(0x4, 0x0300A502);
                break;
            }

            u32 dst = ((FW_Data[0] & 0xFF) << 16) | FW_Data[1];
            u32 len = FW_Data[2];

            for (u32 i = 0; i < len; i++)
            {
                u8 val = NDS::ARM7Read8(addr);
                SPI_Firmware::Firmware[dst & SPI_Firmware::FirmwareMask] = val;
                dst++;
                addr++;
            }

            // hack: trigger firmware save
            SPI_Firmware::Write(0x0A, true);
            SPI_Firmware::Write(0, false);

            SendIPCReply(0x4, 0x0300A500);
        }
        break;

    default:
        printf("unknown FW request %08X (%04X)\n", data, FW_Data[0]);
        break;
    }
}

void RTC_Read(u8 reg, u32 addr, u32 len)
{
    RTC::InputPos = 0;
    RTC::ByteIn(reg | 0x80);

    for (u32 i = 0; i < len; i++)
    {
        NDS::ARM7Write8(addr+i, RTC::Output[i]);
    }
}

void OnIPCRequest_RTC(u32 data)
{
    u32 cmd = (data >> 8) & 0x7F;

    if ((cmd >= 2 && cmd <= 15) ||
        (cmd >= 26 && cmd <= 34) ||
        (cmd >= 42))
    {
        SendIPCReply(0x5, 0x8001 | (cmd << 8));
        return;
    }

    switch (cmd)
    {
    case 0x10: // read date and time
        RTC_Read(0x20, 0x027FFDE8, 7);
        SendIPCReply(0x5, 0x9000);
        break;

    case 0x11: // read date
        RTC_Read(0x20, 0x027FFDE8, 4);
        SendIPCReply(0x5, 0x9100);
        break;

    case 0x12: // read time
        RTC_Read(0x60, 0x027FFDE8+4, 3);
        SendIPCReply(0x5, 0x9200);
        break;

    default:
        printf("HLE: unknown RTC command %02X (%08X)\n", cmd, data);
        break;
    }
}

void OnIPCRequest_Touchscreen(u32 data)
{
    if (data & (1<<25))
    {
        memset(TS_Data, 0, sizeof(TS_Data));
    }

    TS_Data[(data >> 16) & 0xF] = data & 0xFFFF;

    if (!(data & (1<<24))) return;

    switch (TS_Data[0] >> 8)
    {
    case 0: // manual sampling
        {
            Touchscreen_Sample();
            SendIPCReply(0x6, 0x03008000);
        }
        break;

    case 1: // setup auto sampling
        {
            if (TS_Status != 0)
            {
                SendIPCReply(0x6, 0x03008103);
                break;
            }

            // samples per frame
            u8 num = TS_Data[0] & 0xFF;
            if (num == 0 || num > 4)
            {
                SendIPCReply(0x6, 0x03008102);
                break;
            }

            // offset in scanlines for first sample
            u16 offset = TS_Data[1];
            if (offset >= 263)
            {
                SendIPCReply(0x6, 0x03008102);
                break;
            }

            TS_Status = 1;

            TS_NumSamples = num;
            for (int i = 0; i < num; i++)
            {
                u32 ypos = (offset + ((i * 263) / num)) % 263;
                TS_SamplePos[i] = ypos;
            }

            TS_Status = 2;
            SendIPCReply(0x6, 0x03008100);
        }
        break;

    case 2: // stop autosampling
        {
            if (TS_Status != 2)
            {
                SendIPCReply(0x6, 0x03008103);
                break;
            }

            TS_Status = 3;

            // TODO CHECKME
            TS_NumSamples = 0;

            TS_Status = 0;
            SendIPCReply(0x6, 0x03008200);
        }
        break;

    case 3: // manual sampling but with condition (TODO)
        {
            Touchscreen_Sample();
            SendIPCReply(0x6, 0x03008300);
        }
        break;

    default:
        printf("unknown TS request %08X\n", data);
        break;
    }
}

void OnIPCRequest_Sound(u32 data)
{
    if (Sound_Engine == -1)
    {
        if (data >= 0x02000000)
        {
            Sound_Engine = 0;
            Sound_Nitro::Reset();
        }
        else
        {
            Sound_Engine = 1;
            Sound_Peach::Reset();
        }
    }

    if (Sound_Engine == 0) return Sound_Nitro::OnIPCRequest(data);
    if (Sound_Engine == 1) return Sound_Peach::OnIPCRequest(data);
}

void OnIPCRequest_Powerman(u32 data)
{
    if (data & (1<<25))
    {
        memset(PM_Data, 0, sizeof(PM_Data));
    }

    PM_Data[(data >> 16) & 0xF] = data & 0xFFFF;

    if (!(data & (1<<24))) return;

    u32 cmd = (PM_Data[0] >> 8) - 0x60;
    printf("PM CMD %04X %04X\n", PM_Data[0], PM_Data[1]);
    if (false)
    {
        // newer SDK revision
        // TODO figure out condition for enabling this

        switch (cmd)
        {
        case 1: // utility
            {
                switch (PM_Data[1] & 0xFF)
                {
                case 1: // power LED: steady
                    SPI_Powerman::Registers[0] &= ~0x10;
                    break;
                case 2: // power LED: fast blink
                    SPI_Powerman::Registers[0] |= 0x30;
                    break;
                case 3: // power LED: slow blink
                    SPI_Powerman::Registers[0] &= ~0x20;
                    SPI_Powerman::Registers[0] |= 0x10;
                    break;
                case 4: // lower backlights on
                    SPI_Powerman::Registers[0] |= 0x04;
                    break;
                case 5: // lower backlights off
                    SPI_Powerman::Registers[0] &= ~0x04;
                    break;
                case 6: // upper backlights on
                    SPI_Powerman::Registers[0] |= 0x08;
                    break;
                case 7: // upper backlights off
                    SPI_Powerman::Registers[0] &= ~0x08;
                    break;
                case 8: // backlights on
                    SPI_Powerman::Registers[0] |= 0x0C;
                    break;
                case 9: // backlights off
                    SPI_Powerman::Registers[0] &= ~0x0C;
                    break;
                case 10: // sound amp on
                    SPI_Powerman::Registers[0] |= 0x01;
                    break;
                case 11: // sound amp off
                    SPI_Powerman::Registers[0] &= ~0x01;
                    break;
                case 12: // sound mute on
                    SPI_Powerman::Registers[0] |= 0x02;
                    break;
                case 13: // sound mute off
                    SPI_Powerman::Registers[0] &= ~0x02;
                    break;
                case 14: // shutdown
                    SPI_Powerman::Registers[0] &= ~0x01;
                    SPI_Powerman::Registers[0] |= 0x40;
                    NDS::Stop();
                    break;
                case 15: // read register 0 bits
                    printf("%04X %04X %04X %04X\n", PM_Data[0], PM_Data[1], PM_Data[2], PM_Data[3]);
                    break;
                }

                SendIPCReply(0x8, 0x0300E100);
            }
            break;

        default:
            printf("IPC: unknown powerman command %02X %04X\n", cmd, PM_Data[0]);
            break;
        }
    }
    else
    {
        switch (cmd)
        {
        case 3: // utility
            {
                switch (PM_Data[1] & 0xFF)
                {
                case 1: // power LED: steady
                    SPI_Powerman::Registers[0] &= ~0x10;
                    break;
                case 2: // power LED: fast blink
                    SPI_Powerman::Registers[0] |= 0x30;
                    break;
                case 3: // power LED: slow blink
                    SPI_Powerman::Registers[0] &= ~0x20;
                    SPI_Powerman::Registers[0] |= 0x10;
                    break;
                case 4: // lower backlights on
                    SPI_Powerman::Registers[0] |= 0x04;
                    break;
                case 5: // lower backlights off
                    SPI_Powerman::Registers[0] &= ~0x04;
                    break;
                case 6: // upper backlights on
                    SPI_Powerman::Registers[0] |= 0x08;
                    break;
                case 7: // upper backlights off
                    SPI_Powerman::Registers[0] &= ~0x08;
                    break;
                case 8: // backlights on
                    SPI_Powerman::Registers[0] |= 0x0C;
                    break;
                case 9: // backlights off
                    SPI_Powerman::Registers[0] &= ~0x0C;
                    break;
                case 10: // sound amp on
                    SPI_Powerman::Registers[0] |= 0x01;
                    break;
                case 11: // sound amp off
                    SPI_Powerman::Registers[0] &= ~0x01;
                    break;
                case 12: // sound mute on
                    SPI_Powerman::Registers[0] |= 0x02;
                    break;
                case 13: // sound mute off
                    SPI_Powerman::Registers[0] &= ~0x02;
                    break;
                case 14: // shutdown
                    SPI_Powerman::Registers[0] &= ~0x01;
                    SPI_Powerman::Registers[0] |= 0x40;
                    NDS::Stop();
                    break;
                case 15: // ????
                    SPI_Powerman::Registers[0] &= ~0x40;
                    break;
                }

                SendIPCReply(0x8, 0x0300E300);
            }
            break;

        case 4: // write register
            {
                u8 addr = PM_Data[0] & 0xFF;
                u8 val = PM_Data[1] & 0xFF;
                SPI_Powerman::Write(addr & 0x7F, true);
                SPI_Powerman::Write(val, false);
                SendIPCReply(0x8, 0x03008000 | (((PM_Data[1] + 0x70) & 0xFF) << 8));
            }
            break;

        case 5: // read register
            {
                u8 addr = PM_Data[0] & 0xFF;
                SPI_Powerman::Write((addr & 0x7F) | 0x80, true);
                SPI_Powerman::Write(0, false);
                u8 ret = SPI_Powerman::Read();
                SendIPCReply(0x8, 0x03008000 | ret | (((PM_Data[1] + 0x70) & 0xFF) << 8));
            }
            break;

        case 6:
            {
                // TODO

                SendIPCReply(0x8, 0x03008000 | (((PM_Data[1] + 0x70) & 0xFF) << 8));
            }
            break;

        default:
            printf("IPC: unknown powerman command %02X %04X\n", cmd, PM_Data[0]);
            break;
        }
    }
}

void OnIPCRequest_Mic(u32 data)
{
    if (data & (1<<25))
    {
        memset(Mic_Data, 0, sizeof(Mic_Data));
    }

    Mic_Data[(data >> 16) & 0xF] = data & 0xFFFF;

    if (!(data & (1<<24))) return;

    u32 cmd = (Mic_Data[0] >> 8) - 0x40;
    switch (cmd)
    {
    case 0: // sampling?
        {
            // TODO

            SendIPCReply(0x9, 0x0300C000);
        }
        break;

    default:
        printf("unknown mic request %08X\n", data);
        break;
    }
}

void OnIPCRequest_CartSave(u32 data)
{
    if (SM_DataPos == 0)
        SM_Command = data;

    switch (SM_Command)
    {
    case 0:
        if (SM_DataPos == 0) break;
        if (SM_DataPos == 1)
        {
            SM_Buffer = data;
            SendIPCReply(0xB, 0x1, 1);
            SM_DataPos = 0;
            return;
        }
        break;

    case 2: // identify savemem
        // TODO
        SendIPCReply(0xB, 0x1, 1);
        SM_DataPos = 0;
        return;

    case 6: // read
        {
            u32 offset = NDS::ARM7Read32(SM_Buffer+0x0C);
            u32 dst = NDS::ARM7Read32(SM_Buffer+0x10);
            u32 len = NDS::ARM7Read32(SM_Buffer+0x14);

            u8* mem = NDSCart::GetSaveMemory();
            u32 memlen = NDSCart::GetSaveMemoryLength();
            if (mem && memlen)
            {
                memlen--;

                for (u32 i = 0; i < len; i++)
                {
                    NDS::ARM7Write8(dst, mem[offset & memlen]);
                    dst++;
                    offset++;
                }
            }

            SendIPCReply(0xB, 0x1, 1);
            SM_DataPos = 0;
            return;
        }
        break;

    case 8: // write
        {
            u32 src = NDS::ARM7Read32(SM_Buffer+0x0C);
            u32 offset = NDS::ARM7Read32(SM_Buffer+0x10);
            u32 len = NDS::ARM7Read32(SM_Buffer+0x14);

            printf("IPC CMD8: %08X %08X %08X\n", src, offset, len);

            SendIPCReply(0xB, 0x1, 1);
            SM_DataPos = 0;
            return;
        }
        break;

    case 9: // verify
        {
            u32 src = NDS::ARM7Read32(SM_Buffer+0x0C);
            u32 offset = NDS::ARM7Read32(SM_Buffer+0x10);
            u32 len = NDS::ARM7Read32(SM_Buffer+0x14);

            printf("IPC CMD9: %08X %08X %08X\n", src, offset, len);

            // writes result to first word of IPC buffer

            SendIPCReply(0xB, 0x1, 1);
            SM_DataPos = 0;
            return;
        }
        break;

    default:
        printf("SAVEMEM: unknown cmd %08X\n", SM_Command);
        break;
    }

    SM_DataPos++;
}

void OnIPCRequest_Cart(u32 data)
{
    if ((data & 0x3F) == 1)
    {
        // TODO other shito?

        SendIPCReply(0xD, 0x1);
    }
    else
    {
        // do something else
    }
    /*if (data & 0x1)
    {
        // init

        SendIPCReply(0xD, 0x1);
    }*/
}


void OnIPCRequest()
{
    u32 val = NDS::IPCFIFO9.Read();

    if (NDS::IPCFIFO9.IsEmpty() && (NDS::IPCFIFOCnt9 & 0x0004))
        NDS::SetIRQ(0, NDS::IRQ_IPCSendDone);

    u32 service = val & 0x1F;
    u32 data = val >> 6;
    u32 flag = (val >> 5) & 0x1;
//printf("IPC %08X\n", val);
    switch (service)
    {
    case 0x4: // firmware
        if (flag) break;
        OnIPCRequest_Firmware(data);
        break;

    case 0x5: // RTC
        if (flag) break;
        OnIPCRequest_RTC(data);
        break;

    case 0x6: // touchscreen
        if (flag) break;
        OnIPCRequest_Touchscreen(data);
        break;

    case 0x7: // sound
        OnIPCRequest_Sound(data);
        break;

    case 0x8: // powerman
        if (flag) break;
        OnIPCRequest_Powerman(data);
        break;

    case 0x9: // mic
        if (flag) break;
        OnIPCRequest_Mic(data);
        break;

    case 0xA: // wifi
        if (flag) break;
        Wifi::OnIPCRequest(data);
        break;

    case 0xB: // cart savemem
        if (!flag) break;
        OnIPCRequest_CartSave(data);
        break;

    case 0xC:
        if (data == 0x1000)
        {
            // TODO: stop/reset hardware

            SendIPCReply(0xC, 0x1000);
        }
        break;

    case 0xD: // cart
        OnIPCRequest_Cart(data);
        break;

    case 0xF:
        if (data == 0x10000)
        {
            SendIPCReply(0xF, 0x10000);
        }
        break;

    default:
        printf("HLE: unknown IPC request %08X service=%02X data=%08X flag=%d\n", val, service, data, flag);
        break;
    }
}

}
}
