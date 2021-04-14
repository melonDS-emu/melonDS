/*
    Copyright 2016-2021 Arisotura

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

#include <stdio.h>
#include <string.h>
#include "NDS.h"
#include "DSi.h"
#include "NDSCart.h"
#include "ARM.h"
#include "DSi_AES.h"
#include "Platform.h"
#include "Config.h"
#include "ROMList.h"
#include "melonDLDI.h"
#include "NDSCart_SRAMManager.h"

// SRAM TODO: emulate write delays???

namespace NDSCart_SRAM
{

u8* SRAM;
u32 SRAMLength;

char SRAMPath[1024];
bool SRAMFileDirty;

void (*WriteFunc)(u8 val, bool islast);

u32 Hold;
u8 CurCmd;
u32 DataPos;
u8 Data;

u8 StatusReg;
u32 Addr;


void Write_Null(u8 val, bool islast);
void Write_EEPROMTiny(u8 val, bool islast);
void Write_EEPROM(u8 val, bool islast);
void Write_Flash(u8 val, bool islast);


bool Init()
{
    SRAM = NULL;
    return true;
}

void DeInit()
{
    if (SRAM) delete[] SRAM;
}

void Reset()
{
    if (SRAM) delete[] SRAM;
    SRAM = NULL;
}

void DoSavestate(Savestate* file)
{
    file->Section("NDCS");

    // we reload the SRAM contents.
    // it should be the same file (as it should be the same ROM, duh)
    // but the contents may change

    //if (!file->Saving && SRAMLength)
    //    delete[] SRAM;

    u32 oldlen = SRAMLength;

    file->Var32(&SRAMLength);
    if (SRAMLength != oldlen)
    {
        printf("savestate: VERY BAD!!!! SRAM LENGTH DIFFERENT. %d -> %d\n", oldlen, SRAMLength);
        printf("oh well. loading it anyway. adsfgdsf\n");

        if (oldlen) delete[] SRAM;
        if (SRAMLength) SRAM = new u8[SRAMLength];
    }
    if (SRAMLength)
    {
        //if (!file->Saving)
        //    SRAM = new u8[SRAMLength];

        file->VarArray(SRAM, SRAMLength);
    }

    // SPI status shito

    file->Var32(&Hold);
    file->Var8(&CurCmd);
    file->Var32(&DataPos);
    file->Var8(&Data);

    file->Var8(&StatusReg);
    file->Var32(&Addr);

    // SRAMManager might now have an old buffer (or one from the future or alternate timeline!)
    if (!file->Saving)
        NDSCart_SRAMManager::RequestFlush();
}

void LoadSave(const char* path, u32 type)
{
    if (SRAM) delete[] SRAM;

    strncpy(SRAMPath, path, 1023);
    SRAMPath[1023] = '\0';

    FILE* f = Platform::OpenFile(path, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        SRAMLength = (u32)ftell(f);
        SRAM = new u8[SRAMLength];

        fseek(f, 0, SEEK_SET);
        fread(SRAM, SRAMLength, 1, f);

        fclose(f);
    }
    else
    {
        if (type > 9) type = 0;
        int sramlen[] = {0, 512, 8192, 65536, 128*1024, 256*1024, 512*1024, 1024*1024, 8192*1024, 32768*1024};
        SRAMLength = sramlen[type];

        if (SRAMLength)
        {
            SRAM = new u8[SRAMLength];
            memset(SRAM, 0xFF, SRAMLength);
        }
    }

    SRAMFileDirty = false;
    NDSCart_SRAMManager::Setup(path, SRAM, SRAMLength);

    switch (SRAMLength)
    {
    case 512: WriteFunc = Write_EEPROMTiny; break;
    case 8192:
    case 65536:
    case 128*1024: WriteFunc = Write_EEPROM; break;
    case 256*1024:
    case 512*1024:
    case 1024*1024:
    case 8192*1024: WriteFunc = Write_Flash; break;
    case 32768*1024: WriteFunc = Write_Null; break; // NAND FLASH, handled differently
    default:
        printf("!! BAD SAVE LENGTH %d\n", SRAMLength);
    case 0:
        WriteFunc = Write_Null;
        break;
    }

    Hold = 0;
    CurCmd = 0;
    Data = 0;
    StatusReg = 0x00;
}

void RelocateSave(const char* path, bool write)
{
    if (!write)
    {
        LoadSave(path, 0); // lazy
        return;
    }

    strncpy(SRAMPath, path, 1023);
    SRAMPath[1023] = '\0';

    FILE* f = Platform::OpenFile(path, "wb");
    if (!f)
    {
        printf("NDSCart_SRAM::RelocateSave: failed to create new file. fuck\n");
        return;
    }

    fwrite(SRAM, SRAMLength, 1, f);
    fclose(f);
}

u8 Read()
{
    return Data;
}

void Write_Null(u8 val, bool islast) {}

void Write_EEPROMTiny(u8 val, bool islast)
{
    switch (CurCmd)
    {
    case 0x02:
    case 0x0A:
        if (DataPos < 1)
        {
            Addr = val;
            Data = 0;
        }
        else
        {
            SRAM[(Addr + ((CurCmd==0x0A)?0x100:0)) & 0x1FF] = val;
            Addr++;
        }
        break;

    case 0x03:
    case 0x0B:
        if (DataPos < 1)
        {
            Addr = val;
            Data = 0;
        }
        else
        {
            Data = SRAM[(Addr + ((CurCmd==0x0B)?0x100:0)) & 0x1FF];
            Addr++;
        }
        break;

    case 0x9F:
        Data = 0xFF;
        break;

    default:
        if (DataPos==0)
            printf("unknown tiny EEPROM save command %02X\n", CurCmd);
        break;
    }
}

void Write_EEPROM(u8 val, bool islast)
{
    u32 addrsize = 2;
    if (SRAMLength > 65536) addrsize++;

    switch (CurCmd)
    {
    case 0x02:
        if (DataPos < addrsize)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        else
        {
            SRAM[Addr & (SRAMLength-1)] = val;
            Addr++;
        }
        break;

    case 0x03:
        if (DataPos < addrsize)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        else
        {
            Data = SRAM[Addr & (SRAMLength-1)];
            Addr++;
        }
        break;

    case 0x9F:
        Data = 0xFF;
        break;

    default:
        if (DataPos==0)
            printf("unknown EEPROM save command %02X\n", CurCmd);
        break;
    }
}

void Write_Flash(u8 val, bool islast)
{
    switch (CurCmd)
    {
    case 0x02:
        if (DataPos < 3)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        else
        {
            SRAM[Addr & (SRAMLength-1)] = 0;
            Addr++;
        }
        break;

    case 0x03:
        if (DataPos < 3)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        else
        {
            Data = SRAM[Addr & (SRAMLength-1)];
            Addr++;
        }
        break;

    case 0x0A:
        if (DataPos < 3)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        else
        {
            SRAM[Addr & (SRAMLength-1)] = val;
            Addr++;
        }
        break;

    case 0x9F:
        Data = 0xFF;
        break;

    case 0xD8:
        if (DataPos < 3)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        if (DataPos == 2)
        {
            for (u32 i = 0; i < 0x10000; i++)
            {
                SRAM[Addr & (SRAMLength-1)] = 0;
                Addr++;
            }
        }
        break;

    case 0xDB:
        if (DataPos < 3)
        {
            Addr <<= 8;
            Addr |= val;
            Data = 0;
        }
        if (DataPos == 2)
        {
            for (u32 i = 0; i < 0x100; i++)
            {
                SRAM[Addr & (SRAMLength-1)] = 0;
                Addr++;
            }
        }
        break;

    default:
        if (DataPos==0)
            printf("unknown Flash save command %02X\n", CurCmd);
        break;
    }
}

void Write(u8 val, u32 hold)
{
    bool islast = false;

    if (!hold)
    {
        if (Hold) islast = true;
        else CurCmd = val;
        Hold = 0;
    }

    if (hold && (!Hold))
    {
        CurCmd = val;
        Hold = 1;
        Data = 0;
        DataPos = 0;
        Addr = 0;
        //printf("save SPI command %02X\n", CurCmd);
        return;
    }

    switch (CurCmd)
    {
    case 0x00:
        // Pokémon carts have an IR transceiver thing, and send this
        // to bypass it and access SRAM.
        // TODO: design better
        CurCmd = val;
        break;
    case 0x08:
        // see above
        // TODO: work out how the IR thing works. emulate it.
        Data = 0xAA;
        break;

    case 0x02:
    case 0x03:
    case 0x0A:
    case 0x0B:
    case 0x9F:
    case 0xD8:
    case 0xDB:
        WriteFunc(val, islast);
        DataPos++;
        break;

    case 0x04: // write disable
        StatusReg &= ~(1<<1);
        Data = 0;
        break;

    case 0x05: // read status reg
        Data = StatusReg;
        break;

    case 0x06: // write enable
        StatusReg |= (1<<1);
        Data = 0;
        break;

    default:
        if (DataPos==0)
            printf("unknown save SPI command %02X %02X %d\n", CurCmd, val, islast);
        break;
    }

    SRAMFileDirty |= islast && (CurCmd == 0x02 || CurCmd == 0x0A) && (SRAMLength > 0);
}

void FlushSRAMFile()
{
    if (!SRAMFileDirty) return;

    SRAMFileDirty = false;
    NDSCart_SRAMManager::RequestFlush();
}

}


namespace NDSCart
{

u16 SPICnt;
u32 ROMCnt;

u8 SPIData;
u32 SPIDataPos;
bool SPIHold;

u8 ROMCommand[8];
u32 ROMData;

u8 TransferData[0x4000];
u32 TransferPos;
u32 TransferLen;
u32 TransferDir;
u8 TransferCmd[8];

bool CartInserted;
u8* CartROM;
u32 CartROMSize;
u32 CartID;
bool CartIsHomebrew;
bool CartIsDSi;

CartCommon* Cart;

u32 Key1_KeyBuf[0x412];

u64 Key2_X;
u64 Key2_Y;


u32 ByteSwap(u32 val)
{
    return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | (val << 24);
}

void Key1_Encrypt(u32* data)
{
    u32 y = data[0];
    u32 x = data[1];
    u32 z;

    for (u32 i = 0x0; i <= 0xF; i++)
    {
        z = Key1_KeyBuf[i] ^ x;
        x =  Key1_KeyBuf[0x012 +  (z >> 24)        ];
        x += Key1_KeyBuf[0x112 + ((z >> 16) & 0xFF)];
        x ^= Key1_KeyBuf[0x212 + ((z >>  8) & 0xFF)];
        x += Key1_KeyBuf[0x312 +  (z        & 0xFF)];
        x ^= y;
        y = z;
    }

    data[0] = x ^ Key1_KeyBuf[0x10];
    data[1] = y ^ Key1_KeyBuf[0x11];
}

void Key1_Decrypt(u32* data)
{
    u32 y = data[0];
    u32 x = data[1];
    u32 z;

    for (u32 i = 0x11; i >= 0x2; i--)
    {
        z = Key1_KeyBuf[i] ^ x;
        x =  Key1_KeyBuf[0x012 +  (z >> 24)        ];
        x += Key1_KeyBuf[0x112 + ((z >> 16) & 0xFF)];
        x ^= Key1_KeyBuf[0x212 + ((z >>  8) & 0xFF)];
        x += Key1_KeyBuf[0x312 +  (z        & 0xFF)];
        x ^= y;
        y = z;
    }

    data[0] = x ^ Key1_KeyBuf[0x1];
    data[1] = y ^ Key1_KeyBuf[0x0];
}

void Key1_ApplyKeycode(u32* keycode, u32 mod)
{
    Key1_Encrypt(&keycode[1]);
    Key1_Encrypt(&keycode[0]);

    u32 temp[2] = {0,0};

    for (u32 i = 0; i <= 0x11; i++)
    {
        Key1_KeyBuf[i] ^= ByteSwap(keycode[i % mod]);
    }
    for (u32 i = 0; i <= 0x410; i+=2)
    {
        Key1_Encrypt(temp);
        Key1_KeyBuf[i  ] = temp[1];
        Key1_KeyBuf[i+1] = temp[0];
    }
}

void Key1_InitKeycode(bool dsi, u32 idcode, u32 level, u32 mod)
{
    // TODO: source the key data from different possible places
    if (dsi && NDS::ConsoleType==1)
        memcpy(Key1_KeyBuf, &DSi::ARM7iBIOS[0xC6D0], 0x1048); // hax
    else
        memcpy(Key1_KeyBuf, &NDS::ARM7BIOS[0x30], 0x1048); // hax

    u32 keycode[3] = {idcode, idcode>>1, idcode<<1};
    if (level >= 1) Key1_ApplyKeycode(keycode, mod);
    if (level >= 2) Key1_ApplyKeycode(keycode, mod);
    if (level >= 3)
    {
        keycode[1] <<= 1;
        keycode[2] >>= 1;
        Key1_ApplyKeycode(keycode, mod);
    }
}


void Key2_Encrypt(u8* data, u32 len)
{
    for (u32 i = 0; i < len; i++)
    {
        Key2_X = (((Key2_X >> 5) ^
                   (Key2_X >> 17) ^
                   (Key2_X >> 18) ^
                   (Key2_X >> 31)) & 0xFF)
                 + (Key2_X << 8);
        Key2_Y = (((Key2_Y >> 5) ^
                   (Key2_Y >> 23) ^
                   (Key2_Y >> 18) ^
                   (Key2_Y >> 31)) & 0xFF)
                 + (Key2_Y << 8);

        Key2_X &= 0x0000007FFFFFFFFFULL;
        Key2_Y &= 0x0000007FFFFFFFFFULL;
    }
}


void ApplyModcrypt(u32 addr, u32 len, u8* iv)
{return;
    u8 key[16];

    DSi_AES::GetModcryptKey(&CartROM[0], key);
    DSi_AES::ApplyModcrypt(&CartROM[addr], len, key, iv);
}


CartCommon::CartCommon(u8* rom, u32 len, u32 chipid)
{
    ROM = rom;
    ROMLength = len;
    ChipID = chipid;

    u8 unitcode = ROM[0x12];
    IsDSi = (unitcode & 0x02) != 0;
}

CartCommon::~CartCommon()
{
}

void CartCommon::Reset()
{
    CmdEncMode = 0;
    DataEncMode = 0;
}

void CartCommon::SetupDirectBoot()
{
    CmdEncMode = 2;
    DataEncMode = 2;
}

void CartCommon::DoSavestate(Savestate* file)
{
    // TODO?
}

void CartCommon::LoadSave(const char* path, u32 type)
{
}

void CartCommon::RelocateSave(const char* path, bool write)
{
}

void CartCommon::FlushSRAMFile()
{
}

int CartCommon::ROMCommandStart(u8* cmd, u8* data, u32 len)
{
    switch (cmd[0])
    {
    case 0x9F:
        memset(data, 0xFF, len);
        return 0;

    case 0x00:
        memset(data, 0, len);
        if (len > 0x1000)
        {
            ReadROM(0, 0x1000, data, 0);
            for (u32 pos = 0x1000; pos < len; pos += 0x1000)
                memcpy(data+pos, data, 0x1000);
        }
        else
            ReadROM(0, len, data, 0);
        return 0;

    case 0x90:
    case 0xB8:
        for (u32 pos = 0; pos < len; pos += 4)
            *(u32*)&data[pos] = ChipID;
        return 0;

    case 0x3C:
        CmdEncMode = 1;
        Key1_InitKeycode(false, *(u32*)&ROM[0xC], 2, 2);
        return 0;

    case 0x3D:
        if (IsDSi)
        {
            CmdEncMode = 11;
            Key1_InitKeycode(true, *(u32*)&ROM[0xC], 1, 2);
        }
        return 0;

    default:
        if (CmdEncMode == 1 || CmdEncMode == 11)
        {
            // decrypt the KEY1 command as needed
            // (KEY2 commands do not need decrypted because KEY2 is handled entirely by hardware,
            // but KEY1 is not, so DS software is responsible for encrypting KEY1 commands)
            u8 cmddec[8];
            *(u32*)&cmddec[0] = ByteSwap(*(u32*)&cmd[4]);
            *(u32*)&cmddec[4] = ByteSwap(*(u32*)&cmd[0]);
            Key1_Decrypt((u32*)cmddec);
            u32 tmp = ByteSwap(*(u32*)&cmddec[4]);
            *(u32*)&cmddec[4] = ByteSwap(*(u32*)&cmddec[0]);
            *(u32*)&cmddec[0] = tmp;

            // TODO eventually: verify all the command parameters and shit

            switch (cmddec[0] & 0xF0)
            {
            case 0x40:
                DataEncMode = 2;
                return 0;

            case 0x10:
                for (u32 pos = 0; pos < len; pos += 4)
                    *(u32*)&data[pos] = ChipID;
                return 0;

            case 0x20:
                {
                    u32 addr = (cmddec[2] & 0xF0) << 8;
                    if (CmdEncMode == 11)
                    {
                        // the DSi region starts with 0x3000 unreadable bytes
                        // similarly to how the DS region starts at 0x1000 with 0x3000 unreadable bytes
                        // these contain data for KEY1 crypto
                        u32 dsiregion = *(u16*)&ROM[0x92] << 19;
                        addr -= 0x1000;
                        addr += dsiregion;
                    }
                    ReadROM(addr, 0x1000, data, 0);
                }
                return 0;

            case 0xA0:
                CmdEncMode = 2;
                return 0;
            }
        }
        return 0;
    }
}

void CartCommon::ROMCommandFinish(u8* cmd, u8* data, u32 len)
{
}

u8 CartCommon::SPIWrite(u8 val, u32 pos, bool last)
{
    return 0xFF;
}

void CartCommon::ReadROM(u32 addr, u32 len, u8* data, u32 offset)
{
    if (addr >= ROMLength) return;
    if ((addr+len) > ROMLength)
        len = ROMLength - addr;

    memcpy(data+offset, ROM+addr, len);
}


CartRetail::CartRetail(u8* rom, u32 len, u32 chipid) : CartCommon(rom, len, chipid)
{
    SRAM = nullptr;
}

CartRetail::~CartRetail()
{
    if (SRAM) delete[] SRAM;
}

void CartRetail::Reset()
{
    SRAMCmd = 0;
    SRAMAddr = 0;
    SRAMStatus = 0;
}

void CartRetail::DoSavestate(Savestate* file)
{
    // TODO?
}

void CartRetail::LoadSave(const char* path, u32 type)
{
    if (SRAM) delete[] SRAM;

    strncpy(SRAMPath, path, 1023);
    SRAMPath[1023] = '\0';

    if (type > 9) type = 0;
    int sramlen[] = {0, 512, 8192, 65536, 128*1024, 256*1024, 512*1024, 1024*1024, 8192*1024, 8192*1024};
    SRAMLength = sramlen[type];

    if (SRAMLength)
    {
        SRAM = new u8[SRAMLength];
        memset(SRAM, 0xFF, SRAMLength);
    }

    FILE* f = Platform::OpenFile(path, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_SET);
        fread(SRAM, 1, SRAMLength, f);

        fclose(f);
    }

    SRAMFileDirty = false;
    NDSCart_SRAMManager::Setup(path, SRAM, SRAMLength);

    switch (type)
    {
    case 1: SRAMType = 1; break; // EEPROM, small
    case 2:
    case 3:
    case 4: SRAMType = 2; break; // EEPROM, regular
    case 5:
    case 6:
    case 7:
    case 8: SRAMType = 3; break; // FLASH
    case 9: SRAMType = 4; break; // NAND
    default: SRAMType = 0; break; // ...whatever else
    }
}

void CartRetail::RelocateSave(const char* path, bool write)
{
    if (!write)
    {
        LoadSave(path, 0); // lazy
        return;
    }

    strncpy(SRAMPath, path, 1023);
    SRAMPath[1023] = '\0';

    FILE* f = Platform::OpenFile(path, "wb");
    if (!f)
    {
        printf("NDSCart_SRAM::RelocateSave: failed to create new file. fuck\n");
        return;
    }

    fwrite(SRAM, SRAMLength, 1, f);
    fclose(f);
}

void CartRetail::FlushSRAMFile()
{
    if (!SRAMFileDirty) return;

    SRAMFileDirty = false;
    NDSCart_SRAMManager::RequestFlush();
}

int CartRetail::ROMCommandStart(u8* cmd, u8* data, u32 len)
{
    switch (cmd[0])
    {
    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            memset(data, 0, len);

            if (((addr + len - 1) >> 12) != (addr >> 12))
            {
                u32 len1 = 0x1000 - (addr & 0xFFF);
                ReadROM_B7(addr, len1, data, 0);
                ReadROM_B7(addr+len1, len-len1, data, len1);
            }
            else
                ReadROM_B7(addr, len, data, 0);
        }
        return 0;

    default:
        return CartCommon::ROMCommandStart(cmd, data, len);
    }
}

u8 CartRetail::SPIWrite(u8 val, u32 pos, bool last)
{
    if (SRAMType == 0) return 0;

    if (pos == 0)
    {
        // handle generic commands with no parameters
        switch (val)
        {
        case 0x04: // write disable
            SRAMStatus &= ~(1<<1);
            break;
        case 0x06: // write enable
            SRAMStatus |= (1<<1);
            break;

        default:
            SRAMCmd = val;
            SRAMAddr = 0;
        }

        return val;
    }

    switch (SRAMType)
    {
    case 1: return SRAMWrite_EEPROMTiny(val, pos, last);
    case 2: return SRAMWrite_EEPROM(val, pos, last);
    case 3: return SRAMWrite_FLASH(val, pos, last);
    default: return 0;
    }

    //SRAMFileDirty |= last && (SRAMCmd == 0x02 || SRAMCmd == 0x0A);
    //return ret;
}

void CartRetail::ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset)
{
    addr &= (ROMLength-1);

    if (addr < 0x8000)
        addr = 0x8000 + (addr & 0x1FF);

    // TODO: protect DSi secure area
    // also protect DSi region if not unlocked
    // and other security shenanigans

    memcpy(data+offset, ROM+addr, len);
}

u8 CartRetail::SRAMWrite_EEPROMTiny(u8 val, u32 pos, bool last)
{
    switch (SRAMCmd)
    {
    case 0x01: // write status register
        // TODO: WP bits should be nonvolatile!
        if (pos == 1)
            SRAMStatus = (SRAMStatus & 0x01) | (val & 0x0C);
        return val;

    case 0x05: // read status register
        return SRAMStatus | 0xF0;

    case 0x02: // write low
    case 0x0A: // write high
        if (pos < 2)
        {
            SRAMAddr = val;
        }
        else
        {
            // TODO: implement WP bits!
            if (SRAMStatus & (1<<1))
            {
                SRAM[(SRAMAddr + ((SRAMCmd==0x0A)?0x100:0)) & 0x1FF] = val;
                SRAMFileDirty |= last;
            }
            SRAMAddr++;
        }
        if (last) SRAMStatus &= ~(1<<1);
        return val;

    case 0x03: // read low
    case 0x0B: // read high
        if (pos < 2)
        {
            SRAMAddr = val;
            return val;
        }
        else
        {
            u8 ret = SRAM[(SRAMAddr + ((SRAMCmd==0x0B)?0x100:0)) & 0x1FF];
            SRAMAddr++;
            return ret;
        }

    case 0x9F: // read JEDEC ID
        return 0xFF;

    default:
        if (pos == 1)
            printf("unknown tiny EEPROM save command %02X\n", SRAMCmd);
        return val;
    }
}

u8 CartRetail::SRAMWrite_EEPROM(u8 val, u32 pos, bool last)
{
    u32 addrsize = 2;
    if (SRAMLength > 65536) addrsize++;

    switch (SRAMCmd)
    {
    case 0x01: // write status register
        // TODO: WP bits should be nonvolatile!
        if (pos == 1)
            SRAMStatus = (SRAMStatus & 0x01) | (val & 0x0C);
        return val;

    case 0x05: // read status register
        return SRAMStatus;

    case 0x02: // write
        if (pos <= addrsize)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
        }
        else
        {
            // TODO: implement WP bits
            if (SRAMStatus & (1<<1))
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = val;
                SRAMFileDirty |= last;
            }
            SRAMAddr++;
        }
        if (last) SRAMStatus &= ~(1<<1);
        return val;

    case 0x03: // read
        if (pos <= addrsize)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            return val;
        }
        else
        {
            // TODO: size limit!!
            u8 ret = SRAM[SRAMAddr & (SRAMLength-1)];
            SRAMAddr++;
            return ret;
        }

    case 0x9F: // read JEDEC ID
        // TODO: GBAtek implies it's not always all FF (FRAM)
        return 0xFF;

    default:
        if (pos == 1)
            printf("unknown EEPROM save command %02X\n", SRAMCmd);
        return val;
    }
}

u8 CartRetail::SRAMWrite_FLASH(u8 val, u32 pos, bool last)
{
    switch (SRAMCmd)
    {
    case 0x05: // read status register
        return SRAMStatus;

    case 0x02: // page program
        if (pos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
        }
        else
        {
            if (SRAMStatus & (1<<1))
            {
                // CHECKME: should it be &=~val ??
                SRAM[SRAMAddr & (SRAMLength-1)] = 0;
                SRAMFileDirty |= last;
            }
            SRAMAddr++;
        }
        if (last) SRAMStatus &= ~(1<<1);
        return val;

    case 0x03: // read
        if (pos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
            return val;
        }
        else
        {
            u8 ret = SRAM[SRAMAddr & (SRAMLength-1)];
            SRAMAddr++;
            return ret;
        }

    case 0x0A: // page write
        if (pos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
        }
        else
        {
            if (SRAMStatus & (1<<1))
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = val;
                SRAMFileDirty |= last;
            }
            SRAMAddr++;
        }
        if (last) SRAMStatus &= ~(1<<1);
        return val;

    case 0x9F: // read JEDEC IC
        // GBAtek says it should be 0xFF. verify?
        return 0xFF;

    case 0xD8: // sector erase
        if (pos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
        }
        if ((pos == 3) && (SRAMStatus & (1<<1)))
        {
            for (u32 i = 0; i < 0x10000; i++)
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = 0;
                SRAMAddr++;
            }
            SRAMFileDirty = true;
        }
        if (last) SRAMStatus &= ~(1<<1);
        return val;

    case 0xDB: // page erase
        if (pos <= 3)
        {
            SRAMAddr <<= 8;
            SRAMAddr |= val;
        }
        if ((pos == 3) && (SRAMStatus & (1<<1)))
        {
            for (u32 i = 0; i < 0x100; i++)
            {
                SRAM[SRAMAddr & (SRAMLength-1)] = 0;
                SRAMAddr++;
            }
            SRAMFileDirty = true;
        }
        if (last) SRAMStatus &= ~(1<<1);
        return val;

    default:
        if (pos == 1)
            printf("unknown FLASH save command %02X\n", SRAMCmd);
        return val;
    }
}


CartRetailNAND::CartRetailNAND(u8* rom, u32 len, u32 chipid) : CartRetail(rom, len, chipid)
{
}

CartRetailNAND::~CartRetailNAND()
{
}

void CartRetailNAND::Reset()
{
    SRAMAddr = 0;
    SRAMStatus = 0x20;
    SRAMWindow = 0;

    // ROM header 94/96 = SRAM addr start / 0x20000
    SRAMBase = *(u16*)&ROM[0x96] << 17;

    memset(SRAMWriteBuffer, 0, 0x800);
}

void CartRetailNAND::DoSavestate(Savestate* file)
{
    // TODO?
}

void CartRetailNAND::LoadSave(const char* path, u32 type)
{
    CartRetail::LoadSave(path, type);

    // the last 128K of the SRAM are read-only.
    // most of it is FF, except for the NAND ID at the beginning
    // of the last 0x800 bytes.

    if (SRAMLength > 0x20000)
    {
        memset(&SRAM[SRAMLength - 0x20000], 0xFF, 0x20000);

        // TODO: check what the data is all about!
        // this was pulled from a Jam with the Band cart. may be different on other carts.
        // WarioWare DIY may have different data or not have this at all.
        // the ID data is also found in the response to command 94, and JwtB checks it.
        // WarioWare doesn't seem to care.
        // there is also more data here, but JwtB doesn't seem to care.
        u8 iddata[0x10] = {0xEC, 0x00, 0x9E, 0xA1, 0x51, 0x65, 0x34, 0x35, 0x30, 0x35, 0x30, 0x31, 0x19, 0x19, 0x02, 0x0A};
        memcpy(&SRAM[SRAMLength - 0x800], iddata, 16);
    }
}

int CartRetailNAND::ROMCommandStart(u8* cmd, u8* data, u32 len)
{
    switch (cmd[0])
    {
    case 0x81: // write data
        if ((SRAMStatus & (1<<4)) && SRAMWindow >= SRAMBase && SRAMWindow < (SRAMBase+0x800000))
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];

            // the command is issued 4 times, each with the same address
            // seems they use the one from the first command (CHECKME)
            if (!SRAMAddr)
                SRAMAddr = addr;
        }
        else
            SRAMAddr = 0;
        return 1;

    case 0x82: // commit write
        if (SRAMAddr && SRAMWritePos)
        {
            if (SRAMLength && SRAMAddr < (SRAMBase+SRAMLength-0x20000))
            {
                memcpy(&SRAM[SRAMAddr - SRAMBase], SRAMWriteBuffer, 0x800);
                SRAMFileDirty = true;
            }

            SRAMAddr = 0;
            SRAMWritePos = 0;
        }
        SRAMStatus &= ~(1<<4);
        return 0;

    case 0x84: // discard write buffer
        SRAMAddr = 0;
        SRAMWritePos = 0;
        return 0;

    case 0x85: // write enable
        if (SRAMWindow)
        {
            SRAMStatus |= (1<<4);
            SRAMWritePos = 0;
        }
        return 0;

    case 0x8B: // revert to ROM read mode
        SRAMWindow = 0;
        return 0;

    case 0x94: // return ID data
        {
            // TODO: check what the data really is. probably the NAND chip's ID.
            // also, might be different between different games or even between different carts.
            // this was taken from a Jam with the Band cart.
            u8 iddata[0x30] =
            {
                0xEC, 0xF1, 0x00, 0x95, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            };

            if (SRAMLength) memcpy(&iddata[0x18], &SRAM[SRAMLength - 0x800], 16);

            memset(data, 0, len);
            memcpy(data, iddata, std::min(len, 0x30u));
        }
        return 0;

    case 0xB2: // set window for accessing SRAM
        {
            u32 addr = (cmd[1]<<24) | ((cmd[2]&0xFE)<<16);

            // window is 0x20000 bytes, address is aligned to that boundary
            // NAND remains stuck 'busy' forever if this is less than the starting SRAM address
            // TODO.

            SRAMWindow = addr;
        }
        return 0;

    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];

            if (SRAMWindow == 0)
            {
                // regular ROM mode
                memset(data, 0, len);

                if (((addr + len - 1) >> 12) != (addr >> 12))
                {
                    u32 len1 = 0x1000 - (addr & 0xFFF);
                    ReadROM_B7(addr, len1, data, 0);
                    ReadROM_B7(addr+len1, len-len1, data, len1);
                }
                else
                    ReadROM_B7(addr, len, data, 0);
            }
            else
            {
                // SRAM mode
                memset(data, 0xFF, len);

                if (SRAMWindow >= SRAMBase && SRAMWindow < (SRAMBase+SRAMLength) &&
                    addr >= SRAMWindow && addr < (SRAMWindow+0x20000))
                {
                    memcpy(data, &SRAM[addr - SRAMBase], len);
                }
            }
        }
        return 0;

    case 0xD6: // read NAND status
        {
            // status bits
            // bit5: ready
            // bit4: write enable

            for (u32 i = 0; i < len; i+=4)
                *(u32*)&data[i] = SRAMStatus * 0x01010101;
        }
        return 0;

    default:
        return CartRetail::ROMCommandStart(cmd, data, len);
    }
}

void CartRetailNAND::ROMCommandFinish(u8* cmd, u8* data, u32 len)
{
    switch (cmd[0])
    {
    case 0x81: // write data
        if (SRAMAddr)
        {
            if ((SRAMWritePos + len) > 0x800)
                len = 0x800 - SRAMWritePos;

            memcpy(&SRAMWriteBuffer[SRAMWritePos], data, len);
            SRAMWritePos += len;
        }
        return;

    default:
        return CartCommon::ROMCommandFinish(cmd, data, len);
    }
}

u8 CartRetailNAND::SPIWrite(u8 val, u32 pos, bool last)
{
    return 0xFF;
}


//


CartHomebrew::CartHomebrew(u8* rom, u32 len, u32 chipid) : CartCommon(rom, len, chipid)
{
    if (Config::DLDIEnable)
    {
        ApplyDLDIPatch(melonDLDI, sizeof(melonDLDI));
        SDFile = Platform::OpenLocalFile(Config::DLDISDPath, "r+b");
    }
    else
        SDFile = nullptr;
}

CartHomebrew::~CartHomebrew()
{
    if (SDFile) fclose(SDFile);
}

void CartHomebrew::Reset()
{
    if (SDFile) fclose(SDFile);

    if (Config::DLDIEnable)
        SDFile = Platform::OpenLocalFile(Config::DLDISDPath, "r+b");
    else
        SDFile = nullptr;
}

void CartHomebrew::DoSavestate(Savestate* file)
{
    // TODO?
}

int CartHomebrew::ROMCommandStart(u8* cmd, u8* data, u32 len)
{
    switch (cmd[0])
    {
    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            memset(data, 0, len);

            if (((addr + len - 1) >> 12) != (addr >> 12))
            {
                u32 len1 = 0x1000 - (addr & 0xFFF);
                ReadROM_B7(addr, len1, data, 0);
                ReadROM_B7(addr+len1, len-len1, data, len1);
            }
            else
                ReadROM_B7(addr, len, data, 0);
        }
        return 0;

    case 0xC0: // SD read
        {
            u32 sector = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            u64 addr = sector * 0x200ULL;

            if (SDFile)
            {
                fseek(SDFile, addr, SEEK_SET);
                fread(data, len, 1, SDFile);
            }
        }
        return 0;

    case 0xC1: // SD write
        return 1;

    default:
        return CartCommon::ROMCommandStart(cmd, data, len);
    }
}

void CartHomebrew::ROMCommandFinish(u8* cmd, u8* data, u32 len)
{
    // TODO: delayed SD writing? like we have for SRAM

    switch (cmd[0])
    {
    case 0xC1:
        {
            u32 sector = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            u64 addr = sector * 0x200ULL;

            if (SDFile)
            {
                fseek(SDFile, addr, SEEK_SET);
                fwrite(data, len, 1, SDFile);
            }
        }
        break;

    default:
        return CartCommon::ROMCommandFinish(cmd, data, len);
    }
}

void CartHomebrew::ApplyDLDIPatch(const u8* patch, u32 patchlen)
{
    u32 offset = *(u32*)&ROM[0x20];
    u32 size = *(u32*)&ROM[0x2C];

    u8* binary = &ROM[offset];
    u32 dldioffset = 0;

    for (u32 i = 0; i < size; i++)
    {
        if (*(u32*)&binary[i  ] == 0xBF8DA5ED &&
            *(u32*)&binary[i+4] == 0x69684320 &&
            *(u32*)&binary[i+8] == 0x006D6873)
        {
            dldioffset = i;
            break;
        }
    }

    if (!dldioffset)
    {
        return;
    }

    printf("DLDI structure found at %08X (%08X)\n", dldioffset, offset+dldioffset);

    if (*(u32*)&patch[0] != 0xBF8DA5ED ||
        *(u32*)&patch[4] != 0x69684320 ||
        *(u32*)&patch[8] != 0x006D6873)
    {
        printf("bad DLDI patch\n");
        return;
    }

    if (patch[0x0D] > binary[dldioffset+0x0F])
    {
        printf("DLDI driver ain't gonna fit, sorry\n");
        return;
    }

    printf("existing driver is: %s\n", &binary[dldioffset+0x10]);
    printf("new driver is: %s\n", &patch[0x10]);

    u32 memaddr = *(u32*)&binary[dldioffset+0x40];
    if (memaddr == 0)
        memaddr = *(u32*)&binary[dldioffset+0x68] - 0x80;

    u32 patchbase = *(u32*)&patch[0x40];
    u32 delta = memaddr - patchbase;

    u32 patchsize = 1 << patch[0x0D];
    u32 patchend = patchbase + patchsize;

    memcpy(&binary[dldioffset], patch, patchlen);

    *(u32*)&binary[dldioffset+0x40] += delta;
    *(u32*)&binary[dldioffset+0x44] += delta;
    *(u32*)&binary[dldioffset+0x48] += delta;
    *(u32*)&binary[dldioffset+0x4C] += delta;
    *(u32*)&binary[dldioffset+0x50] += delta;
    *(u32*)&binary[dldioffset+0x54] += delta;
    *(u32*)&binary[dldioffset+0x58] += delta;
    *(u32*)&binary[dldioffset+0x5C] += delta;

    *(u32*)&binary[dldioffset+0x68] += delta;
    *(u32*)&binary[dldioffset+0x6C] += delta;
    *(u32*)&binary[dldioffset+0x70] += delta;
    *(u32*)&binary[dldioffset+0x74] += delta;
    *(u32*)&binary[dldioffset+0x78] += delta;
    *(u32*)&binary[dldioffset+0x7C] += delta;

    u8 fixmask = patch[0x0E];

    if (fixmask & 0x01)
    {
        u32 fixstart = *(u32*)&patch[0x40] - patchbase;
        u32 fixend = *(u32*)&patch[0x44] - patchbase;

        for (u32 addr = fixstart; addr < fixend; addr+=4)
        {
            u32 val = *(u32*)&binary[dldioffset+addr];
            if (val >= patchbase && val < patchend)
                *(u32*)&binary[dldioffset+addr] += delta;
        }
    }
    if (fixmask & 0x02)
    {
        u32 fixstart = *(u32*)&patch[0x48] - patchbase;
        u32 fixend = *(u32*)&patch[0x4C] - patchbase;

        for (u32 addr = fixstart; addr < fixend; addr+=4)
        {
            u32 val = *(u32*)&binary[dldioffset+addr];
            if (val >= patchbase && val < patchend)
                *(u32*)&binary[dldioffset+addr] += delta;
        }
    }
    if (fixmask & 0x04)
    {
        u32 fixstart = *(u32*)&patch[0x50] - patchbase;
        u32 fixend = *(u32*)&patch[0x54] - patchbase;

        for (u32 addr = fixstart; addr < fixend; addr+=4)
        {
            u32 val = *(u32*)&binary[dldioffset+addr];
            if (val >= patchbase && val < patchend)
                *(u32*)&binary[dldioffset+addr] += delta;
        }
    }
    if (fixmask & 0x08)
    {
        u32 fixstart = *(u32*)&patch[0x58] - patchbase;
        u32 fixend = *(u32*)&patch[0x5C] - patchbase;

        memset(&binary[dldioffset+fixstart], 0, fixend-fixstart);
    }

    printf("applied DLDI patch\n");
}

void CartHomebrew::ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset)
{
    // TODO: how strict should this be for homebrew?

    addr &= (ROMLength-1);

    memcpy(data+offset, ROM+addr, len);
}


/*void ROMCommand_Retail(u8* cmd);
void ROMCommand_RetailNAND(u8* cmd);
void ROMCommand_Homebrew(u8* cmd);

void (*ROMCommandHandler)(u8* cmd);*/


