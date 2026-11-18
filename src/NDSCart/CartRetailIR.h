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

#ifndef NDSCART_CARTRETAILIR_H
#define NDSCART_CARTRETAILIR_H

#include "CartRetail.h"

namespace melonDS::NDSCart
{

enum CartRetailIRVersion
{
    // Not an IR cartridge
    CartRetailIRVersionInvalid = 0,

    // IR cartridge supporting NTR-027
    // NTR-IMWX, NTR-IA8P
    CartRetailIRVersionEarly,

    // IR cartridge supporting NTR-032
    // NTR-IPKX, NTR-IPGX, TWL-IRBX, TWL-IRAX, TWL-IREX, TWL-IRDX
    CartRetailIRVersionLate
};

// CartRetailIR -- SPI IR device and SRAM
class CartRetailIR : public CartRetail
{
public:
    CartRetailIR(const u8* rom, u32 len, u32 chipid, u32 irversion, bool badDSiDump, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata);
    CartRetailIR(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, u32 irversion, bool badDSiDump, ROMListEntry romparams, std::unique_ptr<u8[]>&& sram, u32 sramlen, void* userdata);
    ~CartRetailIR() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void SPISelect() override;
    void SPIRelease() override;
    u8 SPITransmitReceive(u8 val) override;

private:
    u32 IRVersion = CartRetailIRVersionInvalid;
    u8 IRCmd = 0;
    u32 IRPos = 0;

    // bytes sent by the current send command (0x02)
    u32 IRSent = 0;

    // length of the packet reported by the current receive command (0x01)
    u32 IRReceived = 0;

    // the start of the packet being sent or received, and when the last one
    // was logged, for the packet log
    u8 IRLogBytes[16];
    u64 IRLogLast = 0;

    void LogPacket(const char* dir, u32 len);
};

}

#endif
