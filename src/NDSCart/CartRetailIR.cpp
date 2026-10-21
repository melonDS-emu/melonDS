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
#include "CartRetailIRLink.h"
#include "../NDS.h"
#include "../Utils.h"

// CartRetailIR: NDS cartridge with IR transceiver
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

    IRCmd = 0;
    IRSent = 0;
    IRReceived = 0;
    IRLink::Reset();
}

void CartRetailIR::DoSavestate(Savestate* file)
{
    CartRetail::DoSavestate(file);

    file->Var8(&IRCmd);
}

void CartRetailIR::LogPacket(const char* dir, u32 len)
{
    u64 now = Platform::GetMSCount();
    u64 ms = now - IRLogLast;
    IRLogLast = now;

    // the later version sends IR bytes XORed with 0xAA; show them decoded
    u8 key = (IRVersion == CartRetailIRVersionLate) ? 0xAA : 0x00;
    char text[3 * 16 + 1] = "";
    for (u32 i = 0; i < len && i < 16; i++)
        snprintf(text + i * 3, 4, " %02X", IRLogBytes[i] ^ key);

    Log(LogLevel::Debug, "IR: +%5llums %s %3u:%s%s\n", (unsigned long long)ms, dir, len,
        text, len > 16 ? " ..." : "");
}

void CartRetailIR::SPISelect()
{
    CartRetail::SPISelect();
    IRPos = 0;
    IRSent = 0;
    IRReceived = 0;
}

void CartRetailIR::SPIRelease()
{
    CartRetail::SPIRelease();

    if (IRPos == 0)
        return;

    switch (IRCmd)
    {
    case 0x01:
        // the game has read the packet, drop it
        if (IRReceived)
        {
            for (u32 i = 0; i < IRReceived && i < 16; i++)
                IRLogBytes[i] = IRLink::ReceivedByte(i);
            LogPacket("recv", IRReceived);
            IRLink::DropPacket();
        }
        break;

    case 0x02:
        if (IRSent)
        {
            LogPacket("send", IRSent);
            IRLink::EndPacket();
        }
        break;
    }
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

    case 0x01: // receive from IR
        if (IRPos == 1)
        {
            // the first byte is the length of the packet, then its contents
            IRReceived = IRLink::Receive();
            ret = IRReceived;
        }
        else if (IRPos - 2 < IRReceived)
            ret = IRLink::ReceivedByte(IRPos - 2);
        break;

    case 0x02: // send over IR
        IRLink::SendByte(val);
        if (IRSent < 16)
            IRLogBytes[IRSent] = val;
        IRSent++;
        break;

    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
        Log(LogLevel::Debug, "IRCmd %02X at %08X val %02X (%02X)\n", IRCmd, IRPos, val, val ^ 0xAA);
        break;

    /* Check IR status (only used by late version IR carts) */
    case 0x08:
        if (IRVersion == CartRetailIRVersionLate)
            ret = 0xAA;
        break;
    }

    IRPos++;
    return ret;
}


}

}
