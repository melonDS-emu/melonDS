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

#include <string.h>
#include "NDS.h"
#include "DSi.h"
#include "NDSCart.h"

using Platform::Log;
using Platform::LogLevel;

namespace NDSCart
{

/*
    Original algorithm discovered by yasu, 2007

    http://hp.vector.co.jp/authors/VA013928/
    http://www.usay.jp/
    http://www.yasu.nu/
*/
static void DecryptR4Sector(u8* dest, u8* src, u16 key1)
{
    for (int i = 0; i < 512; i++)
    {
        // Derive key2 from key1.
        u8 key2 = ((key1 >> 7) & 0x80)
            | ((key1 >> 6) & 0x60)
            | ((key1 >> 5) & 0x10)
            | ((key1 >> 4) & 0x0C)
            | (key1 & 0x03);
        
        // Decrypt byte.
        dest[i] = src[i] ^ key2;

        // Derive next key1 from key2.
        u16 tmp = ((src[i] << 8) ^ key1);
        u16 tmpXor = 0;
        for (int ii = 0; ii < 16; ii++)
            tmpXor ^= (tmp >> ii);

        u16 newKey1 = 0;
        newKey1 |= ((tmpXor & 0x80) | (tmp & 0x7C)) << 8;
        newKey1 |= ((tmp ^ (tmpXor >> 14)) << 8) & 0x0300;
        newKey1 |= (((tmp >> 1) ^ tmp) >> 6) & 0xFC;
        newKey1 |= ((tmp ^ (tmpXor >> 1)) >> 8) & 0x03;

        key1 = newKey1;
    }
}

CartR4::CartR4(u8* rom, u32 len, u32 chipid, ROMListEntry romparams, CartR4Type ctype, CartR4Language clanguage)
    : CartSD(rom, len, chipid, romparams)
{
    InitStatus = 0;
    CartType = ctype;
    CartLanguage = clanguage;
}

CartR4::~CartR4()
{
}

void CartR4::Reset()
{
    CartSD::Reset();

    EncryptionKey = -1;
    FATEntryOffset[0] = 0xFFFFFFFF;
    FATEntryOffset[1] = 0xFFFFFFFF;

    if (!SD)
        InitStatus = 1;
    else
    {
        u8 Buffer[512];
        if (!SD->ReadFile("_DS_MENU.DAT", 0, 512, Buffer))
            InitStatus = 3;
        else
            InitStatus = 4;
    }
}

void CartR4::DoSavestate(Savestate* file)
{
    CartCommon::DoSavestate(file);

    file->Var32(&FATEntryOffset[0]);
    file->Var32(&FATEntryOffset[1]);
    file->VarArray(Buffer, 512);
}

int CartR4::ROMCommandStart(NDSCart::NDSCartSlot& cartslot, u8* cmd, u8* data, u32 len)
{
    if (CmdEncMode != 2)
        return CartCommon::ROMCommandStart(cartslot, cmd, data, len);

    switch (cmd[0])
    {
    case 0xB0: /* Get card information */
        {
            u32 Info = 0x75A00000 | ((CartType | CartLanguage) << 3) | InitStatus;
            for (u32 pos = 0; pos < len; pos += 4)
                *(u32*)&data[pos] = Info;
            return 0;
        }
    case 0xB4: /* FAT entry */
        {
            u8 EntryBuffer[512];
            u32 sector = ((cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4]) & (~0x1F);
            // set FAT entry offset to the starting cluster, to gain a bit of speed
            SD->ReadSectors(sector >> 9, 1, EntryBuffer);
            u16 FileEntryOffset = sector & 0x1FF;
            u32 ClusterStart = (EntryBuffer[FileEntryOffset + 27] << 8)
                | EntryBuffer[FileEntryOffset + 26]
                | (EntryBuffer[FileEntryOffset + 21] << 24)
                | (EntryBuffer[FileEntryOffset + 20] << 16);
            FATEntryOffset[cmd[4] & 0x01] = ClusterStart;
            for (u32 pos = 0; pos < len; pos += 4)
                *(u32*)&data[pos] = 0;
            return 0;
        }
    case 0xB8: /* ? Get chip ID ? */
        {
            for (u32 pos = 0; pos < len; pos += 4)
                *(u32*)&data[pos] = ChipID;
            return 0;
        }
    case 0xB2: /* Save read request */
    case 0xB6: /* ROM read request */
        {
            u32 sector = ((cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4]);
            ReadSDToBuffer(sector, cmd[0] == 0xB6);
            for (u32 pos = 0; pos < len; pos += 4)
                *(u32*)&data[pos] = 0;
            return 0;
        }
    case 0xB9: /* SD read request */
        {
            u32 sector = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            if (SD)
                SD->ReadSectors(sector >> 9, 1, Buffer);
            for (u32 pos = 0; pos < len; pos += 4)
                *(u32*)&data[pos] = 0;
            return 0;
        }
    case 0xBB: /* SD write start */
    case 0xBD: /* Save write start */
        return 1;
    case 0xBC: /* SD write status */
    case 0xBE: /* Save write status */
        {
            for (u32 pos = 0; pos < len; pos += 4)
                *(u32*)&data[pos] = 0;
            return 0;
        }
    case 0xB3: /* Save read data */
    case 0xB7: /* ROM read data */
    case 0xBA: /* SD read data */
        {
            // TODO: Do these use separate buffers?
            for (u32 pos = 0; pos < len; pos++)
                data[pos] = Buffer[pos & 0x1FF];
            return 0;
        }
    case 0xBF: /* ROM read decrypted data */
        {
            // TODO: Is decryption done using the sector from 0xBF or 0xB6?
            u32 sector = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            if (len >= 512)
                DecryptR4Sector(data, Buffer, GetEncryptionKey(sector >> 9));
            return 0;        
        }
    default:
        printf("R4: unknown command %02X %02X %02X %02X %02X %02X %02X %02X (%d)\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], len);
        for (u32 pos = 0; pos < len; pos += 4)
            *(u32*)&data[pos] = 0;
        return 0;
    }
}

void CartR4::ROMCommandFinish(u8* cmd, u8* data, u32 len)
{
    if (CmdEncMode != 2) return CartCommon::ROMCommandFinish(cmd, data, len);

    switch (cmd[0])
    {
    case 0xBB: /* SD write start */
        {
            u32 sector = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];

            // The official R4 firmware sends a superfluous write to card
            // (sector 0, byte 1) on boot. TODO: Is this correct?
            if (sector & 0x1FF) break;

            if (SD && (!ReadOnly))
                SD->WriteSectors(sector >> 9, 1, data);
            break;
        }
    case 0xBD: /* Save write start */
        {
            u32 sector = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];

            if (sector & 0x1FF) break;

            if (SD && (!ReadOnly))
                SD->WriteSectors(
                    SDFATEntrySectorGet(FATEntryOffset[1], sector) >> 9,
                    1, data
                );
            break;
        }
    }
}

