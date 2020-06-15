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

#include <string.h>
#include <stdio.h>
#include "DSi.h"
#include "DSi_NWifi.h"
#include "SPI.h"


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


DSi_NWifi* Ctx = nullptr;


DSi_NWifi::DSi_NWifi(DSi_SDHost* host) : DSi_SDDevice(host)
{
    // TODO: check the actual mailbox size (presumably 0x200)
    for (int i = 0; i < 8; i++)
        Mailbox[i] = new FIFO<u8>(0x200);

    // this seems to control whether the firmware upload is done
    EEPROMReady = 0;

    Ctx = this;
}

DSi_NWifi::~DSi_NWifi()
{
    for (int i = 0; i < 8; i++)
        delete Mailbox[i];

    NDS::CancelEvent(NDS::Event_DSi_NWifi);
    Ctx = nullptr;
}

void DSi_NWifi::Reset()
{
    TransferCmd = 0xFFFFFFFF;
    RemSize = 0;

    F0_IRQEnable = 0;
    F0_IRQStatus = 0;

    F1_IRQEnable = 0; F1_IRQEnable_CPU = 0; F1_IRQEnable_Error = 0; F1_IRQEnable_Counter = 0;
    F1_IRQStatus = 0; F1_IRQStatus_CPU = 0; F1_IRQStatus_Error = 0; F1_IRQStatus_Counter = 0;

    WindowData = 0;
    WindowReadAddr = 0;
    WindowWriteAddr = 0;

    // TODO: check the actual mailbox size (presumably 0x200)
    for (int i = 0; i < 8; i++)
        Mailbox[i]->Clear();

    u8* mac = SPI_Firmware::GetWifiMAC();
    printf("NWifi MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    memset(EEPROM, 0, 0x400);

    *(u32*)&EEPROM[0x000] = 0x300;
    *(u16*)&EEPROM[0x008] = 0x8348; // TODO: determine properly (country code)
    memcpy(&EEPROM[0x00A], mac, 6);
    *(u32*)&EEPROM[0x010] = 0x60000000;

    memset(&EEPROM[0x03C], 0xFF, 0x70);
    memset(&EEPROM[0x140], 0xFF, 0x8);

    u16 chk = 0xFFFF;
    for (int i = 0; i < 0x300; i+=2)
        chk ^= *(u16*)&EEPROM[i];

    *(u16*)&EEPROM[0x004] = chk;

    BootPhase = 0;

    ErrorMask = 0;
    ScanTimer = 0;

    NDS::CancelEvent(NDS::Event_DSi_NWifi);
}


// CHECKME
// can IRQ status bits be set when the corresponding IRQs are disabled in the enable register?
// otherwise, does disabling them clear the status register?

void DSi_NWifi::UpdateIRQ()
{
    F0_IRQStatus = 0;
    IRQ = false;

    if (F1_IRQStatus & F1_IRQEnable)
        F0_IRQStatus |= (1<<1);

    if (F0_IRQEnable & (1<<0))
    {
        if (F0_IRQStatus & F0_IRQEnable)
            IRQ = true;
    }

    Host->SetCardIRQ();
}

void DSi_NWifi::UpdateIRQ_F1()
{
    F1_IRQStatus = 0;

    if (!Mailbox[4]->IsEmpty())                      F1_IRQStatus |= (1<<0);
    if (!Mailbox[5]->IsEmpty())                      F1_IRQStatus |= (1<<1);
    if (!Mailbox[6]->IsEmpty())                      F1_IRQStatus |= (1<<2);
    if (!Mailbox[7]->IsEmpty())                      F1_IRQStatus |= (1<<3);
    if (F1_IRQStatus_Counter & F1_IRQEnable_Counter) F1_IRQStatus |= (1<<4);
    if (F1_IRQStatus_CPU & F1_IRQEnable_CPU)         F1_IRQStatus |= (1<<6);
    if (F1_IRQStatus_Error & F1_IRQEnable_Error)     F1_IRQStatus |= (1<<7);

    UpdateIRQ();
}

void DSi_NWifi::SetIRQ_F1_Counter(u32 n)
{
    F1_IRQStatus_Counter |= (1<<n);
    UpdateIRQ_F1();
}

void DSi_NWifi::ClearIRQ_F1_Counter(u32 n)
{
    F1_IRQStatus_Counter &= ~(1<<n);
    UpdateIRQ_F1();
}

void DSi_NWifi::SetIRQ_F1_CPU(u32 n)
{
    F1_IRQStatus_CPU |= (1<<n);
    UpdateIRQ_F1();
}


u8 DSi_NWifi::F0_Read(u32 addr)
{
    switch (addr)
    {
    case 0x00000: return 0x11;
    case 0x00001: return 0x00;

    case 0x00002: return 0x02; // writable??
    case 0x00003: return 0x02;

    case 0x00004: return F0_IRQEnable;
    case 0x00005: return F0_IRQStatus;

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
    switch (addr)
    {
    case 0x00004:
        F0_IRQEnable = val;
        UpdateIRQ();
        return;
    }

    printf("NWIFI: unknown func0 write %05X %02X\n", addr, val);
}


u8 DSi_NWifi::F1_Read(u32 addr)
{
    if (addr < 0x100)
    {
        u8 ret = Mailbox[4]->Read();
        UpdateIRQ_F1();
        return ret;
    }
    else if (addr < 0x200)
    {
        u8 ret = Mailbox[5]->Read();
        UpdateIRQ_F1();
        return ret;
    }
    else if (addr < 0x300)
    {
        u8 ret = Mailbox[6]->Read();
        UpdateIRQ_F1();
        return ret;
    }
    else if (addr < 0x400)
    {
        u8 ret = Mailbox[7]->Read();
        UpdateIRQ_F1();
        return ret;
    }
    else if (addr < 0x800)
    {
        switch (addr)
        {
        case 0x00400: return F1_IRQStatus;
        case 0x00401: return F1_IRQStatus_CPU;
        case 0x00402: return F1_IRQStatus_Error;
        case 0x00403: return F1_IRQStatus_Counter;

        case 0x00405:
            {
                u8 ret = 0;

                if (Mailbox[4]->Level() >= 4) ret |= (1<<0);
                if (Mailbox[5]->Level() >= 4) ret |= (1<<1);
                if (Mailbox[6]->Level() >= 4) ret |= (1<<2);
                if (Mailbox[7]->Level() >= 4) ret |= (1<<3);

                return ret;
            }

        case 0x00408: return Mailbox[4]->Peek(0);
        case 0x00409: return Mailbox[4]->Peek(1);
        case 0x0040A: return Mailbox[4]->Peek(2);
        case 0x0040B: return Mailbox[4]->Peek(3);

        case 0x00418: return F1_IRQEnable;
        case 0x00419: return F1_IRQEnable_CPU;
        case 0x0041A: return F1_IRQEnable_Error;
        case 0x0041B: return F1_IRQEnable_Counter;

        // GROSS FUCKING HACK
        case 0x00440: ClearIRQ_F1_Counter(0); return 0;
        case 0x00450: return 1; // HAX!!

        case 0x00474: return WindowData & 0xFF;
        case 0x00475: return (WindowData >> 8) & 0xFF;
        case 0x00476: return (WindowData >> 16) & 0xFF;
        case 0x00477: return WindowData >> 24;
        }
    }
    else if (addr < 0x1000)
    {
        u8 ret = Mailbox[4]->Read();
        UpdateIRQ_F1();
        return ret;
    }
    else if (addr < 0x1800)
    {
        u8 ret = Mailbox[5]->Read();
        UpdateIRQ_F1();
        return ret;
    }
    else if (addr < 0x2000)
    {
        u8 ret = Mailbox[6]->Read();
        UpdateIRQ_F1();
        return ret;
    }
    else if (addr < 0x2800)
    {
        u8 ret = Mailbox[7]->Read();
        UpdateIRQ_F1();
        return ret;
    }
    else
    {
        u8 ret = Mailbox[4]->Read();
        UpdateIRQ_F1();
        return ret;
    }

    printf("NWIFI: unknown func1 read %05X\n", addr);
    return 0;
}

void DSi_NWifi::F1_Write(u32 addr, u8 val)
{
    if (addr < 0x100)
    {
        if (Mailbox[0]->IsFull()) printf("!!! NWIFI: MBOX0 FULL\n");
        Mailbox[0]->Write(val);
        if (addr == 0xFF) HandleCommand();
        UpdateIRQ_F1();
        return;
    }
    else if (addr < 0x200)
    {
        if (Mailbox[1]->IsFull()) printf("!!! NWIFI: MBOX1 FULL\n");
        Mailbox[1]->Write(val);
        UpdateIRQ_F1();
        return;
    }
    else if (addr < 0x300)
    {
        if (Mailbox[2]->IsFull()) printf("!!! NWIFI: MBOX2 FULL\n");
        Mailbox[2]->Write(val);
        UpdateIRQ_F1();
        return;
    }
    else if (addr < 0x400)
    {
        if (Mailbox[3]->IsFull()) printf("!!! NWIFI: MBOX3 FULL\n");
        Mailbox[3]->Write(val);
        UpdateIRQ_F1();
        return;
    }
    else if (addr < 0x800)
    {
        switch (addr)
        {
        case 0x00418: F1_IRQEnable = val; UpdateIRQ_F1(); return;
        case 0x00419: F1_IRQEnable_CPU = val; UpdateIRQ_F1(); return;
        case 0x0041A: F1_IRQEnable_Error = val; UpdateIRQ_F1(); return;
        case 0x0041B: F1_IRQEnable_Counter = val; UpdateIRQ_F1(); return;

        // GROSS FUCKING HACK
        case 0x00440: ClearIRQ_F1_Counter(0); return;

        case 0x00474: WindowData = (WindowData & 0xFFFFFF00) | val; return;
        case 0x00475: WindowData = (WindowData & 0xFFFF00FF) | (val << 8); return;
        case 0x00476: WindowData = (WindowData & 0xFF00FFFF) | (val << 16); return;
        case 0x00477: WindowData = (WindowData & 0x00FFFFFF) | (val << 24); return;

        case 0x00478:
            WindowWriteAddr = (WindowWriteAddr & 0xFFFFFF00) | val;
            WindowWrite(WindowWriteAddr, WindowData);
            return;
        case 0x00479: WindowWriteAddr = (WindowWriteAddr & 0xFFFF00FF) | (val << 8); return;
        case 0x0047A: WindowWriteAddr = (WindowWriteAddr & 0xFF00FFFF) | (val << 16); return;
        case 0x0047B: WindowWriteAddr = (WindowWriteAddr & 0x00FFFFFF) | (val << 24); return;

        case 0x0047C:
            WindowReadAddr = (WindowReadAddr & 0xFFFFFF00) | val;
            WindowData = WindowRead(WindowReadAddr);
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
        if (addr == 0xFFF) HandleCommand();
        UpdateIRQ_F1();
        return;
    }
    else if (addr < 0x1800)
    {
        if (Mailbox[1]->IsFull()) printf("!!! NWIFI: MBOX1 FULL\n");
        Mailbox[1]->Write(val);
        UpdateIRQ_F1();
        return;
    }
    else if (addr < 0x2000)
    {
        if (Mailbox[2]->IsFull()) printf("!!! NWIFI: MBOX2 FULL\n");
        Mailbox[2]->Write(val);
        UpdateIRQ_F1();
        return;
    }
    else if (addr < 0x2800)
    {
        if (Mailbox[3]->IsFull()) printf("!!! NWIFI: MBOX3 FULL\n");
        Mailbox[3]->Write(val);
        UpdateIRQ_F1();
        return;
    }
    else
    {
        if (Mailbox[0]->IsFull()) printf("!!! NWIFI: MBOX0 FULL\n");
        Mailbox[0]->Write(val);
        if (addr == 0x3FFF) HandleCommand(); // CHECKME
        UpdateIRQ_F1();
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
{
    switch (cmd)
    {
    case 12:
        // stop command
        // CHECKME: does the SDIO controller actually send those??
        // DSi firmware sets it to send them
        return;

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

    len = Host->GetTransferrableLen(len);

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
    len = Host->DataRX(data, len);

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

    len = Host->GetTransferrableLen(len);

    u8 data[0x200];
    if (len = Host->DataTX(data, len))
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


void DSi_NWifi::HandleCommand()
{
    switch (BootPhase)
    {
    case 0: return BMI_Command();
    case 1: return WMI_Command();
    }
}

void DSi_NWifi::BMI_Command()
{
    // HLE command handling stub
    u32 cmd = MB_Read32(0);
    printf("BMI: cmd %08X\n", cmd);

    switch (cmd)
    {
    case 0x01: // BMI_DONE
        {
            printf("BMI_DONE\n");
            EEPROMReady = 1; // GROSS FUCKING HACK
            u8 ready_msg[6] = {0x0A, 0x00, 0x08, 0x06, 0x16, 0x00};
            SendWMIEvent(0, 0x0001, ready_msg, 6);
            BootPhase = 1;
        }
        return;

    case 0x03: // BMI_WRITE_MEMORY
        {
            u32 addr = MB_Read32(0);
            u32 len = MB_Read32(0);
            printf("BMI mem write %08X %08X\n", addr, len);

            for (int i = 0; i < len; i++)
            {
                u8 val = Mailbox[0]->Read();

                // TODO: do something with it!!
            }
        }
        return;

    case 0x04: // BMI_EXECUTE
        {
            u32 entry = MB_Read32(0);
            u32 arg = MB_Read32(0);

            printf("BMI_EXECUTE %08X %08X\n", entry, arg);
        }
        return;

    case 0x06: // BMI_READ_SOC_REGISTER
        {
            u32 addr = MB_Read32(0);
            u32 val = WindowRead(addr);
            MB_Write32(4, val);
        }
        return;

    case 0x07: // BMI_WRITE_SOC_REGISTER
        {
            u32 addr = MB_Read32(0);
            u32 val = MB_Read32(0);
            WindowWrite(addr, val);
        }
        return;

    case 0x08: // BMI_GET_TARGET_ID
        MB_Write32(4, 0xFFFFFFFF);
        MB_Write32(4, 0x0000000C);
        //MB_Write32(4, 0x20000118);
        MB_Write32(4, 0x23000024); // ROM version (TODO: how to determine correct one?)
        MB_Write32(4, 0x00000002);
        return;

    case 0x0D: // BMI_LZ_STREAM_START
        {
            u32 addr = MB_Read32(0);
            printf("BMI_LZ_STREAM_START %08X\n", addr);
        }
        return;

    case 0x0E: // BMI_LZ_DATA
        {
            u32 len = MB_Read32(0);
            printf("BMI LZ write %08X\n", len);
            //FILE* f = fopen("debug/wififirm.bin", "ab");

            for (int i = 0; i < len; i++)
            {
                u8 val = Mailbox[0]->Read();

                // TODO: do something with it!!
                //fwrite(&val, 1, 1, f);
            }
            //fclose(f);
        }
        return;

    default:
        printf("unknown BMI command %08X\n", cmd);
        return;
    }
}

void DSi_NWifi::WMI_Command()
{
    // HLE command handling stub
    u16 h0 = MB_Read16(0);
    u16 len = MB_Read16(0);
    u16 h2 = MB_Read16(0);

    u16 cmd = MB_Read16(0);

    switch (cmd)
    {
    case 0x0002: // service connect
        {
            u16 svc_id = MB_Read16(0);
            u16 conn_flags = MB_Read16(0);
            printf("service connect %04X %04X %04X\n", svc_id, conn_flags, MB_Read16(0));

            u8 svc_resp[8];
            // responses from hardware:
            // 0003 0100 00 01 0602 00 00
            // 0003 0101 00 02 0600 00 00
            // 0003 0102 00 03 0600 00 00
            // 0003 0103 00 04 0600 00 00
            // 0003 0104 00 05 0600 00 00
            *(u16*)&svc_resp[0] = svc_id;
            svc_resp[2] = 0;
            svc_resp[3] = (svc_id & 0xFF) + 1;
            *(u16*)&svc_resp[4] = (svc_id==0x0100) ? 0x0602 : 0x0600; // max message size
            *(u16*)&svc_resp[6] = 0x0000;
            SendWMIEvent(0, 0x0003, svc_resp, 8);
        }
        break;

    case 0x0004: // setup complete
        {
            u8 ready_evt[12];
            memcpy(&ready_evt[0], SPI_Firmware::GetWifiMAC(), 6);
            ready_evt[6] = 0x02;
            ready_evt[7] = 0;
            *(u32*)&ready_evt[8] = 0x2300006C;
            SendWMIEvent(1, 0x1001, ready_evt, 12);

            u8 regdomain_evt[4];
            *(u32*)&regdomain_evt[0] = 0x80000000 | (*(u16*)&EEPROM[0x008] & 0x0FFF);
            SendWMIEvent(1, 0x1006, regdomain_evt, 4);

            NDS::ScheduleEvent(NDS::Event_DSi_NWifi, true, 33611, MSTimer, 0);
        }
        break;

    case 0x0007: // start scan
        {
            u32 forcefg = MB_Read32(0);
            u32 legacy = MB_Read32(0);
            u32 scantime = MB_Read32(0);
            u32 forceinterval = MB_Read32(0);
            u8 scantype = Mailbox[0]->Read();
            u8 nchannels = Mailbox[0]->Read();

            printf("WMI: start scan, forceFG=%d, legacy=%d, scanTime=%d, interval=%d, scanType=%d, chan=%d\n",
                   forcefg, legacy, scantime, forceinterval, scantype, nchannels);

            if (ScanTimer > 0)
            {
                printf("!! CHECKME: START SCAN BUT WAS ALREADY SCANNING (%d)\n", ScanTimer);
            }

            ScanTimer = scantime;

            SendWMIAck();
        }
        break;

    case 0x0008: // set scan params
        {
            // TODO: do something with the params!!

            SendWMIAck();
        }
        break;

    case 0x0009: // set BSS filter
        {
            // TODO: do something with the params!!
            u8 bssfilter = Mailbox[0]->Read();
            Mailbox[0]->Read();
            Mailbox[0]->Read();
            Mailbox[0]->Read();
            u32 iemask = MB_Read32(0);

            printf("WMI: set BSS filter, filter=%02X, iemask=%08X\n", bssfilter, iemask);

            SendWMIAck();
        }
        break;

    case 0x000A: // set probed BSSID
        {
            u8 id = Mailbox[0]->Read();
            u8 flags = Mailbox[0]->Read();
            u8 len = Mailbox[0]->Read();

            char ssid[33];
            for (int i = 0; i < len && i < 32; i++)
                ssid[i] = Mailbox[0]->Read();
            ssid[32]= '\0';

            // TODO: store it somewhere
            printf("WMI: set probed SSID: id=%d, flags=%02X, len=%d, SSID=%s\n", id, flags, len, ssid);

            SendWMIAck();
        }
        break;

    case 0x000E: // get channel list
        {
            int nchan = 11; // TODO: customize??
            u8 reply[2 + (nchan*2) + 2];

            reply[0] = 0;
            reply[1] = nchan;
            for (int i = 0; i < nchan; i++)
                *(u16*)&reply[2 + (i*2)] = 2412 + (i*5);
            *(u16*)&reply[2 + (nchan*2)] = 0;

            SendWMIEvent(1, 0x000E, reply, 4+(nchan*2));

            SendWMIAck();
        }
        break;

    case 0x0011: // set channel params
        {
            Mailbox[0]->Read();
            u8 scan = Mailbox[0]->Read();
            u8 phymode = Mailbox[0]->Read();
            u8 len = Mailbox[0]->Read();

            u16 channels[32];
            for (int i = 0; i < len && i < 32; i++)
                channels[i] = MB_Read16(0);

            // TODO: store it somewhere
            printf("WMI: set channel params: scan=%d, phymode=%d, len=%d, channels=", scan, phymode, len);
            for (int i = 0; i < len && i < 32; i++)
                printf("%d,", channels[i]);
            printf("\n");

            SendWMIAck();
        }
        break;

    case 0x0022: // set error bitmask
        {
            ErrorMask = MB_Read32(0);

            SendWMIAck();
        }
        break;

    case 0x002E: // extension shit
        {
            u16 extcmd = MB_Read16(0);
            switch (extcmd)
            {
            case 0x2008: // 'heartbeat'??
                {
                    u32 cookie = MB_Read32(0);
                    u32 source = MB_Read32(0);

                    u8 reply[10];
                    *(u16*)&reply[0] = 0x3007;
                    *(u32*)&reply[2] = cookie;
                    *(u32*)&reply[6] = source;
                    SendWMIEvent(1, 0x1010, reply, 10);
                }
                break;

            default:
                printf("WMI: unknown ext cmd 002E:%04X\n", extcmd);
                break;
            }

            SendWMIAck();
        }
        break;

    case 0x0047: // cmd47 -- timer shenanigans??
        {
            //

            SendWMIAck();
        }
        break;

    default:
        printf("unknown WMI command %04X\n", cmd);
        for (int i = 0; i < len; i++)
        {
            printf("%02X ", Mailbox[0]->Read());
            if ((i&0xF)==0xF) printf("\n");
        }
        printf("\n");
        break;
    }

    MB_Drain(0);
}

//void DSi_NWifi::SendWMIFrame(u8* data, u32 len, u8 ep, u8 flags, u16 ctrl)
void DSi_NWifi::SendWMIEvent(u8 ep, u16 id, u8* data, u32 len)
{
    u32 wlen = 0;

    Mailbox[4]->Write(ep);    // eid
    Mailbox[4]->Write(0x02);  // flags (trailer)
    MB_Write16(4, len+2+8);   // data length (plus event ID and trailer)
    Mailbox[4]->Write(8);     // trailer length
    Mailbox[4]->Write(0);     //
    MB_Write16(4, id);        // event ID
    wlen += 8;

    for (int i = 0; i < len; i++)
    {
        Mailbox[4]->Write(data[i]);
        wlen++;
    }

    // trailer
    Mailbox[4]->Write(0x02);
    Mailbox[4]->Write(0x06);
    Mailbox[4]->Write(0x00);
    Mailbox[4]->Write(0x00);
    Mailbox[4]->Write(0x00);
    Mailbox[4]->Write(0x00);
    Mailbox[4]->Write(0x00);
    Mailbox[4]->Write(0x00);
    wlen += 8;

    for (; wlen & 0x7F; wlen++)
        Mailbox[4]->Write(0);
}

void DSi_NWifi::SendWMIAck()
{
    u32 wlen = 0;

    Mailbox[4]->Write(0);     // eid
    Mailbox[4]->Write(0x02);  // flags (trailer)
    MB_Write16(4, 0xC);       // data length (plus event ID and trailer)
    Mailbox[4]->Write(0xC);   // trailer length
    Mailbox[4]->Write(0);     //
    wlen += 6;

    // trailer
    Mailbox[4]->Write(0x01);
    Mailbox[4]->Write(0x02);
    Mailbox[4]->Write(0x01);
    Mailbox[4]->Write(0x01);
    Mailbox[4]->Write(0x02);
    Mailbox[4]->Write(0x06);

    // observed trailer data:
    // 00 06 00 00 02 00 (in the first few responses)
    // 00 00 00 00 00 00
    // TODO: work out what that all means?
    // does software even care?
    Mailbox[4]->Write(0x00);
    Mailbox[4]->Write(0x06);
    Mailbox[4]->Write(0x00);
    Mailbox[4]->Write(0x00);
    Mailbox[4]->Write(0x02);
    Mailbox[4]->Write(0x00);
    wlen += 0xC;

    for (; wlen & 0x7F; wlen++)
        Mailbox[4]->Write(0);
}


u32 DSi_NWifi::WindowRead(u32 addr)
{
    printf("NWifi: window read %08X\n", addr);

    if ((addr & 0xFFFF00) == 0x520000)
    {
        // RAM host interest area
        // TODO: different base based on hardware version

        switch (addr & 0xFF)
        {
        case 0x54:
            // base address of EEPROM data
            // TODO find what the actual address is!
            return 0x1FFC00;
        case 0x58: return EEPROMReady;
        }

        return 0;
    }

    // hax
    if ((addr & 0x1FFC00) == 0x1FFC00)
    {
        return *(u32*)&EEPROM[addr & 0x3FF];
    }

    switch (addr)
    {
    case 0x40EC: // chip ID
        // 0D000000 / 0D000001 == AR6013
        // TODO: check firmware.bin to determine the correct value
        return 0x0D000001;

    // SOC_RESET_CAUSE
    case 0x40C0: return 2;
    }

    return 0;
}

void DSi_NWifi::WindowWrite(u32 addr, u32 val)
{
    printf("NWifi: window write %08X %08X\n", addr, val);
}


void DSi_NWifi::_MSTimer()
{
    if (ScanTimer > 0)
    {
        ScanTimer--;

        if (ScanTimer == 0)
        {
            u32 status = 0;
            SendWMIEvent(1, 0x100A, (u8*)&status, 4);
        }
    }

    /*for (;;)
    {
        int rxlen = WifiAP::RecvPacket(RXBuffer);
        if (rxlen == 0) return;
        if (rxlen < 12+24) continue;

        framelen = *(u16*)&RXBuffer[10];
        if (framelen != rxlen-12)
        {
            printf("bad frame length\n");
            continue;
        }
        framelen -= 4;

        framectl = *(u16*)&RXBuffer[12+0];
        if ((framectl & 0x00FC) == 0x0080) // beacon
        {
            printf("NWifi: got beacon\n");

            u32 bodylen = framelen - 24;
            if (bodylen > 256-16) bodylen = 256-16;

            u8 beacon_evt[256];
            memset(beacon_evt, 0, 256);
            *(u16*)&beacon_evt[0] = 0x1004; // WMI_BSSINFO_EVENT
            *(u16*)&beacon_evt[2] = 2442; // channel (in MHz???? welp)
            beacon_evt[2] = 0x01; // frame type
            beacon_evt[3] = 0x33; // 'snr' (???????)
            *(u16*)&beacon_evt[4] = 0xFFD4; // 'rssi', 'snr'-95 (?????)
            memcpy(&beacon_evt[6], &RXBuffer[12+16], 6); // BSSID
            *(u32*)&beacon_evt[12] = 0; // ieMask (???????)
            memcpy(&beacon_evt[16], &RXBuffer[12+24], bodylen); // frame body
            //SendWMIFrame(beacon_evt, 16+bodylen, 1, 0, 0);
        }
    }*/
}

void DSi_NWifi::MSTimer(u32 param)
{
    Ctx->_MSTimer();
    NDS::ScheduleEvent(NDS::Event_DSi_NWifi, true, 33611, MSTimer, 0);
}