bool Init()
{
    if (!NDSCart_SRAM::Init()) return false;

    CartROM = nullptr;
    Cart = nullptr;

    return true;
}

void DeInit()
{
    if (CartROM) delete[] CartROM;
    if (Cart) delete Cart;

    NDSCart_SRAM::DeInit();
}

void Reset()
{
    CartInserted = false;
    if (CartROM) delete[] CartROM;
    CartROM = nullptr;
    CartROMSize = 0;
    CartID = 0;
    CartIsHomebrew = false;
    CartIsDSi = false;

    if (Cart) delete Cart;
    Cart = nullptr;

    NDSCart_SRAM::Reset();

    ResetCart();
}

void DoSavestate(Savestate* file)
{
    file->Section("NDSC");

    file->Var16(&SPICnt);
    file->Var32(&ROMCnt);

    file->VarArray(ROMCommand, 8);
    file->Var32(&ROMData);

    file->VarArray(TransferData, 0x4000);
    file->Var32(&TransferPos);
    file->Var32(&TransferLen);
    file->Var32(&TransferDir);
    file->VarArray(TransferCmd, 8);

    // cart inserted/len/ROM/etc should be already populated
    // savestate should be loaded after the right game is loaded
    // (TODO: system to verify that indeed the right ROM is loaded)
    // (what to CRC? whole ROM? code binaries? latter would be more convenient for ie. romhaxing)

    //file->Var32(&CmdEncMode);
    //file->Var32(&DataEncMode);

    // TODO: check KEY1 shit??

    NDSCart_SRAM::DoSavestate(file);
}