u16 CartR4::GetEncryptionKey(u16 sector)
{
    if (EncryptionKey == -1)
    {
        u8 EncryptedBuffer[512];
        u8 DecryptedBuffer[512];
        SD->ReadFile("_DS_MENU.DAT", 0, 512, EncryptedBuffer);
        for (u32 key = 0; key < 0x10000; key++)
        {
            DecryptR4Sector(DecryptedBuffer, EncryptedBuffer, key);
            if (DecryptedBuffer[12] == '#' && DecryptedBuffer[13] == '#'
                && DecryptedBuffer[14] == '#' && DecryptedBuffer[15] == '#')
            {
                EncryptionKey = key;
                break;
            }
        }
        
        if (EncryptionKey != -1)
        {
            Log(LogLevel::Warn, "R4: found cartridge key = %04X\n", EncryptionKey);
        }
        else
        {
            EncryptionKey = -2;
            Log(LogLevel::Warn, "R4: could not find cartridge key\n");
        }
    }
    return EncryptionKey ^ sector;
}

void CartR4::ReadSDToBuffer(u32 sector, bool rom)
{
    if (SD)
    {
        if (rom && FATEntryOffset[0] == 0xFFFFFFFF)
        {
            // On first boot, read from _DS_MENU.DAT.
            SD->ReadFile("_DS_MENU.DAT", sector & ~0x1FF, 512, Buffer);
        }
        else
        {
            // Default mode.
            SD->ReadSectors(
                SDFATEntrySectorGet(FATEntryOffset[rom ? 0 : 1], sector) >> 9,
                1, Buffer
            );
        }
    }
}

