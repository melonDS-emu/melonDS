/*
    Copyright 2016-2023 melonDS team

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

#ifndef DSI_TMD_H
#define DSI_TMD_H

#include "types.h"
#include <array>

namespace melonDS::DSi_TMD
{

struct TitleMetadataContent {
    u8 ContentId[4]; /// Content ID (00,00,00,vv) ;lowercase/hex ;"0000000vv.app"
    u8 ContentIndex[2]; /// Content Index (00,00)
    u8 ContentType[2]; /// Content Type (00,01) ;aka DSi .app
    u8 ContentSize[8]; /// Content Size (00,00,00,00,00,19,E4,00)
    u8 ContentSha1Hash[20]; /// Content SHA1 (on decrypted ".app" file)

    [[nodiscard]] u32 GetVersion() const noexcept
    {
        return (ContentId[0] << 24) | (ContentId[1] << 16) | (ContentId[2] << 8) | ContentId[3];
    }
};

static_assert(sizeof(TitleMetadataContent) == 36, "TitleMetadataContent is not 36 bytes!");

/// Metadata for a DSiWare title.
/// Used to install DSiWare titles to NAND.
/// @see https://problemkaputt.de/gbatek.htm#dsisdmmcdsiwareticketsandtitlemetadata
struct TitleMetadata
{
    u32 SignatureType;
    u8 Signature[256];
    u8 SignatureAlignment[60];
    char SignatureName[64];

    u8 TmdVersion;
    u8 CaCrlVersion;
    u8 SignerCrlVersion;
    u8 Padding0;

    u8 SystemVersion[8];
    u8 TitleId[8];
    u32 TitleType;
    u8 GroupId[2];
    u8 PublicSaveSize[4];
    u8 PrivateSaveSize[4];
    u8 Padding1[4];

    u8 SrlFlag;
    u8 Padding2[3];

    u8 AgeRatings[16];
    u8 Padding3[30];

    u32 AccessRights;
    u16 TitleVersion;

    u16 NumberOfContents; /// There's always one or zero content entries in practice
    u16 BootContentIndex;
    u8 Padding4[2];

    TitleMetadataContent Contents;

    [[nodiscard]] bool HasPublicSaveData() const noexcept { return GetPublicSaveSize() != 0; }
    [[nodiscard]] bool HasPrivateSaveData() const noexcept { return GetPrivateSaveSize() != 0; }

    [[nodiscard]] u32 GetPublicSaveSize() const noexcept
    {
        return (PublicSaveSize[0] << 24) | (PublicSaveSize[1] << 16) | (PublicSaveSize[2] << 8) | PublicSaveSize[3];
    }

    [[nodiscard]] u32 GetPrivateSaveSize() const noexcept
    {
        return (PrivateSaveSize[0] << 24) | (PrivateSaveSize[1] << 16) | (PrivateSaveSize[2] << 8) | PrivateSaveSize[3];
    }

    [[nodiscard]] u32 GetCategory() const noexcept
    {
       return (TitleId[0] << 24) | (TitleId[1] << 16) | (TitleId[2] << 8) | TitleId[3];
    }

    [[nodiscard]] u32 GetCategoryNoByteswap() const noexcept
    {
        return reinterpret_cast<const u32&>(TitleId);
    }

    [[nodiscard]] u32 GetID() const noexcept
    {
        return (TitleId[4] << 24) | (TitleId[5] << 16) | (TitleId[6] << 8) | TitleId[7];
    }

    [[nodiscard]] u32 GetIDNoByteswap() const noexcept
    {
        return *reinterpret_cast<const u32*>(&TitleId[4]);
    }
};

static_assert(sizeof(TitleMetadata) == 520, "TitleMetadata is not 520 bytes!");

}

#endif // DSI_TMD_H