bool ReadROMParams(u32 gamecode, ROMListEntry* params)
{
    u32 len = sizeof(ROMList) / sizeof(ROMListEntry);

    u32 offset = 0;
    u32 chk_size = len >> 1;
    for (;;)
    {
        u32 key = 0;
        ROMListEntry* curentry = &ROMList[offset + chk_size];
        key = curentry->GameCode;

        if (key == gamecode)
        {
            memcpy(params, curentry, sizeof(ROMListEntry));
            return true;
        }
        else
        {
            if (key < gamecode)
            {
                if (chk_size == 0)
                    offset++;
                else
                    offset += chk_size;
            }
            else if (chk_size == 0)
            {
                return false;
            }

            chk_size >>= 1;
        }

        if (offset >= len)
        {
            return false;
        }
    }
}


void DecryptSecureArea(u8* out)
{
    // TODO: source decryption data from different possible sources
    // * original DS-mode ARM7 BIOS has the key data at 0x30
    // * .srl ROMs (VC dumps) have encrypted secure areas but have precomputed
    //   decryption data at 0x1000 (and at the beginning of the DSi region if any)

    u32 gamecode = *(u32*)&CartROM[0x0C];
    u32 arm9base = *(u32*)&CartROM[0x20];

    memcpy(out, &CartROM[arm9base], 0x800);

    Key1_InitKeycode(false, gamecode, 2, 2);
    Key1_Decrypt((u32*)&out[0]);

    Key1_InitKeycode(false, gamecode, 3, 2);
    for (u32 i = 0; i < 0x800; i += 8)
        Key1_Decrypt((u32*)&out[i]);

    if (!strncmp((const char*)out, "encryObj", 8))
    {
        printf("Secure area decryption OK\n");
        *(u32*)&out[0] = 0xE7FFDEFF;
        *(u32*)&out[4] = 0xE7FFDEFF;
    }
    else
    {
        printf("Secure area decryption failed\n");
        for (u32 i = 0; i < 0x800; i += 4)
            *(u32*)&out[i] = 0xE7FFDEFF;
    }
}

