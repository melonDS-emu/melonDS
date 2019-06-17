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

#ifndef DSI_SD_H
#define DSI_SD_H

class DSi_SDDevice;


class DSi_SDHost
{
public:
    DSi_SDHost(u32 num);
    ~DSi_SDHost();

    void Reset();

    void DoSavestate(Savestate* file);

    void SendResponse(u32 val, bool last);

    u16 Read(u32 addr);
    void Write(u32 addr, u16 val);

private:
    u32 Num;

    u16 PortSelect;
    u16 SoftReset;
    u16 SDClock;

    u32 IRQStatus;  // IF
    u32 IRQMask;    // ~IE

    u16 Command;
    u32 Param;
    u16 ResponseBuffer[8];

    DSi_SDDevice* Ports[2];

    void SetIRQ(u32 irq);
};


class DSi_SDDevice
{
public:
    DSi_SDDevice(DSi_SDHost* host) { Host = host; }
    ~DSi_SDDevice() {}

    virtual void SendCMD(u8 cmd, u32 param) = 0;

protected:
    DSi_SDHost* Host;
};


class DSi_MMCStorage : public DSi_SDDevice
{
public:
    DSi_MMCStorage(DSi_SDHost* host, bool internal, const char* path);
    ~DSi_MMCStorage();

    void SetCID(u8* cid) { memcpy(CID, cid, 16); }

    void SendCMD(u8 cmd, u32 param);
    void SendACMD(u8 cmd, u32 param);

private:
    bool Internal;
    char FilePath[1024];
    FILE* File;

    u8 CID[16];
    u8 CSD[16];

    u32 CSR;
    u32 OCR;
    u32 RCA;

    void SetState(u32 state) { CSR &= ~(0xF << 9); CSR |= (state << 9); }
};

#endif // DSI_SD_H
