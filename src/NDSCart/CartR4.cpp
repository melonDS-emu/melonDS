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

#include "../NDS.h"
#include "CartR4.h"

namespace melonDS
{
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

CartR4::CartR4(std::unique_ptr<u8[]>&& rom, u32 len, u32 chipid, ROMListEntry romparams, CartR4Type ctype, CartR4Language clanguage, void* userdata,
            std::optional<FATStorage>&& sdcard)
    : CartSD(std::move(rom), len, chipid, romparams, userdata, std::move(sdcard))
{
    InitStatus = 0;
    R4CartType = ctype;
    CartLanguage = clanguage;
}

CartR4::~CartR4()
{
}

void CartR4::Reset()
{
    CartSD::Reset();

    BufferInitialized = false;
    EncryptionKey = -1;
    FATEntryOffset[0] = 0xFFFFFFFF;
    FATEntryOffset[1] = 0xFFFFFFFF;

    if (!SD)
        InitStatus = 1;
    else
    {
        u8 buffer[512];
        if (!SD->ReadFile("_DS_MENU.DAT", 0, 512, buffer))
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

// FIXME: Ace3DS/clone behavior is only partially verified.
void CartR4::ROMCommandStart(NDSCart::NDSCartSlot& cartslot, const u8* cmd)
{
    if (CmdEncMode != 2)
        return CartSD::ROMCommandStart(cartslot, cmd);

    memcpy(ROMCmd, cmd, 8);

    switch (ROMCmd[0])
    {
    case 0xB4: /* FAT entry */
        {
            u8 entryBuffer[512];
            u32 sector = ((ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4]) & (~0x1F);
            // set FAT entry offset to the starting cluster, to gain a bit of speed
            SD->ReadSectors(sector >> 9, 1, entryBuffer);
            u16 fileEntryOffset = sector & 0x1FF;
            u32 clusterStart = (entryBuffer[fileEntryOffset + 27] << 8)
                | entryBuffer[fileEntryOffset + 26]
                | (entryBuffer[fileEntryOffset + 21] << 24)
                | (entryBuffer[fileEntryOffset + 20] << 16);
            FATEntryOffset[ROMCmd[4] & 0x01] = clusterStart;
            return;
        }
    case 0xB2: /* Save read request */
    case 0xB6: /* ROM read request */
        {
            u32 sector = ((ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4]);
            ReadSDToBuffer(sector, cmd[0] == 0xB6);
            return;
        }
    case 0xB9: /* SD read request */
        {
            u32 sector = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];
            if (SD)
                SD->ReadSectors(GetAdjustedSector(sector), 1, Buffer);
            return;
        }
    case 0xBB: /* SD write start */
    case 0xBD: /* Save write start */
        return;
    case 0xBC: /* SD write status */
    case 0xBE: /* Save write status */
        {
            if (R4CartType == CartR4TypeAce3DS && cmd[0] == 0xBC)
            {
                uint8_t checksum = 0;
                for (int i = 0; i < 7; i++)
                    checksum ^= cmd[i];
                if (checksum != cmd[7])
                    Log(LogLevel::Warn, "R4: invalid 0xBC command checksum (%d != %d)", cmd[7], checksum);
            }
            return;
        }
    case 0xB7: /* ROM read data */
        {
            /* If the buffer has not been initialized yet, emulate ROM. */
            /* TODO: When does the R4 do this exactly? */
            if (!BufferInitialized)
            {
                ROMAddr = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];
                return;
            }
            /* Otherwise, fall through. */
        }
    case 0xB3: /* Save read data */
    case 0xBA: /* SD read data */
        {
            BufferPos = 0;
            return;
        }
    case 0xBF: /* ROM read decrypted data */
        {
            // TODO: Is decryption done using the sector from 0xBF or 0xB6?
            u32 sector = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];
            DecryptR4Sector(SectorBuffer, Buffer, GetEncryptionKey(sector >> 9));
            return;
        }
    default:
        Log(LogLevel::Warn, "R4: unknown command %02X %02X %02X %02X %02X %02X %02X %02X\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7]);
        return;
    }
}

u32 CartR4::ROMCommandReceive()
{
    if (CmdEncMode != 2) return CartSD::ROMCommandReceive();

    switch (ROMCmd[0])
    {
    case 0xB0: /* Get card information */
        {
            u32 info = 0x75A00000 | (((R4CartType >= 1 ? 4 : 0) | CartLanguage) << 3) | InitStatus;
            return info;
        }
    case 0xB8: /* ? Get chip ID ? */
        return ChipID;
    case 0xB7: /* ROM read data */
        {
            /* If the buffer has not been initialized yet, emulate ROM. */
            /* TODO: When does the R4 do this exactly? */
            if (!BufferInitialized)
            {
                return *(u32*)&ROM[ROMAddr & (ROMLength-1)];
            }
            /* Otherwise, fall through. */
        }
    case 0xB3: /* Save read data */
    case 0xBA: /* SD read data */
        {
            // TODO: Do these use separate buffers?
            u32 ret = *(u32*)&Buffer[BufferPos];
            BufferPos += 4;
            BufferPos &= 0x1FF;
            return ret;
        }
    case 0xBF: /* ROM read decrypted data */
        {
            u32 ret = *(u32*)&SectorBuffer[SectorPos];
            SectorPos += 4;
            SectorPos &= 0x1FF;
            return ret;
        }
    default:
        return CartSD::ROMCommandReceive();
    }
}

void CartR4::ROMCommandTransmit(u32 val)
{
    if (CmdEncMode != 2) return CartSD::ROMCommandTransmit(val);

    switch (ROMCmd[0])
    {
    case 0xBB: /* SD write start */
    case 0xBD: /* Save write start */
        if (SectorPos < 512)
        {
            *(u32*)&SectorBuffer[SectorPos] = val;
            SectorPos += 4;
        }
        return;
    default:
        return CartSD::ROMCommandTransmit(val);
    }
}

void CartR4::ROMCommandFinish()
{
    if (CmdEncMode != 2) return CartSD::ROMCommandFinish();

    switch (ROMCmd[0])
    {
    case 0xBB: /* SD write start */
        {
            u32 sector = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];

            // The official R4 firmware sends a superfluous write to card
            // (sector 0, byte 1) on boot. TODO: Is this correct?
            if (GetAdjustedSector(sector) != sector && (sector & 0x1FF)) break;

            if (SD && !SD->IsReadOnly())
                SD->WriteSectors(GetAdjustedSector(sector), 1, SectorBuffer);
            break;
        }
    case 0xBD: /* Save write start */
        {
            u32 sector = (ROMCmd[1]<<24) | (ROMCmd[2]<<16) | (ROMCmd[3]<<8) | ROMCmd[4];

            if (sector & 0x1FF) break;

            if (SD && !SD->IsReadOnly())
                SD->WriteSectors(
                    SDFATEntrySectorGet(FATEntryOffset[1], sector) >> 9,
                    1, SectorBuffer
                );
            break;
        }
    default:
        return CartSD::ROMCommandFinish();
    }
}

