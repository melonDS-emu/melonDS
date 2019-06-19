/*
    Copyright 2016-2019 Arisotura

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
#include "DSi.h"
#include "DSi_AES.h"
#include "FIFO.h"
#include "tiny-AES-c/aes.hpp"


namespace DSi_AES
{

u32 Cnt;

u32 BlkCnt;
u32 RemBlocks;

u32 InputDMASize, OutputDMASize;
u32 AESMode;

FIFO<u32>* InputFIFO;
FIFO<u32>* OutputFIFO;

u8 IV[16];

u8 KeyNormal[4][16];
u8 KeyX[4][16];
u8 KeyY[4][16];

u8 CurKey[16];

AES_ctx Ctx;


void Swap16(u8* dst, u8* src)
{
    for (int i = 0; i < 16; i++)
        dst[i] = src[15-i];
}

void ROL16(u8* val, u32 n)
{
    u32 n_coarse = n >> 3;
    u32 n_fine = n & 7;
    u8 tmp[16];

    for (u32 i = 0; i < 16; i++)
    {
        tmp[i] = val[(i - n_coarse) & 0xF];
    }

    for (u32 i = 0; i < 16; i++)
    {
        val[i] = (tmp[i] << n_fine) | (tmp[(i - 1) & 0xF] >> (8-n_fine));
    }
}

#define _printhex(str, size) { for (int z = 0; z < (size); z++) printf("%02X", (str)[z]); printf("\n"); }


bool Init()
{
    InputFIFO = new FIFO<u32>(16);
    OutputFIFO = new FIFO<u32>(16);

    const u8 zero[16] = {0};
    AES_init_ctx_iv(&Ctx, zero, zero);

    return true;
}

void DeInit()
{
    delete InputFIFO;
    delete OutputFIFO;
}

void Reset()
{
    Cnt = 0;

    BlkCnt = 0;
    RemBlocks = 0;

    InputDMASize = 0;
    OutputDMASize = 0;
    AESMode = 0;

    InputFIFO->Clear();
    OutputFIFO->Clear();

    memset(KeyNormal, 0, sizeof(KeyNormal));
    memset(KeyX, 0, sizeof(KeyX));
    memset(KeyY, 0, sizeof(KeyY));

    memset(CurKey, 0, sizeof(CurKey));

    // initialize keys, as per GBAtek

    // slot 3: console-unique eMMC crypto
    *(u32*)&KeyX[3][0] = (u32)DSi::ConsoleID;
    *(u32*)&KeyX[3][4] = (u32)DSi::ConsoleID ^ 0x24EE6906;
    *(u32*)&KeyX[3][8] = (u32)(DSi::ConsoleID >> 32) ^ 0xE65B601D;
    *(u32*)&KeyX[3][12] = (u32)(DSi::ConsoleID >> 32);
    *(u32*)&KeyY[3][0] = 0x0AB9DC76;
    *(u32*)&KeyY[3][4] = 0xBD4DC4D3;
    *(u32*)&KeyY[3][8] = 0x202DDD1D;
}


void ProcessBlock_CTR()
{
    u8 data[16];
    u8 data_rev[16];

    *(u32*)&data[0] = InputFIFO->Read();
    *(u32*)&data[4] = InputFIFO->Read();
    *(u32*)&data[8] = InputFIFO->Read();
    *(u32*)&data[12] = InputFIFO->Read();

    //printf("AES-CTR: INPUT: "); _printhex(data, 16);

    Swap16(data_rev, data);
    AES_CTR_xcrypt_buffer(&Ctx, data_rev, 16);
    Swap16(data, data_rev);

    //printf("AES-CTR: OUTPUT: "); _printhex(data, 16);

    OutputFIFO->Write(*(u32*)&data[0]);
    OutputFIFO->Write(*(u32*)&data[4]);
    OutputFIFO->Write(*(u32*)&data[8]);
    OutputFIFO->Write(*(u32*)&data[12]);
}


u32 ReadCnt()
{
    u32 ret = Cnt;

    ret |= InputFIFO->Level();
    ret |= (OutputFIFO->Level() << 5);

    return ret;
}

void WriteCnt(u32 val)
{
    u32 oldcnt = Cnt;
    Cnt = val & 0xFC1FF000;

    if (val & (1<<10)) InputFIFO->Clear();
    if (val & (1<<11)) OutputFIFO->Clear();

    u32 dmasize[4] = {4, 8, 12, 16};
    InputDMASize = dmasize[3 - ((val >> 12) & 0x3)];
    OutputDMASize = dmasize[(val >> 14) & 0x3];

    AESMode = (val >> 28) & 0x3;
    if (AESMode < 2) printf("AES-CCM TODO\n");

    if (val & (1<<24))
    {
        u32 slot = (val >> 26) & 0x3;
        memcpy(CurKey, KeyNormal[slot], 16);

        //printf("AES: key(%d): ", slot); _printhex(CurKey, 16);

        u8 tmp[16];
        Swap16(tmp, CurKey);
        AES_init_ctx(&Ctx, tmp);
    }

    if (!(oldcnt & (1<<31)) && (val & (1<<31)))
    {
        // transfer start (checkme)
        RemBlocks = BlkCnt >> 16;
    }

    printf("AES CNT: %08X / mode=%d inDMA=%d outDMA=%d blocks=%d\n",
           val, AESMode, InputDMASize, OutputDMASize, RemBlocks);
}

void WriteBlkCnt(u32 val)
{
    BlkCnt = val;
}

u32 ReadOutputFIFO()
{
    return OutputFIFO->Read();
}

void WriteInputFIFO(u32 val)
{
    // TODO: add some delay to processing

    InputFIFO->Write(val);

    if (!(Cnt & (1<<31))) return;

    while (InputFIFO->Level() >= 4 && RemBlocks > 0)
    {
        switch (AESMode)
        {
        case 2:
        case 3: ProcessBlock_CTR(); break;
        default:
            // dorp
            OutputFIFO->Write(InputFIFO->Read());
            OutputFIFO->Write(InputFIFO->Read());
            OutputFIFO->Write(InputFIFO->Read());
            OutputFIFO->Write(InputFIFO->Read());
        }

        RemBlocks--;
    }

    if (OutputFIFO->Level() >= OutputDMASize)
    {
        // trigger DMA
        DSi::CheckNDMAs(1, 0x2B);
    }
    // TODO: DMA the other way around

    if (RemBlocks == 0)
    {
        Cnt &= ~(1<<31);
        if (Cnt & (1<<30)) NDS::SetIRQ2(NDS::IRQ2_DSi_AES);
        DSi::StopNDMAs(1, 0x2B);
    }
}


void WriteIV(u32 offset, u32 val, u32 mask)
{
    u32 old = *(u32*)&IV[offset];

    *(u32*)&IV[offset] = (old & ~mask) | (val & mask);

    //printf("AES: IV: "); _printhex(IV, 16);

    u8 tmp[16];
    Swap16(tmp, IV);
    AES_ctx_set_iv(&Ctx, tmp);
}

void WriteMAC(u32 offset, u32 val, u32 mask)
{
    //
}

void DeriveNormalKey(u32 slot)
{
    const u8 key_const[16] = {0xFF, 0xFE, 0xFB, 0x4E, 0x29, 0x59, 0x02, 0x58, 0x2A, 0x68, 0x0F, 0x5F, 0x1A, 0x4F, 0x3E, 0x79};
    u8 tmp[16];

    //printf("keyX: "); _printhex(KeyX[slot], 16);
    //printf("keyY: "); _printhex(KeyY[slot], 16);

    for (int i = 0; i < 16; i++)
        tmp[i] = KeyX[slot][i] ^ KeyY[slot][i];

    u32 carry = 0;
    for (int i = 0; i < 16; i++)
    {
        u32 res = tmp[i] + key_const[15-i] + carry;
        tmp[i] = res & 0xFF;
        carry = res >> 8;
    }

    ROL16(tmp, 42);

    //printf("derive normalkey %d\n", slot); _printhex(tmp, 16);

    memcpy(KeyNormal[slot], tmp, 16);
}

void WriteKeyNormal(u32 slot, u32 offset, u32 val, u32 mask)
{
    u32 old = *(u32*)&KeyNormal[slot][offset];

    *(u32*)&KeyNormal[slot][offset] = (old & ~mask) | (val & mask);
}

void WriteKeyX(u32 slot, u32 offset, u32 val, u32 mask)
{
    u32 old = *(u32*)&KeyX[slot][offset];

    *(u32*)&KeyX[slot][offset] = (old & ~mask) | (val & mask);
}

void WriteKeyY(u32 slot, u32 offset, u32 val, u32 mask)
{
    u32 old = *(u32*)&KeyY[slot][offset];

    *(u32*)&KeyY[slot][offset] = (old & ~mask) | (val & mask);

    if (offset >= 0xC)
    {
        DeriveNormalKey(slot);
    }
}

}
