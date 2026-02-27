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

#ifndef NDSCART_CARTCOMMON_H
#define NDSCART_CARTCOMMON_H

#include <memory>

#include "../types.h"
#include "../Savestate.h"
#include "../NDS_Header.h"
#include "../ROMList.h"

namespace melonDS
{
class NDS;
}
namespace melonDS::NDSCart
{

enum CartType
{
    Default = 0x001,
    Retail = 0x101,
    RetailNAND = 0x102,
    RetailIR = 0x103,
    RetailBT = 0x104,
    Homebrew = 0x201,
    UnlicensedR4 = 0x301
};

class NDSCartSlot;

// CartCommon -- base code shared by all cart types
class CartCommon
{
public:
    CartCommon(const u8* rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, CartType type, void* userdata);
    CartCommon(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, bool badDSiDump, ROMListEntry romparams, CartType type, void* userdata);
    virtual ~CartCommon();

    [[nodiscard]] u32 Type() const { return CartType; };
    [[nodiscard]] u32 Checksum() const;

    virtual void Reset();
    virtual void SetupDirectBoot(const std::string& romname, NDS& nds);

    virtual void DoSavestate(Savestate* file);

    virtual void ROMCommandStart(NDSCart::NDSCartSlot& cartslot, const u8* cmd);
    virtual u32 ROMCommandReceive();
    virtual void ROMCommandTransmit(u32 val) {}
    virtual void ROMCommandFinish() {}

    virtual void SPISelect() { SPISelected = true; }
    virtual void SPIRelease() { SPISelected = false; };
    virtual u8 SPITransmitReceive(u8 val) { return 0xFF; }

    virtual u8* GetSaveMemory() { return nullptr; }
    virtual const u8* GetSaveMemory() const { return nullptr; }
    virtual u32 GetSaveMemoryLength() const { return 0; }
    virtual void SetSaveMemory(const u8* savedata, u32 savelen) {};

    [[nodiscard]] const NDSHeader& GetHeader() const { return Header; }
    [[nodiscard]] NDSHeader& GetHeader() { return Header; }

    /// @return The cartridge's banner if available, or \c nullptr if not.
    [[nodiscard]] const NDSBanner* Banner() const;
    [[nodiscard]] const ROMListEntry& GetROMParams() const { return ROMParams; };
    [[nodiscard]] u32 ID() const { return ChipID; }
    [[nodiscard]] const u8* GetROM() const { return ROM.get(); }
    [[nodiscard]] u32 GetROMLength() const { return ROMLength; }

protected:
    u32 ROMRead32();

    void* UserData;

    std::unique_ptr<u8[]> ROM = nullptr;
    u32 ROMLength = 0;
    u32 ROMMask = 0;
    u32 ChipID = 0;
    bool IsDSi = false;
    bool DSiMode = false;
    u32 DSiBase = 0;

    // lenient addressing: do not redirect ROM accesses below 0x8000 etc
    bool LenientAddressing = false;

    u32 CmdEncMode = 0;
    u32 DataEncMode = 0;

    u8 ROMCmd[8] {};
    u32 ROMAddr = 0;

    bool SPISelected = false;

    // Kept separate from the ROM data so we can decrypt the modcrypt area
    // without touching the overall ROM data
    NDSHeader Header {};
    ROMListEntry ROMParams {};
    const melonDS::NDSCart::CartType CartType = Default;
};

}

#endif