u32 CartR4::SDFATEntrySectorGet(u32 entry, u32 addr)
{
    u8 Buffer[512];
    u32 BufferSector = 0xFFFFFFFF;

    // Parse FAT header.
    SD->ReadSectors(0, 1, Buffer);
    u16 BytesPerSector = (Buffer[12] << 8) | Buffer[11];
    u8 SectorsPerCluster = Buffer[13];
    u16 FirstFATSector = (Buffer[15] << 8) | Buffer[14];
    u8 FATTableCount = Buffer[16];
    u32 ClustersTotal = SD->GetSectorCount() / SectorsPerCluster;
    bool IsFat32 = ClustersTotal >= 65526;

    u32 FATTableSize = (Buffer[23] << 8) | Buffer[22];
    if (FATTableSize == 0 && IsFat32)
        FATTableSize = (Buffer[39] << 24) | (Buffer[38] << 16) | (Buffer[37] << 8) | Buffer[36];
    u32 BytesPerCluster = BytesPerSector * SectorsPerCluster;
    u32 RootDirSectors = 0;
    if (!IsFat32) {
        u32 RootDirEntries = (Buffer[18] << 8) | Buffer[17];
        RootDirSectors = ((RootDirEntries * 32) + (BytesPerSector - 1)) / BytesPerSector;
    }
    u32 FirstDataSector = FirstFATSector + FATTableCount * FATTableSize + RootDirSectors;

    // Parse file entry (done when processing command 0xB4).
    u32 ClusterStart = entry;

    // Parse cluster table.
    u32 CurrentCluster = ClusterStart;
    while (true)
    {
        CurrentCluster &= IsFat32 ? 0x0FFFFFFF : 0xFFFF;
        if (addr < BytesPerCluster)
        {
            // Read from this cluster.    
            u32 SectorAddr = (FirstDataSector + ((CurrentCluster - 2) * SectorsPerCluster)) * BytesPerSector + addr;
            return SectorAddr;
        }
        else if (CurrentCluster >= 2 && CurrentCluster <= (IsFat32 ? 0x0FFFFFF6 : 0xFFF6))
        {
            // Follow into next cluster.
            u32 NextClusterOffset = FirstFATSector * BytesPerSector + CurrentCluster * (IsFat32 ? 4 : 2);
            u32 NextClusterTableSector = NextClusterOffset >> 9;
            if (BufferSector != NextClusterTableSector)
            {
                SD->ReadSectors(NextClusterTableSector, 1, Buffer);
                BufferSector = NextClusterTableSector;
            }
            NextClusterOffset &= 0x1FF;
            CurrentCluster = (Buffer[NextClusterOffset + 1] << 8) | Buffer[NextClusterOffset];
            if (IsFat32)
                CurrentCluster |= (Buffer[NextClusterOffset + 3] << 24) | (Buffer[NextClusterOffset + 2] << 16);
            addr -= BytesPerCluster;
        }
        else
        {
            // End of cluster table.
            return 0;
        }
    }
}   

}