u16 CartR4::GetEncryptionKey(u16 sector)
{
    if (EncryptionKey == -1)
    {
        u8 encryptedBuffer[512];
        u8 decryptedBuffer[512];
        SD->ReadFile("_DS_MENU.DAT", 0, 512, encryptedBuffer);
        for (u32 key = 0; key < 0x10000; key++)
        {
            DecryptR4Sector(decryptedBuffer, encryptedBuffer, key);
            if (decryptedBuffer[12] == '#' && decryptedBuffer[13] == '#'
                && decryptedBuffer[14] == '#' && decryptedBuffer[15] == '#')
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
        BufferInitialized = true;
    }
}

u64 CartR4::SDFATEntrySectorGet(u32 entry, u32 addr)
{
    u8 buffer[512];
    u32 bufferSector = 0xFFFFFFFF;

    // Parse FAT header.
    SD->ReadSectors(0, 1, buffer);
    u16 bytesPerSector = (buffer[12] << 8) | buffer[11];
    u8 sectorsPerCluster = buffer[13];
    u16 firstFatSector = (buffer[15] << 8) | buffer[14];
    u8 fatTableCount = buffer[16];
    u32 clustersTotal = SD->GetSectorCount() / sectorsPerCluster;
    bool isFat32 = clustersTotal >= 65526;

    u32 fatTableSize = (buffer[23] << 8) | buffer[22];
    if (fatTableSize == 0 && isFat32)
        fatTableSize = (buffer[39] << 24) | (buffer[38] << 16) | (buffer[37] << 8) | buffer[36];
    u32 bytesPerCluster = bytesPerSector * sectorsPerCluster;
    u32 rootDirSectors = 0;
    if (!isFat32) {
        u32 rootDirEntries = (buffer[18] << 8) | buffer[17];
        rootDirSectors = ((rootDirEntries * 32) + (bytesPerSector - 1)) / bytesPerSector;
    }
    u32 firstDataSector = firstFatSector + fatTableCount * fatTableSize + rootDirSectors;

    // Parse file entry (done when processing command 0xB4).
    u32 clusterStart = entry;

    // Parse cluster table.
    u32 currentCluster = clusterStart;
    while (true)
    {
        currentCluster &= isFat32 ? 0x0FFFFFFF : 0xFFFF;
        if (addr < bytesPerCluster)
        {
            // Read from this cluster.
            return (u64) (firstDataSector + ((currentCluster - 2) * sectorsPerCluster)) * bytesPerSector + addr;
        }
        else if (currentCluster >= 2 && currentCluster <= (isFat32 ? 0x0FFFFFF6 : 0xFFF6))
        {
            // Follow into next cluster.
            u32 nextClusterOffset = firstFatSector * bytesPerSector + currentCluster * (isFat32 ? 4 : 2);
            u32 nextClusterTableSector = nextClusterOffset >> 9;
            if (bufferSector != nextClusterTableSector)
            {
                SD->ReadSectors(nextClusterTableSector, 1, buffer);
                bufferSector = nextClusterTableSector;
            }
            nextClusterOffset &= 0x1FF;
            currentCluster = (buffer[nextClusterOffset + 1] << 8) | buffer[nextClusterOffset];
            if (isFat32)
                currentCluster |= (buffer[nextClusterOffset + 3] << 24) | (buffer[nextClusterOffset + 2] << 16);
            addr -= bytesPerCluster;
        }
        else
        {
            // End of cluster table.
            return 0;
        }
    }
}   

}
}
