/*
    Copyright 2016-2017 Arisotura

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
#include "CRC32.h"
#include "DSi_AES.h"
#include "Platform.h"
#include "Config.h"
#include "ROMList.h"
#include "melonDLDI.h"


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
    if (!SRAMFileDirty)
        return;

    SRAMFileDirty = false;

    FILE* f = Platform::OpenFile(SRAMPath, "wb");
    if (f)
    {
        fwrite(SRAM, SRAMLength, 1, f);
        fclose(f);
    }
}

}


namespace NDSCart
{

u16 SPICnt;
u32 ROMCnt;

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
u32 CartCRC;
u32 CartID;
bool CartIsHomebrew;
bool CartIsDSi;

FILE* CartSD;

u32 CmdEncMode;
u32 DataEncMode;

u32 Key1_KeyBuf[0x412];

u64 Key2_X;
u64 Key2_Y;


void ROMCommand_Retail(u8* cmd);
void ROMCommand_RetailNAND(u8* cmd);
void ROMCommand_Homebrew(u8* cmd);

void (*ROMCommandHandler)(u8* cmd);


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


bool Init()
{
    if (!NDSCart_SRAM::Init()) return false;

    CartROM = NULL;

    CartSD = NULL;

    return true;
}

void DeInit()
{
    if (CartROM) delete[] CartROM;

    if (CartSD) fclose(CartSD);

    NDSCart_SRAM::DeInit();
}

void Reset()
{
    CartInserted = false;
    if (CartROM) delete[] CartROM;
    CartROM = NULL;
    CartROMSize = 0;
    CartID = 0;
    CartIsHomebrew = false;
    CartIsDSi = false;

    if (CartSD) fclose(CartSD);
    CartSD = NULL;

    ROMCommandHandler = NULL;

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

    file->Var32(&CmdEncMode);
    file->Var32(&DataEncMode);

    // TODO: check KEY1 shit??

    NDSCart_SRAM::DoSavestate(file);
}


void ApplyDLDIPatch(const u8* patch, u32 len)
{
    u32 offset = *(u32*)&CartROM[0x20];
    u32 size = *(u32*)&CartROM[0x2C];

    u8* binary = &CartROM[offset];
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
        delete[] patch;
        return;
    }

    if (patch[0x0D] > binary[dldioffset+0x0F])
    {
        printf("DLDI driver ain't gonna fit, sorry\n");
        delete[] patch;
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

    memcpy(&binary[dldioffset], patch, len);

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

    u32 gamecode;
    fseek(f, 0x0C, SEEK_SET);
    fread(&gamecode, 4, 1, f);
    printf("Game code: %c%c%c%c\n", gamecode&0xFF, (gamecode>>8)&0xFF, (gamecode>>16)&0xFF, gamecode>>24);

    u8 unitcode;
    fseek(f, 0x12, SEEK_SET);
    fread(&unitcode, 1, 1, f);
    CartIsDSi = (unitcode & 0x02) != 0;

    CartROM = new u8[CartROMSize];
    memset(CartROM, 0, CartROMSize);
    fseek(f, 0, SEEK_SET);
    fread(CartROM, 1, len, f);

    fclose(f);
    //CartROM = f;

    CartCRC = CRC32(CartROM, CartROMSize);
    printf("ROM CRC32: %08X\n", CartCRC);

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

    if (romparams.ROMSize != len) printf("!! bad ROM size %d (expected %d) rounded to %d\n", len, romparams.ROMSize, CartROMSize);

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
        if (Config::DLDIEnable)
            ApplyDLDIPatch(melonDLDI, sizeof(melonDLDI));
    }

    if (direct)
    {
        // TODO: in the case of an already-encrypted secure area, direct boot
        // needs it decrypted
        NDS::SetupDirectBoot();
        CmdEncMode = 2;
    }

    CartInserted = true;

    // TODO: support more fancy cart types (homebrew?, flashcarts, etc)
    if (CartIsHomebrew)
        ROMCommandHandler = ROMCommand_Homebrew;
    else if (CartID & 0x08000000)
        ROMCommandHandler = ROMCommand_RetailNAND;
    else
        ROMCommandHandler = ROMCommand_Retail;

    // encryption
    Key1_InitKeycode(false, gamecode, 2, 2);

    // save
    printf("Save file: %s\n", sram);
    NDSCart_SRAM::LoadSave(sram, romparams.SaveMemType);

    if (CartIsHomebrew && Config::DLDIEnable)
    {
        CartSD = Platform::OpenLocalFile(Config::DLDISDPath, "r+b");
    }
    else
        CartSD = NULL;

    return true;
}

void RelocateSave(const char* path, bool write)
{
    // herp derp
    NDSCart_SRAM::RelocateSave(path, write);
}

void FlushSRAMFile()
{
    NDSCart_SRAM::FlushSRAMFile();
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

    CmdEncMode = 0;
    DataEncMode = 0;
}

void ReadROM(u32 addr, u32 len, u32 offset)
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
}


void ROMEndTransfer(u32 param)
{
    ROMCnt &= ~(1<<31);

    if (SPICnt & (1<<14))
        NDS::SetIRQ((NDS::ExMemCnt[0]>>11)&0x1, NDS::IRQ_CartSendDone);

    if (TransferDir == 1)
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
    }
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


void ROMCommand_Retail(u8* cmd)
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
}


void WriteROMCnt(u32 val)
{
    ROMCnt = (val & 0xFF7F7FFF) | (ROMCnt & 0x00800000);

    if (!(SPICnt & (1<<15))) return;

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

    // handle KEY1 encryption as needed.
    // KEY2 encryption is implemented in hardware and doesn't need to be handled.
    u8 cmd[8];
    if (CmdEncMode == 1 || CmdEncMode == 11)
    {
        *(u32*)&cmd[0] = ByteSwap(*(u32*)&ROMCommand[4]);
        *(u32*)&cmd[4] = ByteSwap(*(u32*)&ROMCommand[0]);
        Key1_Decrypt((u32*)cmd);
        u32 tmp = ByteSwap(*(u32*)&cmd[4]);
        *(u32*)&cmd[4] = ByteSwap(*(u32*)&cmd[0]);
        *(u32*)&cmd[0] = tmp;
    }
    else
    {
        *(u32*)&cmd[0] = *(u32*)&ROMCommand[0];
        *(u32*)&cmd[4] = *(u32*)&ROMCommand[4];
    }

    /*printf("ROM COMMAND %04X %08X %02X%02X%02X%02X%02X%02X%02X%02X SIZE %04X\n",
           SPICnt, ROMCnt,
           cmd[0], cmd[1], cmd[2], cmd[3],
           cmd[4], cmd[5], cmd[6], cmd[7],
           datasize);*/

    // default is read
    // commands that do writes will change this
    TransferDir = 0;

    switch (cmd[0])
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
    }

    ROMCnt &= ~(1<<23);

    // ROM transfer timings
    // the bus is parallel with 8 bits
    // thus a command would take 8 cycles to be transferred
    // and it would take 4 cycles to receive a word of data
    // TODO: advance read position if bit28 is set

    u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
    u32 cmddelay = 8;

    // delays are only applied when the WR bit is cleared
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

    return NDSCart_SRAM::Read();
}

void WriteSPIData(u8 val)
{
    if (!(SPICnt & (1<<15))) return;
    if (!(SPICnt & (1<<13))) return;

    if (SPICnt & (1<<7)) printf("!! WRITING AUXSPIDATA DURING PENDING TRANSFER\n");

    SPICnt |= (1<<7);
    NDSCart_SRAM::Write(val, SPICnt&(1<<6));

    // SPI transfers one bit per cycle -> 8 cycles per byte
    u32 delay = 8 * (8 << (SPICnt & 0x3));
    NDS::ScheduleEvent(NDS::Event_ROMSPITransfer, false, delay, SPITransferDone, 0);
}

}
