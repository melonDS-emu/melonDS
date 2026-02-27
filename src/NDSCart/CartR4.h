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

#ifndef NDSCART_CARTR4_H
#define NDSCART_CARTR4_H

#include "CartSD.h"

namespace melonDS::NDSCart
{

// CartR4 -- unlicensed R4 'cart' (NDSCartR4.cpp)
enum CartR4Type
{
    /* non-SDHC carts */
    CartR4TypeM3Simply = 0,
    CartR4TypeR4 = 1,
    /* SDHC carts */
    CartR4TypeAce3DS = 2
};

enum CartR4Language
{
    CartR4LanguageJapanese = (7 << 3) | 1,
    CartR4LanguageEnglish = (7 << 3) | 2,
    CartR4LanguageFrench = (2 << 3) | 2,
    CartR4LanguageKorean = (4 << 3) | 2,
    CartR4LanguageSimplifiedChinese = (6 << 3) | 3,
    CartR4LanguageTraditionalChinese = (7 << 3) | 3
};

class CartR4 : public CartSD
{
public:
    CartR4(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, CartR4Type ctype, CartR4Language clanguage, void* userdata,
           std::optional<FATStorage>&& sdcard = std::nullopt);
    ~CartR4() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    //int ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd, u8* data, u32 len) override;
    //void ROMCommandFinish(const u8* cmd, u8* data, u32 len) override;
    void ROMCommandStart(NDS& nds, NDSCart::NDSCartSlot& cartslot, const u8* cmd) override;
    u32 ROMCommandReceive() override;
    void ROMCommandTransmit(u32 val) override;
    void ROMCommandFinish() override;

private:
    inline u32 GetAdjustedSector(u32 sector) const
    {
        return R4CartType >= CartR4TypeAce3DS ? sector : sector >> 9;
    }

    u16 GetEncryptionKey(u16 sector);
    void ReadSDToBuffer(u32 sector, bool rom);
    u64 SDFATEntrySectorGet(u32 entry, u32 addr);

    s32 EncryptionKey;
    u32 FATEntryOffset[2];
    u8 Buffer[512];
    u32 BufferPos;
    u8 InitStatus;
    CartR4Type R4CartType;
    CartR4Language CartLanguage;
    bool BufferInitialized;
};

}

#endif
