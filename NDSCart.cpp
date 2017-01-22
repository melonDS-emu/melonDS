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


void Init()
{
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
    CartROM = NULL;
    CartROMSize = 0;
    CartID = 0;

    CmdEncMode = 0;
    DataEncMode = 0;
}


void LoadROM(char* path)
{
    // TODO: streaming mode? for really big ROMs or systems with limited RAM
    // for now we're lazy

    FILE* f = fopen(path, "rb");

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

    CartInserted = true;

    // generate a ROM ID
    // note: most games don't check the actual value
    // it just has to stay the same throughout gameplay
    CartID = 0x00001FC2;

    // encryption
    Key1_InitKeycode(gamecode, 2, 2);
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
    addr &= (CartROMSize-1);
    if (addr < 0x8000) addr = 0x8000 + (addr & 0x1FF);

    memcpy(DataOut+offset, CartROM+addr, len);
}


void EndTransfer()
{
    ROMCnt &= ~(1<<23);
    ROMCnt &= ~(1<<31);

    if (SPICnt & (1<<14))
        NDS::TriggerIRQ((NDS::ExMemCnt[0]>>11)&0x1, NDS::IRQ_CartSendDone);
}

void ROMPrepareData(u32 param)
{
    if (DataOutPos >= DataOutLen)
        ROMDataOut = 0;
    else
        ROMDataOut = *(u32*)&DataOut[DataOutPos];

    DataOutPos += 4;

    ROMCnt |= (1<<23);
    NDS::CheckDMAs(0, 0x06);
    NDS::CheckDMAs(1, 0x12);

    if (DataOutPos < DataOutLen)
        NDS::ScheduleEvent((ROMCnt & (1<<27)) ? 8:5, ROMPrepareData, 0);
}

void WriteCnt(u32 val)
{
    ROMCnt = val & 0xFF7F7FFF;

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

    printf("ROM COMMAND %04X %08X %02X%02X%02X%02X%02X%02X%02X%02X SIZE %04X\n",
           SPICnt, ROMCnt,
           cmd[0], cmd[1], cmd[2], cmd[3],
           cmd[4], cmd[5], cmd[6], cmd[7],
           datasize);

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
        CmdEncMode = 1;
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

        case 0xA0:
            CmdEncMode = 2;
            break;
        }
        break;
    }

    //ROMCnt &= ~(1<<23);
    ROMCnt |= (1<<23);

    if (datasize == 0)
        EndTransfer();
    else
    {
        NDS::CheckDMAs(0, 0x06);
        NDS::CheckDMAs(1, 0x12);
    }
        //NDS::ScheduleEvent((ROMCnt & (1<<27)) ? 8:5, ROMPrepareData, 0);
}

u32 ReadData()
{
    /*if (ROMCnt & (1<<23))
    {
        ROMCnt &= ~(1<<23);
        if (DataOutPos >= DataOutLen)
            EndTransfer();
    }

    return ROMDataOut;*/
    u32 ret;
    if (DataOutPos >= DataOutLen)
        ret = 0;
    else
        ret = *(u32*)&DataOut[DataOutPos];

    DataOutPos += 4;

    if (DataOutPos == DataOutLen)
        EndTransfer();

    return ret;
}

void DMA(u32 addr)
{
    void (*writefn)(u32,u32) = (NDS::ExMemCnt[0] & (1<<11)) ? NDS::ARM7Write32 : NDS::ARM9Write32;
    for (u32 i = 0; i < DataOutLen; i+=4)
    {
        writefn(addr+i, *(u32*)&DataOut[i]);
    }

    EndTransfer();
}

}