bool LoadROMCommon(u32 filelength, const char *sram, bool direct)
{
    u32 gamecode;
    memcpy(&gamecode, CartROM + 0x0C, 4);
    printf("Game code: %c%c%c%c\n", gamecode&0xFF, (gamecode>>8)&0xFF, (gamecode>>16)&0xFF, gamecode>>24);

    u8 unitcode = CartROM[0x12];
    CartIsDSi = (unitcode & 0x02) != 0;

    ROMListEntry romparams;
    if (!ReadROMParams(gamecode, &romparams))
    {
        // set defaults
        printf("ROM entry not found\n");

        romparams.GameCode = gamecode;
        romparams.ROMSize = CartROMSize;
        if (*(u32*)&CartROM[0x20] < 0x4000)
            romparams.SaveMemType = 0; // no saveRAM for homebrew
        else
            romparams.SaveMemType = 2; // assume EEPROM 64k (TODO FIXME)
    }
    else
        printf("ROM entry: %08X %08X\n", romparams.ROMSize, romparams.SaveMemType);

    if (romparams.ROMSize != filelength) printf("!! bad ROM size %d (expected %d) rounded to %d\n", filelength, romparams.ROMSize, CartROMSize);

    // generate a ROM ID
    // note: most games don't check the actual value
    // it just has to stay the same throughout gameplay
    CartID = 0x000000C2;

    if (CartROMSize >= 1024*1024 && CartROMSize <= 128*1024*1024)
        CartID |= ((CartROMSize >> 20) - 1) << 8;
    else
        CartID |= (0x100 - (CartROMSize >> 28)) << 8;

    if (romparams.SaveMemType == 8)
        CartID |= 0x08000000; // NAND flag

    if (CartIsDSi)
        CartID |= 0x40000000;

    // cart ID for Jam with the Band
    // TODO: this kind of ID triggers different KEY1 phase
    // (repeats commands a bunch of times)
    //CartID = 0x88017FEC;

    printf("Cart ID: %08X\n", CartID);

    u32 arm9base = *(u32*)&CartROM[0x20];

    if (arm9base < 0x8000)
    {
        if (arm9base >= 0x4000)
        {
            // reencrypt secure area if needed
            if (*(u32*)&CartROM[arm9base] == 0xE7FFDEFF && *(u32*)&CartROM[arm9base+0x10] != 0xE7FFDEFF)
            {
                printf("Re-encrypting cart secure area\n");

                strncpy((char*)&CartROM[arm9base], "encryObj", 8);

                Key1_InitKeycode(false, gamecode, 3, 2);
                for (u32 i = 0; i < 0x800; i += 8)
                    Key1_Encrypt((u32*)&CartROM[arm9base + i]);

                Key1_InitKeycode(false, gamecode, 2, 2);
                Key1_Encrypt((u32*)&CartROM[arm9base]);
            }
        }
    }

    if ((arm9base < 0x4000) || (gamecode == 0x23232323))
    {
        CartIsHomebrew = true;
        //if (Config::DLDIEnable)
        //    ApplyDLDIPatch(melonDLDI, sizeof(melonDLDI));
    }

    CartInserted = true;

    // TODO: support more fancy cart types (homebrew?, flashcarts, etc)
    /*if (CartIsHomebrew)
        ROMCommandHandler = ROMCommand_Homebrew;
    else if (CartID & 0x08000000)
        ROMCommandHandler = ROMCommand_RetailNAND;
    else
        ROMCommandHandler = ROMCommand_Retail;*/
    // TODO: add case for pokémon typing game
    if (CartIsHomebrew)
        Cart = new CartHomebrew(CartROM, CartROMSize, CartID);
    else if (CartID & 0x08000000)
        Cart = new CartRetailNAND(CartROM, CartROMSize, CartID);
    //else if (CartID & 0x00010000)
    //    Cart = new CartRetailIR(CartROM, CartROMSize, CartID);
    else
        Cart = new CartRetail(CartROM, CartROMSize, CartID);

    if (Cart)
    {
        Cart->Reset();
        if (direct)
        {
            NDS::SetupDirectBoot();
            Cart->SetupDirectBoot();
        }
    }

    // encryption
    Key1_InitKeycode(false, gamecode, 2, 2);

    // save
    printf("Save file: %s\n", sram);
    //NDSCart_SRAM::LoadSave(sram, romparams.SaveMemType);
    if (Cart) Cart->LoadSave(sram, romparams.SaveMemType);

    /*if (CartIsHomebrew && Config::DLDIEnable)
    {
        CartSD = Platform::OpenLocalFile(Config::DLDISDPath, "r+b");
    }
    else
        CartSD = NULL;*/

    return true;
}

