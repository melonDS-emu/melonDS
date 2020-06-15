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

#ifndef DSI_NWIFI_H
#define DSI_NWIFI_H

#include "DSi_SD.h"
#include "FIFO.h"

class DSi_NWifi : public DSi_SDDevice
{
public:
    DSi_NWifi(DSi_SDHost* host);
    ~DSi_NWifi();

    void Reset();

    void SendCMD(u8 cmd, u32 param);
    void SendACMD(u8 cmd, u32 param);

    void ContinueTransfer();

    void SetIRQ_F1_Counter(u32 n);

    void CheckRX();

    static void MSTimer(u32 param);

private:
    u32 TransferCmd;
    u32 TransferAddr;
    u32 RemSize;

    void UpdateIRQ();
    void UpdateIRQ_F1();
    //void SetIRQ_F1_Counter(u32 n);
    void ClearIRQ_F1_Counter(u32 n);
    void SetIRQ_F1_CPU(u32 n);

    u8 F0_Read(u32 addr);
    void F0_Write(u32 addr, u8 val);

    u8 F1_Read(u32 addr);
    void F1_Write(u32 addr, u8 val);

    u8 SDIO_Read(u32 func, u32 addr);
    void SDIO_Write(u32 func, u32 addr, u8 val);

    void ReadBlock();
    void WriteBlock();

    void HandleCommand();
    void BMI_Command();
    void WMI_Command();

    void SendWMIEvent(u8 ep, u16 id, u8* data, u32 len);
    void SendWMIAck();

    u32 WindowRead(u32 addr);
    void WindowWrite(u32 addr, u32 val);

    u16 MB_Read16(int n)
    {
        u16 ret = Mailbox[n]->Read();
        ret |= (Mailbox[n]->Read() << 8);
        return ret;
    }

    void MB_Write16(int n, u16 val)
    {
        Mailbox[n]->Write(val & 0xFF); val >>= 8;
        Mailbox[n]->Write(val & 0xFF);
    }

    u32 MB_Read32(int n)
    {
        u32 ret = Mailbox[n]->Read();
        ret |= (Mailbox[n]->Read() << 8);
        ret |= (Mailbox[n]->Read() << 16);
        ret |= (Mailbox[n]->Read() << 24);
        return ret;
    }

    void MB_Write32(int n, u32 val)
    {
        Mailbox[n]->Write(val & 0xFF); val >>= 8;
        Mailbox[n]->Write(val & 0xFF); val >>= 8;
        Mailbox[n]->Write(val & 0xFF); val >>= 8;
        Mailbox[n]->Write(val & 0xFF);
    }

    void MB_Drain(int n)
    {
        while (!Mailbox[n]->IsEmpty()) Mailbox[n]->Read();
    }

    FIFO<u8>* Mailbox[8];

    u8 F0_IRQEnable;
    u8 F0_IRQStatus;

    u8 F1_IRQEnable, F1_IRQEnable_CPU, F1_IRQEnable_Error, F1_IRQEnable_Counter;
    u8 F1_IRQStatus, F1_IRQStatus_CPU, F1_IRQStatus_Error, F1_IRQStatus_Counter;

    u32 WindowData, WindowReadAddr, WindowWriteAddr;

    u8 EEPROM[0x400];
    u32 EEPROMReady;

    u32 BootPhase;

    u32 ErrorMask;

    u8 RXBuffer[2048];
};

#endif // DSI_NWIFI_H
