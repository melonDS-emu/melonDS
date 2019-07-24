/*
    Copyright 2016-2019 Arisotura

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
#include <stdio.h>
#include "DSi.h"
#include "DSi_NWifi.h"


const u8 CIS0[256] =
{
    0x01, 0x03, 0xD9, 0x01, 0xFF,
    0x20, 0x04, 0x71, 0x02, 0x00, 0x02,
    0x21, 0x02, 0x0C, 0x00,
    0x22, 0x04, 0x00, 0x00, 0x08, 0x32,
    0x1A, 0x05, 0x01, 0x01, 0x00, 0x02, 0x07,
    0x1B, 0x08, 0xC1, 0x41, 0x30, 0x30, 0xFF, 0xFF, 0x32, 0x00,
    0x14, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00
};

const u8 CIS1[256] =
{
    0x20, 0x04, 0x71, 0x02, 0x00, 0x02,
    0x21, 0x02, 0x0C, 0x00,
    0x22, 0x2A, 0x01,
    0x01, 0x11,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08,
    0x00, 0x00, 0xFF, 0x80,
    0x00, 0x00, 0x00,
    0x00, 0x01, 0x0A,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x01,
    0x80, 0x01, 0x06,
    0x81, 0x01, 0x07,
    0x82, 0x01, 0xDF,
    0xFF,
    0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


DSi_NWifi::DSi_NWifi(DSi_SDHost* host) : DSi_SDDevice(host)
{
    TransferCmd = 0xFFFFFFFF;
    RemSize = 0;

    WindowData = 0;
    WindowReadAddr = 0;
    WindowWriteAddr = 0;

    // TODO: check the actual mailbox size (presumably 0x200)
    for (int i = 0; i < 8; i++)
        Mailbox[i] = new FIFO<u8>(0x200);
}

DSi_NWifi::~DSi_NWifi()
{
    for (int i = 0; i < 8; i++)
        delete Mailbox[i];
}


u8 DSi_NWifi::F0_Read(u32 addr)
{
    switch (addr)
    {
    case 0x00000: return 0x11;
    case 0x00001: return 0x00;

    case 0x00002: return 0x02; // writable??
    case 0x00003: return 0x02;

    case 0x00008: return 0x17;

    case 0x00009: return 0x00;
    case 0x0000A: return 0x10;
    case 0x0000B: return 0x00;

    case 0x00012: return 0x03;

    case 0x00109: return 0x00;
    case 0x0010A: return 0x11;
    case 0x0010B: return 0x00;
    }

    if (addr >= 0x01000 && addr < 0x01100)
    {
        return CIS0[addr & 0xFF];
    }
    if (addr >= 0x01100 && addr < 0x01200)
    {
        return CIS1[addr & 0xFF];
    }

    printf("NWIFI: unknown func0 read %05X\n", addr);
    return 0;
}

void DSi_NWifi::F0_Write(u32 addr, u8 val)
{
    printf("NWIFI: unknown func0 write %05X %02X\n", addr, val);
}


u8 DSi_NWifi::F1_Read(u32 addr)
{printf("F1 READ %05X\n", addr);
    if (addr < 0x100)
    {
        return Mailbox[4]->Read();
    }
    else if (addr < 0x200)
    {
        return Mailbox[5]->Read();
    }
    else if (addr < 0x300)
    {
        return Mailbox[6]->Read();
    }
    else if (addr < 0x400)
    {
        return Mailbox[7]->Read();
    }
    else if (addr < 0x800)
    {
        switch (addr)
        {
        case 0x00450: return 1; // HAX!!

        case 0x00474: return WindowData & 0xFF;
        case 0x00475: return (WindowData >> 8) & 0xFF;
        case 0x00476: return (WindowData >> 16) & 0xFF;
        case 0x00477: return WindowData >> 24;
        }
    }
    else if (addr < 0x1000)
    {
        return Mailbox[4]->Read();
    }
    else if (addr < 0x1800)
    {
        return Mailbox[5]->Read();
    }
    else if (addr < 0x2000)
    {
        return Mailbox[6]->Read();
    }
    else if (addr < 0x2800)
    {
        return Mailbox[7]->Read();
    }
    else
    {
        return Mailbox[4]->Read();
    }

    printf("NWIFI: unknown func1 read %05X\n", addr);
    return 0;
}

void DSi_NWifi::F1_Write(u32 addr, u8 val)
{printf("F1 WRITE %05X %02X\n", addr, val);
    if (addr < 0x100)
    {
        if (Mailbox[0]->IsFull()) printf("!!! NWIFI: MBOX0 FULL\n");
        Mailbox[0]->Write(val);
        if (addr == 0xFF) BMI_Command();
        return;
    }
    else if (addr < 0x200)
    {
        if (Mailbox[1]->IsFull()) printf("!!! NWIFI: MBOX1 FULL\n");
        Mailbox[1]->Write(val);
        return;
    }
    else if (addr < 0x300)
    {
        if (Mailbox[2]->IsFull()) printf("!!! NWIFI: MBOX2 FULL\n");
        Mailbox[2]->Write(val);
        return;
    }
    else if (addr < 0x400)
    {
        if (Mailbox[3]->IsFull()) printf("!!! NWIFI: MBOX3 FULL\n");
        Mailbox[3]->Write(val);
        return;
    }
    else if (addr < 0x800)
    {
        switch (addr)
        {
        case 0x00474: WindowData = (WindowData & 0xFFFFFF00) | val; return;
        case 0x00475: WindowData = (WindowData & 0xFFFF00FF) | (val << 8); return;
        case 0x00476: WindowData = (WindowData & 0xFF00FFFF) | (val << 16); return;
        case 0x00477: WindowData = (WindowData & 0x00FFFFFF) | (val << 24); return;

        case 0x00478:
            WindowWriteAddr = (WindowWriteAddr & 0xFFFFFF00) | val;
            WindowWrite();
            return;
        case 0x00479: WindowWriteAddr = (WindowWriteAddr & 0xFFFF00FF) | (val << 8); return;
        case 0x0047A: WindowWriteAddr = (WindowWriteAddr & 0xFF00FFFF) | (val << 16); return;
        case 0x0047B: WindowWriteAddr = (WindowWriteAddr & 0x00FFFFFF) | (val << 24); return;

        case 0x0047C:
            WindowReadAddr = (WindowReadAddr & 0xFFFFFF00) | val;
            WindowRead();
            return;
        case 0x0047D: WindowReadAddr = (WindowReadAddr & 0xFFFF00FF) | (val << 8); return;
        case 0x0047E: WindowReadAddr = (WindowReadAddr & 0xFF00FFFF) | (val << 16); return;
        case 0x0047F: WindowReadAddr = (WindowReadAddr & 0x00FFFFFF) | (val << 24); return;
        }
    }
    else if (addr < 0x1000)
    {
        if (Mailbox[0]->IsFull()) printf("!!! NWIFI: MBOX0 FULL\n");
        Mailbox[0]->Write(val);
        if (addr == 0xFFF) BMI_Command();
        return;
    }
    else if (addr < 0x1800)
    {
        if (Mailbox[1]->IsFull()) printf("!!! NWIFI: MBOX1 FULL\n");
        Mailbox[1]->Write(val);
        return;
    }
    else if (addr < 0x2000)
    {
        if (Mailbox[2]->IsFull()) printf("!!! NWIFI: MBOX2 FULL\n");
        Mailbox[2]->Write(val);
        return;
    }
    else if (addr < 0x2800)
    {
        if (Mailbox[3]->IsFull()) printf("!!! NWIFI: MBOX3 FULL\n");
        Mailbox[3]->Write(val);
        return;
    }
    else
    {
        if (Mailbox[0]->IsFull()) printf("!!! NWIFI: MBOX0 FULL\n");
        Mailbox[0]->Write(val);
        if (addr == 0x3FFF) BMI_Command(); // CHECKME
        return;
    }

    printf("NWIFI: unknown func1 write %05X %02X\n", addr, val);
}


u8 DSi_NWifi::SDIO_Read(u32 func, u32 addr)
{
    switch (func)
    {
    case 0: return F0_Read(addr);
    case 1: return F1_Read(addr);
    }

    printf("NWIFI: unknown SDIO read %d %05X\n", func, addr);
    return 0;
}

void DSi_NWifi::SDIO_Write(u32 func, u32 addr, u8 val)
{
    switch (func)
    {
    case 0: return F0_Write(addr, val);
    case 1: return F1_Write(addr, val);
    }

    printf("NWIFI: unknown SDIO write %d %05X %02X\n", func, addr, val);
}


void DSi_NWifi::SendCMD(u8 cmd, u32 param)
{printf("NWIFI CMD %d %08X %08X\n", cmd, param, NDS::GetPC(1));
    switch (cmd)
    {
    case 52: // IO_RW_DIRECT
        {
            u32 func = (param >> 28) & 0x7;
            u32 addr = (param >> 9) & 0x1FFFF;

            if (param & (1<<31))
            {
                // write

                u8 val = param & 0xFF;
                SDIO_Write(func, addr, val);
                if (param & (1<<27))
                    val = SDIO_Read(func, addr); // checkme
                Host->SendResponse(val | 0x1000, true);
            }
            else
            {
                // read

                u8 val = SDIO_Read(func, addr);
                Host->SendResponse(val | 0x1000, true);
            }
        }
        return;

    case 53: // IO_RW_EXTENDED
        {
            u32 addr = (param >> 9) & 0x1FFFF;

            TransferCmd = param;
            TransferAddr = addr;
            if (param & (1<<27))
            {
                RemSize = (param & 0x1FF) << 9; // checkme
            }
            else
            {
                RemSize = (param & 0x1FF);
                if (!RemSize) RemSize = 0x200;
            }

            if (param & (1<<31))
            {
                // write

                WriteBlock();
                Host->SendResponse(0x1000, true);
            }
            else
            {
                // read

                ReadBlock();
                Host->SendResponse(0x1000, true);
            }
        }
        return;
    }

    printf("NWIFI: unknown CMD %d %08X\n", cmd, param);
}

void DSi_NWifi::SendACMD(u8 cmd, u32 param)
{
    printf("NWIFI: unknown ACMD %d %08X\n", cmd, param);
}

void DSi_NWifi::ContinueTransfer()
{
    if (TransferCmd & (1<<31))
        WriteBlock();
    else
        ReadBlock();
}

void DSi_NWifi::ReadBlock()
{
    u32 func = (TransferCmd >> 28) & 0x7;
    u32 len = (TransferCmd & (1<<27)) ? 0x200 : RemSize;

    u8 data[0x200];

    for (u32 i = 0; i < len; i++)
    {
        data[i] = SDIO_Read(func, TransferAddr);
        if (TransferCmd & (1<<26))
        {
            TransferAddr++;
            TransferAddr &= 0x1FFFF; // checkme
        }
    }
    Host->SendData(data, len);

    if (RemSize > 0)
    {
        RemSize -= len;
        if (RemSize == 0)
        {
            // TODO?
        }
    }
}

void DSi_NWifi::WriteBlock()
{
    u32 func = (TransferCmd >> 28) & 0x7;
    u32 len = (TransferCmd & (1<<27)) ? 0x200 : RemSize;

    u8 data[0x200];
    if (Host->ReceiveData(data, len))
    {
        for (u32 i = 0; i < len; i++)
        {
            SDIO_Write(func, TransferAddr, data[i]);
            if (TransferCmd & (1<<26))
            {
                TransferAddr++;
                TransferAddr &= 0x1FFFF; // checkme
            }
        }

        if (RemSize > 0)
        {
            RemSize -= len;
            if (RemSize == 0)
            {
                // TODO?
            }
        }
    }
}


void DSi_NWifi::BMI_Command()
{
    // HLE command handling stub
    u32 cmd = MB_Read32(0);
    printf("BMI: cmd %08X\n", cmd);

    switch (cmd)
    {
    case 0x08: // BMI_GET_TARGET_ID
        MB_Write32(4, 0xFFFFFFFF);
        MB_Write32(4, 0x0000000C);
        MB_Write32(4, 0x20000118);
        MB_Write32(4, 0x00000002);
        return;
    }
}


void DSi_NWifi::WindowRead()
{
    printf("NWifi: window read %08X\n", WindowReadAddr);

    switch (WindowReadAddr)
    {
    case 0x40EC: WindowData = 0x02000001; return;

    // SOC_RESET_CAUSE
    case 0x40C0: WindowData = 2; return;
    }
}

void DSi_NWifi::WindowWrite()
{
    printf("NWifi: window write %08X %08X\n", WindowWriteAddr, WindowData);
}