bool LoadROM(const char* path, const char* sram, bool direct)
{
    // TODO: streaming mode? for really big ROMs or systems with limited RAM
    // for now we're lazy
    // also TODO: validate what we're loading!!

    FILE* f = Platform::OpenFile(path, "rb");
    if (!f)
    {
        return false;
    }

    NDS::Reset();

    fseek(f, 0, SEEK_END);
    u32 len = (u32)ftell(f);

    CartROMSize = 0x200;
    while (CartROMSize < len)
        CartROMSize <<= 1;

    CartROM = new u8[CartROMSize];
    memset(CartROM, 0, CartROMSize);
    fseek(f, 0, SEEK_SET);
    fread(CartROM, 1, len, f);

    fclose(f);

    return LoadROMCommon(len, sram, direct);
}

bool LoadROM(const u8* romdata, u32 filelength, const char *sram, bool direct)
{
    NDS::Reset();

    u32 len = filelength;
    CartROMSize = 0x200;
    while (CartROMSize < len)
        CartROMSize <<= 1;

    CartROM = new u8[CartROMSize];
    memset(CartROM, 0, CartROMSize);
    memcpy(CartROM, romdata, filelength);

    return LoadROMCommon(filelength, sram, direct);
}

void RelocateSave(const char* path, bool write)
{
    // herp derp
    //NDSCart_SRAM::RelocateSave(path, write);
    if (Cart) Cart->RelocateSave(path, write);
}

