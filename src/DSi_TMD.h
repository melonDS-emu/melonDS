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

namespace DSi_TMD
{

struct [[gnu::packed]] TitleMetadataContent {
    /// Content ID (00,00,00,vv) ;lowercase/hex ;"0000000vv.app"
    u32 ContentId;

    /// Content Index (00,00)
    u16 ContentIndex;

    /// Content Type (00,01) ;aka DSi .app
    u16 ContentType;

    /// Content Size (00,00,00,00,00,19,E4,00)
    u64 ContentSize;

    /// Content SHA1 (on decrypted ".app" file)
    u8 ContentSha1Hash[0x14];
};

/// Metadata for a DSiWare title.
/// Used to install DSiWare titles to NAND.
/// @see https://problemkaputt.de/gbatek.htm#dsisdmmcdsiwareticketsandtitlemetadata
struct [[gnu::packed]] TitleMetadata
{
    /// Signature Type (00h,01h,00h,01h) (100h-byte RSA)
    u32 SignatureType;

    /// Signature RSA-OpenPGP-SHA1 across 140h..207h
    u8 Signature[0x100];

    /// Signature padding/alignment (zerofilled)
    u8 SignatureAlignment[0x3C];

    /// Signature Name "Root-CA00000001-CP00000007", 00h-padded
    u8 SignatureName[0x40];

    /// TMD Version (00h) (unlike 3DS)
    u8 TmdVersion;

    /// ca_crl_version (00h)
    u8 CaCrlVersion;

    /// signer_crl_version (00h)
    u8 SignerCrlVersion;

    /// (padding/align 4h)
    u8 Padding0;

    /// System Version (0)
    u64 SystemVersion;

    /// Title ID (00,03,00,17,"HNAP")
    u64 TitleId;

    /// Title Type (0)
    u32 TitleType;

    /// Group ID (eg. "01"=Nintendo)
    u16 GroupId;

    /// SD/MMC "public.sav" filesize in bytes (0=none)
    u32 PublicSaveSize;

    /// SD/MMC "private.sav" filesize in bytes (0=none)
    u32 PrivateSaveSize;

    /// Zero
    u8 Padding1[0x4];

    /// (3DS: SRL Flag)
    u8 SrlFlag;

    /// Zero
    u8 Padding2[0x3];

    /// Parental Control Age Ratings
    u8 AgeRatings[0x10];

    /// Zerofilled
    u8 Padding3[0x1e];

    /// Access rights (0)
    u32 AccessRights;

    /// Title Version (vv,00) (LITTLE-ENDIAN!?)
    u16 TitleVersion;

    /// Number of contents (at 1E4h and up) (usually 0 or 1)
    u16 NumberOfContents;

    /// boot content index (0)
    u16 BootContentIndex;

    /// Zerofilled (padding/align 4h)
    u8 Padding4[0x2];

    /// There's always one or zero content entries in practice
    TitleMetadataContent Contents;

    [[nodiscard]] bool HasPublicSaveData() const noexcept { return PublicSaveSize != 0; }
    [[nodiscard]] bool HasPrivateSaveData() const noexcept { return PrivateSaveSize != 0; }
};

static_assert(sizeof(TitleMetadata) == 0x208, "TitleMetadata is not 520 bytes!");

}

#endif //DSI_TMD_H
