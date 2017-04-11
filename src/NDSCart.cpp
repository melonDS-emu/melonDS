/*
    Copyright 2016-2017 StapleButter

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
#include "NDSCart.h"


namespace NDSCart_SRAM
{

u8* SRAM;
u32 SRAMLength;

char SRAMPath[256];

void (*WriteFunc)(u8 val, bool islast);

u32 Discover_MemoryType;
u32 Discover_Likeliness;
u8* Discover_Buffer;
u32 Discover_DataPos;

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
void Write_Discover(u8 val, bool islast);


bool Init()
{
    SRAM = NULL;
    Discover_Buffer = NULL;
    return true;
}

void DeInit()
{
    if (SRAM) delete[] SRAM;
    if (Discover_Buffer) delete[] Discover_Buffer;
}

void Reset()
{
    if (SRAM) delete[] SRAM;
    if (Discover_Buffer) delete[] Discover_Buffer;
    SRAM = NULL;
    Discover_Buffer = NULL;
}

void LoadSave(char* path)
{
    if (SRAM) delete[] SRAM;
    if (Discover_Buffer) delete[] Discover_Buffer;

    Discover_Buffer = NULL;

    strncpy(SRAMPath, path, 255);
    SRAMPath[255] = '\0';

    FILE* f = fopen(path, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        SRAMLength = (u32)ftell(f);
        SRAM = new u8[SRAMLength];

        fseek(f, 0, SEEK_SET);
        fread(SRAM, SRAMLength, 1, f);

        fclose(f);

        switch (SRAMLength)
        {
        case 512: WriteFunc = Write_EEPROMTiny; break;
        case 8192:
        case 65536: WriteFunc = Write_EEPROM; break;
        case 256*1024:
        case 512*1024:
        case 1024*1024:
        case 8192*1024: WriteFunc = Write_Flash; break;
        default:
            printf("!! BAD SAVE LENGTH %d\n", SRAMLength);
            WriteFunc = Write_Null;
            break;
        }
    }
    else
    {
        SRAMLength = 0;
        WriteFunc = Write_Discover;
        Discover_MemoryType = 2;
        Discover_Likeliness = 0;

        Discover_DataPos = 0;
        Discover_Buffer = new u8[256*1024];
        memset(Discover_Buffer, 0, 256*1024);
    }

    Hold = 0;
    CurCmd = 0;
    Data = 0;
    StatusReg = 0x00;
}

u8 Read()
{
    return Data;
}

void SetMemoryType()
{
    switch (Discover_MemoryType)
    {
    case 1:
        printf("Save memory type: EEPROM 4k\n");
        WriteFunc = Write_EEPROMTiny;
        SRAMLength = 512;
        break;

    case 2:
        printf("Save memory type: EEPROM 64k\n");
        WriteFunc = Write_EEPROM;
        SRAMLength = 8192;
        break;

    case 3:
        printf("Save memory type: EEPROM 512k\n");
        WriteFunc = Write_EEPROM;
        SRAMLength = 65536;
        break;

    case 4:
        printf("Save memory type: Flash. Hope the size is 256K.\n");
        WriteFunc = Write_Flash;
        SRAMLength = 256*1024;
        break;

    case 5:
        printf("Save memory type: ...something else\n");
        WriteFunc = Write_Null;
        SRAMLength = 0;
        break;
    }

    if (!SRAMLength)
        return;

    SRAM = new u8[SRAMLength];

    // replay writes that occured during discovery
    u8 prev_cmd = CurCmd;
    u32 pos = 0;
    while (pos < 256*1024)
    {
        u32 len = *(u32*)&Discover_Buffer[pos];
        pos += 4;
        if (len == 0) break;

        CurCmd = Discover_Buffer[pos++];
        DataPos = 0;
        Addr = 0;
        Data = 0;
        for (u32 i = 1; i < len; i++)
        {
            WriteFunc(Discover_Buffer[pos++], (i==(len-1)));
            DataPos++;
        }
    }

    CurCmd = prev_cmd;

    delete[] Discover_Buffer;
    Discover_Buffer = NULL;
}

void Write_Discover(u8 val, bool islast)
{
    // attempt at autodetecting the type of save memory.
    // we basically hope the game will be nice and clear whole pages of memory.

    if (CurCmd == 0x03 || CurCmd == 0x0B)
    {
        if (Discover_Likeliness)
        {
            // apply. and pray.
            SetMemoryType();

            DataPos = 0;
            Addr = 0;
            Data = 0;
            return WriteFunc(val, islast);
        }
        else
        {
            Data = 0;
            return;
        }
    }

    if (CurCmd == 0x02 || CurCmd == 0x0A)
    {
        if (DataPos == 0)
            Discover_Buffer[Discover_DataPos + 4] = CurCmd;

        Discover_Buffer[Discover_DataPos + 5 + DataPos] = val;

        if (islast)
        {
            u32 len = DataPos+1;

            *(u32*)&Discover_Buffer[Discover_DataPos] = len+1;
            Discover_DataPos += 5+len;

            if (Discover_Likeliness <= len)
            {
                Discover_Likeliness = len;

                if (len > 3+256) // bigger Flash, FRAM, whatever
                {
                    Discover_MemoryType = 5;
                }
                else if (len > 2+128) // Flash
                {
                    Discover_MemoryType = 4;
                }
                else if (len > 2+32) // EEPROM 512k
                {
                    Discover_MemoryType = 3;
                }
                else if (len > 1+16 || (len != 1+16 && CurCmd != 0x0A)) // EEPROM 64k
                {
                    Discover_MemoryType = 2;
                }
                else // EEPROM 4k
                {
                    Discover_MemoryType = 1;
                }
            }

            printf("discover: type=%d likeliness=%d\n", Discover_MemoryType, Discover_Likeliness);
        }
    }
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
    switch (CurCmd)
    {
    case 0x02:
        if (DataPos < 2)
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
        if (DataPos < 2)
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

    case 0x02:
    case 0x03:
    case 0x0A:
    case 0x0B:
    case 0x9F:
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
            printf("unknown save SPI command %02X %08X\n", CurCmd);
        break;
    }

    if (islast && (CurCmd == 0x02 || CurCmd == 0x0A) && (SRAMLength > 0))
    {
        FILE* f = fopen(SRAMPath, "wb");
        if (f)
        {
            fwrite(SRAM, SRAMLength, 1, f);
            fclose(f);
        }
    }
}

}


namespace NDSCart
{

u16 SPICnt;
u32 ROMCnt;

u8 ROMCommand[8];
u32 ROMDataOut;

u8 DataOut[0x4000];
u32 DataOutPos;
u32 DataOutLen;

bool CartInserted;
u8* CartROM;
u32 CartROMSize;
u32 CartID;
bool CartIsHomebrew;

u32 CmdEncMode;
u32 DataEncMode;

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

void Key1_InitKeycode(u32 idcode, u32 level, u32 mod)
{
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


bool Init()
{
    if (!NDSCart_SRAM::Init()) return false;

    CartROM = NULL;

    return true;
}

void DeInit()
{
    if (CartROM) delete[] CartROM;

    NDSCart_SRAM::DeInit();
}

void Reset()
{
    SPICnt = 0;
    ROMCnt = 0;

    memset(ROMCommand, 0, 8);
    ROMDataOut = 0;

    Key2_X = 0;
    Key2_Y = 0;

    memset(DataOut, 0, 0x4000);
    DataOutPos = 0;
    DataOutLen = 0;

    CartInserted = false;
    if (CartROM) delete[] CartROM;
    CartROM = NULL;
    CartROMSize = 0;
    CartID = 0;
    CartIsHomebrew = false;

    CmdEncMode = 0;
    DataEncMode = 0;

    NDSCart_SRAM::Reset();
}


bool LoadROM(const char* path, bool direct)
{
    // TODO: streaming mode? for really big ROMs or systems with limited RAM
    // for now we're lazy

    if (CartROM) delete[] CartROM;

    FILE* f = fopen(path, "rb");
    if (!f)
    {
        printf("Failed to open ROM file %s\n", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    u32 len = (u32)ftell(f);

    CartROMSize = 0x200;
    while (CartROMSize < len)
        CartROMSize <<= 1;

    u32 gamecode;
    fseek(f, 0x0C, SEEK_SET);
    fread(&gamecode, 4, 1, f);

    CartROM = new u8[CartROMSize];
    memset(CartROM, 0, CartROMSize);
    fseek(f, 0, SEEK_SET);
    fread(CartROM, 1, len, f);

    fclose(f);
    //CartROM = f;

    if (direct)
    {
        NDS::SetupDirectBoot();
        CmdEncMode = 2;
    }

    CartInserted = true;

    // generate a ROM ID
    // note: most games don't check the actual value
    // it just has to stay the same throughout gameplay
    CartID = 0x00001FC2;

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

                Key1_InitKeycode(gamecode, 3, 2);
                for (u32 i = 0; i < 0x800; i += 8)
                    Key1_Encrypt((u32*)&CartROM[arm9base + i]);

                Key1_InitKeycode(gamecode, 2, 2);
                Key1_Encrypt((u32*)&CartROM[arm9base]);
            }
        }
        else
            CartIsHomebrew = true;
    }

    // encryption
    Key1_InitKeycode(gamecode, 2, 2);


    // save
    char savepath[256];
    strncpy(savepath, path, 255);
    savepath[255] = '\0';
    strncpy(savepath + strlen(path) - 3, "sav", 3);
    printf("Save file: %s\n", savepath);
    NDSCart_SRAM::LoadSave(savepath);

    return true;
}

void ReadROM(u32 addr, u32 len, u32 offset)
{
    if (!CartInserted) return;

    if (addr >= CartROMSize) return;
    if ((addr+len) > CartROMSize)
        len = CartROMSize - addr;

    memcpy(DataOut+offset, CartROM+addr, len);
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

    memcpy(DataOut+offset, CartROM+addr, len);
}


void ROMEndTransfer(u32 param)
{
    ROMCnt &= ~(1<<31);

    if (SPICnt & (1<<14))
        NDS::SetIRQ((NDS::ExMemCnt[0]>>11)&0x1, NDS::IRQ_CartSendDone);
}

void ROMPrepareData(u32 param)
{
    if (DataOutPos >= DataOutLen)
        ROMDataOut = 0;
    else
        ROMDataOut = *(u32*)&DataOut[DataOutPos];

    DataOutPos += 4;

    ROMCnt |= (1<<23);
    NDS::CheckDMAs(0, 0x05);
    NDS::CheckDMAs(1, 0x12);
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

    DataOutPos = 0;
    DataOutLen = datasize;

    // handle KEY1 encryption as needed.
    // KEY2 encryption is implemented in hardware and doesn't need to be handled.
    u8 cmd[8];
    if (CmdEncMode == 1)
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

    switch (cmd[0])
    {
    case 0x9F:
        memset(DataOut, 0xFF, DataOutLen);
        break;

    case 0x00:
        memset(DataOut, 0, DataOutLen);
        if (DataOutLen > 0x1000)
        {
            ReadROM(0, 0x1000, 0);
            for (u32 pos = 0x1000; pos < DataOutLen; pos += 0x1000)
                memcpy(DataOut+pos, DataOut, 0x1000);
        }
        else
            ReadROM(0, DataOutLen, 0);
        break;

    case 0x90:
    case 0xB8:
        for (u32 pos = 0; pos < DataOutLen; pos += 4)
            *(u32*)&DataOut[pos] = CartID;
        break;

    case 0x3C:
        if (CartInserted) CmdEncMode = 1;
        break;

    case 0xB7:
        {
            u32 addr = (cmd[1]<<24) | (cmd[2]<<16) | (cmd[3]<<8) | cmd[4];
            memset(DataOut, 0, DataOutLen);

            if (((addr + DataOutLen - 1) >> 12) != (addr >> 12))
            {
                u32 len1 = 0x1000 - (addr & 0xFFF);
                ReadROM_B7(addr, len1, 0);
                ReadROM_B7(addr+len1, DataOutLen-len1, len1);
            }
            else
                ReadROM_B7(addr, DataOutLen, 0);
        }
        break;

    default:
        switch (cmd[0] & 0xF0)
        {
        case 0x40:
            DataEncMode = 2;
            break;

        case 0x10:
            for (u32 pos = 0; pos < DataOutLen; pos += 4)
                *(u32*)&DataOut[pos] = CartID;
            break;

        case 0x20:
            {
                u32 addr = (cmd[2] & 0xF0) << 8;
                ReadROM(addr, 0x1000, 0);
            }
            break;

        case 0xA0:
            CmdEncMode = 2;
            break;
        }
        break;
    }

    ROMCnt &= ~(1<<23);

    // ROM transfer timings
    // the bus is parallel with 8 bits
    // thus a command would take 8 cycles to be transferred
    // and it would take 4 cycles to receive a word of data

    u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
    if (datasize == 0)
        NDS::ScheduleEvent(NDS::Event_ROMTransfer, false, xfercycle*8, ROMEndTransfer, 0);
    else
        NDS::ScheduleEvent(NDS::Event_ROMTransfer, true, xfercycle*(8+4), ROMPrepareData, 0);
}

u32 ReadROMData()
{
    if (ROMCnt & (1<<23))
    {
        ROMCnt &= ~(1<<23);

        if (DataOutPos < DataOutLen)
        {
            u32 xfercycle = (ROMCnt & (1<<27)) ? 8 : 5;
            NDS::ScheduleEvent(NDS::Event_ROMTransfer, true, xfercycle*4, ROMPrepareData, 0);
        }
        else
            ROMEndTransfer(0);
    }

    return ROMDataOut;
}


void WriteSPICnt(u16 val)
{
    SPICnt = (SPICnt & 0x0080) | (val & 0xE043);
}

u8 ReadSPIData()
{
    if (!(SPICnt & (1<<15))) return 0;
    if (!(SPICnt & (1<<13))) return 0;

    return NDSCart_SRAM::Read();
}

void WriteSPIData(u8 val)
{
    if (!(SPICnt & (1<<15))) return;
    if (!(SPICnt & (1<<13))) return;

    // TODO: take delays into account

    NDSCart_SRAM::Write(val, SPICnt&(1<<6));
}

}