void FlushSRAMFile()
{
    //NDSCart_SRAM::FlushSRAMFile();
    if (Cart) Cart->FlushSRAMFile();
}

int ImportSRAM(const u8* data, u32 length)
{
    memcpy(NDSCart_SRAM::SRAM, data, std::min(length, NDSCart_SRAM::SRAMLength));
    FILE* f = Platform::OpenFile(NDSCart_SRAM::SRAMPath, "wb");
    if (f)
    {
        fwrite(NDSCart_SRAM::SRAM, NDSCart_SRAM::SRAMLength, 1, f);
        fclose(f);
    }

    return length - NDSCart_SRAM::SRAMLength;
}

void ResetCart()
{
    // CHECKME: what if there is a transfer in progress?

    SPICnt = 0;
    ROMCnt = 0;

    SPIData = 0;
    SPIDataPos = 0;
    SPIHold = false;

    memset(ROMCommand, 0, 8);
    ROMData = 0;

    Key2_X = 0;
    Key2_Y = 0;

    memset(TransferData, 0, 0x4000);
    TransferPos = 0;
    TransferLen = 0;
    TransferDir = 0;
    memset(TransferCmd, 0, 8);
    TransferCmd[0] = 0xFF;

    //CmdEncMode = 0;
    //DataEncMode = 0;

    if (Cart) Cart->Reset();
}

