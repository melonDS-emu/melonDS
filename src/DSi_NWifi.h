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

#ifndef DSI_NWIFI_H
#define DSI_NWIFI_H

#include "DSi_SD.h"
#include "FIFO.h"

class DSi_NWifi : public DSi_SDDevice
{
public:
    DSi_NWifi(DSi_SDHost* host);
    ~DSi_NWifi();

    void SendCMD(u8 cmd, u32 param);
    void SendACMD(u8 cmd, u32 param);

    void ContinueTransfer();

private:
    u32 TransferCmd;
    u32 TransferAddr;
    u32 RemSize;

    u8 F0_Read(u32 addr);
    void F0_Write(u32 addr, u8 val);

    u8 F1_Read(u32 addr);
    void F1_Write(u32 addr, u8 val);

    u8 SDIO_Read(u32 func, u32 addr);
    void SDIO_Write(u32 func, u32 addr, u8 val);

    void ReadBlock();
    void WriteBlock();

    void BMI_Command();

    void WindowRead();
    void WindowWrite();

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

    FIFO<u8>* Mailbox[8];

    u32 WindowData, WindowReadAddr, WindowWriteAddr;
};

#endif // DSI_NWIFI_H
