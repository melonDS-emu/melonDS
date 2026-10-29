/*
    Copyright 2016-2026 melonDS team

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

#include "CartRetailIR.h"
#include "../NDS.h"
#include "../Utils.h"

// CartRetailIR: NDS cartridge with IR transceiver (Pokémon games)
// the IR transceiver is connected to the SPI interface, with a passthrough command for SRAM access

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

namespace NDSCart
{

CartRetailIR::CartRetailIR(const u8* rom, u32 len, u32 chipid, u32 irversion, bool badDSiDump, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata) :
    CartRetailIR(CopyToUnique(rom, len), len, chipid, irversion, badDSiDump, romparams, std::move(sram), sramlen, userdata)
{
}

CartRetailIR::CartRetailIR(
    std::unique_ptr<u8[]>&& rom,
    u32 len,
    u32 chipid,
    u32 irversion,
    bool badDSiDump,
    ROMListEntry romparams,
    std::unique_ptr<u8[]>&& sram,
    u32 sramlen,
    void* userdata
) :
    CartRetail(std::move(rom), len, chipid, badDSiDump, romparams, std::move(sram), sramlen, userdata, CartType::RetailIR),
    IRVersion(irversion)
{
}

CartRetailIR::~CartRetailIR() = default;

void CartRetailIR::Reset()
{
    CartRetail::Reset();
    Platform::IRClose();
    IRCmd = 0;
    IRPos = 0;
}

void CartRetailIR::DoSavestate(Savestate* file)
{
    CartRetail::DoSavestate(file);

    file->Var8(&IRCmd);
}

void CartRetailIR::SPISelect()
{
    CartRetail::SPISelect();

    IRPos = 0;
    // Log(LogLevel::Info, "--- SPISelect: IRCmd [%d], IRPos %d\n", IRCmd, IRPos);
}

void CartRetailIR::SPIRelease()
{
    CartRetail::SPIRelease();

    if (IRCmd == 0x02 && IRPos > 1)
    {
        u8 sendLen = IRPos - 1; // IRPos includes the command byte
        SendIR(sendLen);
        memset(TxBuf, 0, sizeof(TxBuf));
    }
    // if (IRCmd == 0x01 && IRPos > 1)
    // {
    //     u8 dataLen = IRPos - 2; // subtract cmd byte and length byte
    //     Log(LogLevel::Info, "--- SPIRelease: IRCmd [1] Read %d bytes:", dataLen);
    //     for (int i = 0; i < dataLen; i++) Log(LogLevel::Info, " %02X", RxBuf[i]);
    //     Log(LogLevel::Info, "\n");
    // }
}

u8 CartRetailIR::SPITransmitReceive(u8 val)
{
    if (IRPos == 0)
    {
        IRCmd = val;
        IRPos++;
        return 0;
    }

    u8 ret = 0;
    switch (IRCmd)
    {
    case 0x00: // pass-through
        ret = CartRetail::SPITransmitReceive(val);
        break;

    case 0x01: // Read from IR
        if (IRPos == 1) ret = ReadIR();
        else ret = (u8)RxBuf[IRPos - 2]; // We start returning actual packet data to the game now
        break;

    case 0x02: // Write to IR
        TxBuf[IRPos - 1] = val; // Load spi data into Tx Buffer
        ret = 0x00; // Return value on success
        break;

    case 0x08: // ID
        ret = 0xAA;
        break;
    }

    IRPos++;
    return ret;
}

u8 CartRetailIR::ReadIR()
{
    memset(RxBuf, 0, sizeof(RxBuf));

    u32 readTimeoutUs = Platform::IRReadTimeOutUs(UserData);
    if (readTimeoutUs > 0)
    {
        u8 len = Platform::IRReceivePacket(RxBuf, sizeof(RxBuf), UserData);
        if (len <= 0) return 0;

        u8 pointer = len;
        u64 lastRxTime = Platform::GetUSCount();
        while ((Platform::GetUSCount() - lastRxTime) < readTimeoutUs)
        {
            len = Platform::IRReceivePacket(RxBuf + pointer, sizeof(RxBuf) - pointer, UserData);
            if (len > 0)
            {
                pointer += len;
                lastRxTime = Platform::GetUSCount();
            }
        }
        return pointer;
    }
    else return Platform::IRReceivePacket(RxBuf, sizeof(RxBuf), UserData);

}

u8 CartRetailIR::SendIR(u8 len)
{
    u32 sendDelayUs = Platform::IRSendDelayUs(UserData);
    if (sendDelayUs > 0) Platform::Sleep(sendDelayUs);
    // This packet needs to WAIT or else it will be piggybacked onto the latest packet (on the pokewalker's end)
    if ((u8)TxBuf[0] == 0x5E) Platform::Sleep(10000);
    return Platform::IRSendPacket(TxBuf, len, UserData);
}


}

}