/*void ReadROM(u32 addr, u32 len, u32 offset)
{
    if (!CartInserted) return;

    if (addr >= CartROMSize) return;
    if ((addr+len) > CartROMSize)
        len = CartROMSize - addr;

    memcpy(TransferData+offset, CartROM+addr, len);
}

void ReadROM_B7(u32 addr, u32 len, u32 offset)
{
    if (!CartInserted) return;

    addr &= (CartROMSize-1);
    if (!CartIsHomebrew)
    {
        if (addr < 0x8000)
            addr = 0x8000 + (addr & 0x1FF);
    }

    memcpy(TransferData+offset, CartROM+addr, len);
}*/


void ROMEndTransfer(u32 param)
{
    ROMCnt &= ~(1<<31);

    if (SPICnt & (1<<14))
        NDS::SetIRQ((NDS::ExMemCnt[0]>>11)&0x1, NDS::IRQ_CartXferDone);

    /*if (TransferDir == 1)
    {
        // finish a write

        u8* cmd = TransferCmd;
        switch (cmd[0])
        {
        case 0xC1:
            {
                u32 sector = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
                u64 addr = sector * 0x200ULL;

                if (CartSD)
                {
                    fseek(CartSD, addr, SEEK_SET);
                    fwrite(TransferData, TransferLen, 1, CartSD);
                }
            }
            break;
        }
    }*/
    if (Cart) Cart->ROMCommandFinish(TransferCmd, TransferData, TransferLen);
}

void ROMPrepareData(u32 param)
{
    if (TransferDir == 0)
    {
        if (TransferPos >= TransferLen)
            ROMData = 0;
        else
            ROMData = *(u32*)&TransferData[TransferPos];

        TransferPos += 4;
    }

    ROMCnt |= (1<<23);

    if (NDS::ExMemCnt[0] & (1<<11))
        NDS::CheckDMAs(1, 0x12);
    else
        NDS::CheckDMAs(0, 0x05);
}


/*void ROMCommand_Retail(u8* cmd)
{
    switch (cmd[0])
    {
    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            memset(TransferData, 0, TransferLen);

            if (((addr + TransferLen - 1) >> 12) != (addr >> 12))
            {
                u32 len1 = 0x1000 - (addr & 0xFFF);
                ReadROM_B7(addr, len1, 0);
                ReadROM_B7(addr+len1, TransferLen-len1, len1);
            }
            else
                ReadROM_B7(addr, TransferLen, 0);
        }
        break;

    default:
        printf("unknown retail cart command %02X\n", cmd[0]);
        break;
    }
}

void ROMCommand_RetailNAND(u8* cmd)
{
    switch (cmd[0])
    {
    case 0x94: // NAND init
        {
            // initial value: should have bit7 clear
            NDSCart_SRAM::StatusReg = 0;

            // Jam with the Band stores words 6-9 of this at 0x02131BB0
            // it doesn't seem to use those anywhere later
            for (u32 pos = 0; pos < TransferLen; pos += 4)
                *(u32*)&TransferData[pos] = 0;
        }
        break;

    case 0xB2: // set savemem addr
        {
            NDSCart_SRAM::StatusReg |= 0x20;
        }
        break;

    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            memset(TransferData, 0, TransferLen);

            if (((addr + TransferLen - 1) >> 12) != (addr >> 12))
            {
                u32 len1 = 0x1000 - (addr & 0xFFF);
                ReadROM_B7(addr, len1, 0);
                ReadROM_B7(addr+len1, TransferLen-len1, len1);
            }
            else
                ReadROM_B7(addr, TransferLen, 0);
        }
        break;

    case 0xD6: // NAND status
        {
            // status reg bits:
            // * bit7: busy? error?
            // * bit5: accessing savemem

            for (u32 pos = 0; pos < TransferLen; pos += 4)
                *(u32*)&TransferData[pos] = NDSCart_SRAM::StatusReg * 0x01010101;
        }
        break;

    default:
        printf("unknown NAND command %02X %04Xn", cmd[0], TransferLen);
        break;
    }
}

void ROMCommand_Homebrew(u8* cmd)
{
    switch (cmd[0])
    {
    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            memset(TransferData, 0, TransferLen);

            if (((addr + TransferLen - 1) >> 12) != (addr >> 12))
            {
                u32 len1 = 0x1000 - (addr & 0xFFF);
                ReadROM_B7(addr, len1, 0);
                ReadROM_B7(addr+len1, TransferLen-len1, len1);
            }
            else
                ReadROM_B7(addr, TransferLen, 0);
        }
        break;

    case 0xC0: // SD read
        {
            u32 sector = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            u64 addr = sector * 0x200ULL;

            if (CartSD)
            {
                fseek(CartSD, addr, SEEK_SET);
                fread(TransferData, TransferLen, 1, CartSD);
            }
        }
        break;

    case 0xC1: // SD write
        {
            TransferDir = 1;
            memcpy(TransferCmd, cmd, 8);
        }
        break;

    default:
        printf("unknown homebrew cart command %02X\n", cmd[0]);
        break;
    }
}*/


void WriteROMCnt(u32 val)
{
    ROMCnt = (val & 0xFF7F7FFF) | (ROMCnt & 0x00800000);

    if (!(SPICnt & (1<<15))) return;

    // all this junk would only really be useful if melonDS was interfaced to
    // a DS cart reader
    if (val & (1<<15))
    {
        u32 snum = (NDS::ExMemCnt[0]>>8)&0x8;
        u64 seed0 = *(u32*)&NDS::ROMSeed0[snum] | ((u64)NDS::ROMSeed0[snum+4] << 32);
        u64 seed1 = *(u32*)&NDS::ROMSeed1[snum] | ((u64)NDS::ROMSeed1[snum+4] << 32);

        Key2_X = 0;
        Key2_Y = 0;
        for (u32 i = 0; i < 39; i++)
        {
            if (seed0 & (1ULL << i)) Key2_X |= (1ULL << (38-i));
            if (seed1 & (1ULL << i)) Key2_Y |= (1ULL << (38-i));
        }

        printf("seed0: %02X%08X\n", (u32)(seed0>>32), (u32)seed0);
        printf("seed1: %02X%08X\n", (u32)(seed1>>32), (u32)seed1);
        printf("key2 X: %02X%08X\n", (u32)(Key2_X>>32), (u32)Key2_X);
        printf("key2 Y: %02X%08X\n", (u32)(Key2_Y>>32), (u32)Key2_Y);
    }

    if (!(ROMCnt & (1<<31))) return;

    u32 datasize = (ROMCnt >> 24) & 0x7;
    if (datasize == 7)
        datasize = 4;
    else if (datasize > 0)
        datasize = 0x100 << datasize;

    TransferPos = 0;
    TransferLen = datasize;

    *(u32*)&TransferCmd[0] = *(u32*)&ROMCommand[0];
    *(u32*)&TransferCmd[4] = *(u32*)&ROMCommand[4];

    // handle KEY1 encryption as needed.
    // KEY2 encryption is implemented in hardware and doesn't need to be handled.
    /*if (CmdEncMode == 1 || CmdEncMode == 11)
    {
        *(u32*)&TransferCmd[0] = ByteSwap(*(u32*)&ROMCommand[4]);
        *(u32*)&TransferCmd[4] = ByteSwap(*(u32*)&ROMCommand[0]);
        Key1_Decrypt((u32*)TransferCmd);
        u32 tmp = ByteSwap(*(u32*)&TransferCmd[4]);
        *(u32*)&TransferCmd[4] = ByteSwap(*(u32*)&TransferCmd[0]);
        *(u32*)&TransferCmd[0] = tmp;
    }
    else
    {

    }

    printf("ROM COMMAND %04X %08X %02X%02X%02X%02X%02X%02X%02X%02X SIZE %04X\n",
           SPICnt, ROMCnt,
           TransferCmd[0], TransferCmd[1], TransferCmd[2], TransferCmd[3],
           TransferCmd[4], TransferCmd[5], TransferCmd[6], TransferCmd[7],
           datasize);*/

    // default is read
    // commands that do writes will change this
    TransferDir = 0;

    /*switch (cmd[0])
    {
    case 0x9F:
        memset(TransferData, 0xFF, TransferLen);
        break;

    case 0x00:
        memset(TransferData, 0, TransferLen);
        if (TransferLen > 0x1000)
        {
            ReadROM(0, 0x1000, 0);
            for (u32 pos = 0x1000; pos < TransferLen; pos += 0x1000)
                memcpy(TransferData+pos, TransferData, 0x1000);
        }
        else
            ReadROM(0, TransferLen, 0);
        break;

    case 0x90:
    case 0xB8:
        for (u32 pos = 0; pos < TransferLen; pos += 4)
            *(u32*)&TransferData[pos] = CartID;
        break;

    case 0x3C:
        if (CartInserted)
        {
            CmdEncMode = 1;
            Key1_InitKeycode(false, *(u32*)&CartROM[0xC], 2, 2);
        }
        break;

    case 0x3D:
        if (CartInserted && CartIsDSi)
        {
            CmdEncMode = 11;
            Key1_InitKeycode(true, *(u32*)&CartROM[0xC], 1, 2);
        }
        break;

    default:
        if (CmdEncMode == 1 || CmdEncMode == 11)
        {
            switch (cmd[0] & 0xF0)
            {
            case 0x40:
                DataEncMode = 2;
                break;

            case 0x10:
                for (u32 pos = 0; pos < TransferLen; pos += 4)
                    *(u32*)&TransferData[pos] = CartID;
                break;

            case 0x20:
                {
                    u32 addr = (cmd[2] & 0xF0) << 8;
                    if (CmdEncMode == 11)
                    {
                        u32 arm9i_base = *(u32*)&CartROM[0x1C0];
                        addr -= 0x4000;
                        addr += arm9i_base;
                    }
                    ReadROM(addr, 0x1000, 0);
                }
                break;

            case 0xA0:
                CmdEncMode = 2;
                break;
            }
        }
        else if (ROMCommandHandler)
            ROMCommandHandler(cmd);
        break;
    }*/
    // TODO: how should we detect that the transfer should be a write?
    // you're supposed to set bit30 of ROMCNT for a write, but it's also
    // possible to do reads just fine when that bit is set
    if (Cart) TransferDir = Cart->ROMCommandStart(TransferCmd, TransferData, TransferLen);

    ROMCnt &= ~(1<<23);

    // ROM transfer timings
    // the bus is parallel with 8 bits
    // thus a command would take 8 cycles to be transferred
    // and it would take 4 cycles to receive a word of data
    // TODO: advance read position if bit28 is set

    u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
    u32 cmddelay = 8;

    // delays are only applied when the WR bit is cleared
    // CHECKME: do the delays apply at the end (instead of start) when WR is set?
    if (!(ROMCnt & (1<<30)))
    {
        cmddelay += (ROMCnt & 0x1FFF);
        if (datasize) cmddelay += ((ROMCnt >> 16) & 0x3F);
    }

    if (datasize == 0)
        NDS::ScheduleEvent(NDS::Event_ROMTransfer, false, xfercycle*cmddelay, ROMEndTransfer, 0);
    else
        NDS::ScheduleEvent(NDS::Event_ROMTransfer, false, xfercycle*(cmddelay+4), ROMPrepareData, 0);
}

void AdvanceROMTransfer()
{
    ROMCnt &= ~(1<<23);

    if (TransferPos < TransferLen)
    {
        u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
        u32 delay = 4;
        if (!(ROMCnt & (1<<30)))
        {
            if (!(TransferPos & 0x1FF))
                delay += ((ROMCnt >> 16) & 0x3F);
        }

        NDS::ScheduleEvent(NDS::Event_ROMTransfer, false, xfercycle*delay, ROMPrepareData, 0);
    }
    else
        ROMEndTransfer(0);
}

u32 ReadROMData()
{
    if (ROMCnt & (1<<23))
    {
        AdvanceROMTransfer();
    }

    return ROMData;
}

void WriteROMData(u32 val)
{
    ROMData = val;

    if (ROMCnt & (1<<23))
    {
        if (TransferDir == 1)
        {
            if (TransferPos < TransferLen)
                *(u32*)&TransferData[TransferPos] = ROMData;

            TransferPos += 4;
        }

        AdvanceROMTransfer();
    }
}


void WriteSPICnt(u16 val)
{
    SPICnt = (SPICnt & 0x0080) | (val & 0xE043);
    if (SPICnt & (1<<7))
        printf("!! CHANGING AUXSPICNT DURING TRANSFER: %04X\n", val);
}

void SPITransferDone(u32 param)
{
    SPICnt &= ~(1<<7);
}

u8 ReadSPIData()
{
    if (!(SPICnt & (1<<15))) return 0;
    if (!(SPICnt & (1<<13))) return 0;
    if (SPICnt & (1<<7)) return 0; // checkme

    //return NDSCart_SRAM::Read();
    return SPIData;
}

void WriteSPIData(u8 val)
{
    if (!(SPICnt & (1<<15))) return;
    if (!(SPICnt & (1<<13))) return;

    if (SPICnt & (1<<7)) printf("!! WRITING AUXSPIDATA DURING PENDING TRANSFER\n");

    SPICnt |= (1<<7);
    //NDSCart_SRAM::Write(val, SPICnt&(1<<6));

    bool hold = SPICnt&(1<<6);
    bool islast = false;
    if (!hold)
    {
        if (SPIHold) SPIDataPos++;
        else         SPIDataPos = 0;
        islast = true;
        SPIHold = false;
    }
    else if (hold && (!SPIHold))
    {
        SPIHold = true;
        SPIDataPos = 0;
    }
    else
    {
        SPIDataPos++;
    }

    if (Cart) SPIData = Cart->SPIWrite(val, SPIDataPos, islast);
    else      SPIData = val; // checkme

    // SPI transfers one bit per cycle -> 8 cycles per byte
    u32 delay = 8 * (8 << (SPICnt & 0x3));
    NDS::ScheduleEvent(NDS::Event_ROMSPITransfer, false, delay, SPITransferDone, 0);
}

}
