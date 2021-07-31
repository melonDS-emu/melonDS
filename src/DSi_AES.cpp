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
#include "DSi.h"
#include "DSi_AES.h"
#include "FIFO.h"
#include "tiny-AES-c/aes.hpp"
#include "Platform.h"


namespace DSi_AES
{

u32 Cnt;

u32 BlkCnt;
u32 RemBlocks;

bool OutputFlush;

u32 InputDMASize, OutputDMASize;
u32 AESMode;

FIFO<u32, 16> InputFIFO;
FIFO<u32, 16> OutputFIFO;

u8 IV[16];

u8 MAC[16];

u8 KeyNormal[4][16];
u8 KeyX[4][16];
u8 KeyY[4][16];

u8 CurKey[16];
u8 CurMAC[16];

// output MAC for CCM encrypt
u8 OutputMAC[16];
bool OutputMACDue;

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
#define _printhex2(str, size) { for (int z = 0; z < (size); z++) printf("%02X", (str)[z]); }

#define _printhexR(str, size) { for (int z = 0; z < (size); z++) printf("%02X", (str)[((size)-1)-z]); printf("\n"); }
#define _printhex2R(str, size) { for (int z = 0; z < (size); z++) printf("%02X", (str)[((size)-1)-z]); }


bool Init()
{
    const u8 zero[16] = {0};
    AES_init_ctx_iv(&Ctx, zero, zero);

    return true;
}

void DeInit()
{
}

void Reset()
{
    Cnt = 0;

    BlkCnt = 0;
    RemBlocks = 0;

    OutputFlush = false;

    InputDMASize = 0;
    OutputDMASize = 0;
    AESMode = 0;

    InputFIFO.Clear();
    OutputFIFO.Clear();

    memset(IV, 0, sizeof(IV));

    memset(MAC, 0, sizeof(MAC));

    memset(KeyNormal, 0, sizeof(KeyNormal));
    memset(KeyX, 0, sizeof(KeyX));
    memset(KeyY, 0, sizeof(KeyY));

    memset(CurKey, 0, sizeof(CurKey));
    memset(CurMAC, 0, sizeof(CurMAC));

    memset(OutputMAC, 0, sizeof(OutputMAC));
    OutputMACDue = false;

    // initialize keys

    // slot 0: modcrypt
    *(u32*)&KeyX[0][0] = 0x746E694E;
    *(u32*)&KeyX[0][4] = 0x6F646E65;

    // slot 1: 'Tad'/dev.kp
    *(u32*)&KeyX[1][0] = 0x4E00004A;
    *(u32*)&KeyX[1][4] = 0x4A00004E;
    *(u32*)&KeyX[1][8] = (u32)(DSi::ConsoleID >> 32) ^ 0xC80C4B72;
    *(u32*)&KeyX[1][12] = (u32)DSi::ConsoleID;

    // slot 3: console-unique eMMC crypto
    *(u32*)&KeyX[3][0] = (u32)DSi::ConsoleID;
    *(u32*)&KeyX[3][4] = (u32)DSi::ConsoleID ^ 0x24EE6906;
    *(u32*)&KeyX[3][8] = (u32)(DSi::ConsoleID >> 32) ^ 0xE65B601D;
    *(u32*)&KeyX[3][12] = (u32)(DSi::ConsoleID >> 32);
    *(u32*)&KeyY[3][0] = 0x0AB9DC76;
    *(u32*)&KeyY[3][4] = 0xBD4DC4D3;
    *(u32*)&KeyY[3][8] = 0x202DDD1D;
}


void ProcessBlock_CCM_Decrypt()
{
    u8 data[16];
    u8 data_rev[16];

    *(u32*)&data[0] = InputFIFO.Read();
    *(u32*)&data[4] = InputFIFO.Read();
    *(u32*)&data[8] = InputFIFO.Read();
    *(u32*)&data[12] = InputFIFO.Read();

    //printf("AES-CCM: "); _printhex2(data, 16);

    Swap16(data_rev, data);

    AES_CTR_xcrypt_buffer(&Ctx, data_rev, 16);
    for (int i = 0; i < 16; i++) CurMAC[i] ^= data_rev[i];
    AES_ECB_encrypt(&Ctx, CurMAC);

    Swap16(data, data_rev);

    //printf(" -> "); _printhex2(data, 16);

    OutputFIFO.Write(*(u32*)&data[0]);
    OutputFIFO.Write(*(u32*)&data[4]);
    OutputFIFO.Write(*(u32*)&data[8]);
    OutputFIFO.Write(*(u32*)&data[12]);
}

void ProcessBlock_CCM_Encrypt()
{
    u8 data[16];
    u8 data_rev[16];

    *(u32*)&data[0] = InputFIFO.Read();
    *(u32*)&data[4] = InputFIFO.Read();
    *(u32*)&data[8] = InputFIFO.Read();
    *(u32*)&data[12] = InputFIFO.Read();

    //printf("AES-CCM: "); _printhex2(data, 16);

    Swap16(data_rev, data);

    for (int i = 0; i < 16; i++) CurMAC[i] ^= data_rev[i];
    AES_CTR_xcrypt_buffer(&Ctx, data_rev, 16);
    AES_ECB_encrypt(&Ctx, CurMAC);

    Swap16(data, data_rev);

    //printf(" -> "); _printhex2(data, 16);

    OutputFIFO.Write(*(u32*)&data[0]);
    OutputFIFO.Write(*(u32*)&data[4]);
    OutputFIFO.Write(*(u32*)&data[8]);
    OutputFIFO.Write(*(u32*)&data[12]);
}

void ProcessBlock_CTR()
{
    u8 data[16];
    u8 data_rev[16];

    *(u32*)&data[0] = InputFIFO.Read();
    *(u32*)&data[4] = InputFIFO.Read();
    *(u32*)&data[8] = InputFIFO.Read();
    *(u32*)&data[12] = InputFIFO.Read();

    //printf("AES-CTR: "); _printhex2(data, 16);

    Swap16(data_rev, data);
    AES_CTR_xcrypt_buffer(&Ctx, data_rev, 16);
    Swap16(data, data_rev);

    //printf(" -> "); _printhex(data, 16);

    OutputFIFO.Write(*(u32*)&data[0]);
    OutputFIFO.Write(*(u32*)&data[4]);
    OutputFIFO.Write(*(u32*)&data[8]);
    OutputFIFO.Write(*(u32*)&data[12]);
}


u32 ReadCnt()
{
    u32 ret = Cnt;

    ret |= InputFIFO.Level();
    ret |= (OutputFIFO.Level() << 5);

    return ret;
}

void WriteCnt(u32 val)
{
    u32 oldcnt = Cnt;
    Cnt = val & 0xFC1FF000;

    /*if (val & (3<<10))
    {
        if (val & (1<<11)) OutputFlush = true;
        Update();
    }*/

    u32 dmasize_in[4] = {0, 4, 8, 12};
    u32 dmasize_out[4] = {4, 8, 12, 16};
    InputDMASize = dmasize_in[(val >> 12) & 0x3];
    OutputDMASize = dmasize_out[(val >> 14) & 0x3];

    AESMode = (val >> 28) & 0x3;

    if (val & (1<<24))
    {
        u32 slot = (val >> 26) & 0x3;
        memcpy(CurKey, KeyNormal[slot], 16);
    }

    if (!(oldcnt & (1<<31)) && (val & (1<<31)))
    {
        // transfer start (checkme)
        RemBlocks = BlkCnt >> 16;

        OutputMACDue = false;

        if (AESMode == 0 && (!(val & (1<<20)))) printf("AES: CCM-DECRYPT MAC FROM WRFIFO, TODO\n");

        if (RemBlocks > 0)
        {
            u8 key[16];
            u8 iv[16];

            Swap16(key, CurKey);
            Swap16(iv, IV);

            if (AESMode < 2)
            {
                if (BlkCnt & 0xFFFF) printf("AES: CCM EXTRA LEN TODO\n");

                u32 maclen = (val >> 16) & 0x7;
                if (maclen < 1) maclen = 1;

                iv[0] = 0x02;
                for (int i = 0; i < 12; i++) iv[1+i] = iv[4+i];
                iv[13] = 0x00;
                iv[14] = 0x00;
                iv[15] = 0x01;

                AES_init_ctx_iv(&Ctx, key, iv);

                iv[0] |= (maclen << 3) | ((BlkCnt & 0xFFFF) ? (1<<6) : 0);
                iv[13] = RemBlocks >> 12;
                iv[14] = RemBlocks >> 4;
                iv[15] = RemBlocks << 4;

                memcpy(CurMAC, iv, 16);
                AES_ECB_encrypt(&Ctx, CurMAC);
            }
            else
            {
                AES_init_ctx_iv(&Ctx, key, iv);
            }

            DSi::CheckNDMAs(1, 0x2A);
        }
        else
        {
            // no blocks to process? oh well. mark it finished
            // CHECKME: does this trigger any IRQ or shit?

            Cnt &= ~(1<<31);
        }
    }

    //printf("AES CNT: %08X / mode=%d key=%d inDMA=%d outDMA=%d blocks=%d\n",
    //       val, AESMode, (val >> 26) & 0x3, InputDMASize, OutputDMASize, RemBlocks);
}

void WriteBlkCnt(u32 val)
{
    BlkCnt = val;
}

u32 ReadOutputFIFO()
{
    if (OutputFIFO.IsEmpty()) printf("!!! AES OUTPUT FIFO EMPTY\n");

    u32 ret = OutputFIFO.Read();

    if (Cnt & (1<<31))
    {
        CheckInputDMA();
        CheckOutputDMA();
    }
    else
    {
        if (OutputFIFO.Level() > 0)
            DSi::CheckNDMAs(1, 0x2B);
        else
            DSi::StopNDMAs(1, 0x2B);

        if (OutputMACDue && OutputFIFO.Level() <= 12)
        {
            OutputFIFO.Write(*(u32*)&OutputMAC[0]);
            OutputFIFO.Write(*(u32*)&OutputMAC[4]);
            OutputFIFO.Write(*(u32*)&OutputMAC[8]);
            OutputFIFO.Write(*(u32*)&OutputMAC[12]);
            OutputMACDue = false;
        }
    }

    return ret;
}

void WriteInputFIFO(u32 val)
{
    // TODO: add some delay to processing

    if (InputFIFO.IsFull()) printf("!!! AES INPUT FIFO FULL\n");

    InputFIFO.Write(val);

    if (!(Cnt & (1<<31))) return;

    Update();
}

void CheckInputDMA()
{
    if (RemBlocks == 0) return;

    if (InputFIFO.Level() <= InputDMASize)
    {
        // trigger input DMA
        DSi::CheckNDMAs(1, 0x2A);
    }

    Update();
}

void CheckOutputDMA()
{
    if (OutputFIFO.Level() >= OutputDMASize)
    {
        // trigger output DMA
        DSi::CheckNDMAs(1, 0x2B);
    }
}

void Update()
{
    while (InputFIFO.Level() >= 4 && OutputFIFO.Level() <= 12 && RemBlocks > 0)
    {
        switch (AESMode)
        {
        case 0: ProcessBlock_CCM_Decrypt(); break;
        case 1: ProcessBlock_CCM_Encrypt(); break;
        case 2:
        case 3: ProcessBlock_CTR(); break;
        }

        RemBlocks--;
    }

    CheckOutputDMA();

    if (RemBlocks == 0)
    {
        if (AESMode == 0)
        {
            Ctx.Iv[13] = 0x00;
            Ctx.Iv[14] = 0x00;
            Ctx.Iv[15] = 0x00;
            AES_CTR_xcrypt_buffer(&Ctx, CurMAC, 16);

            //printf("FINAL MAC: "); _printhexR(CurMAC, 16);
            //printf("INPUT MAC: "); _printhex(MAC, 16);

            Cnt |= (1<<21);
            for (int i = 0; i < 16; i++)
            {
                if (CurMAC[15-i] != MAC[i]) Cnt &= ~(1<<21);
            }
        }
        else if (AESMode == 1)
        {
            Ctx.Iv[13] = 0x00;
            Ctx.Iv[14] = 0x00;
            Ctx.Iv[15] = 0x00;
            AES_CTR_xcrypt_buffer(&Ctx, CurMAC, 16);

            Swap16(OutputMAC, CurMAC);
            OutputMACDue = true;

            // CHECKME
            Cnt &= ~(1<<21);
        }
        else
        {
            // CHECKME
            Cnt &= ~(1<<21);
        }

        Cnt &= ~(1<<31);
        if (Cnt & (1<<30)) NDS::SetIRQ2(NDS::IRQ2_DSi_AES);
        DSi::StopNDMAs(1, 0x2A);

        if (!OutputFIFO.IsEmpty())
            DSi::CheckNDMAs(1, 0x2B);
        else
            DSi::StopNDMAs(1, 0x2B);
        OutputFlush = false;
    }
}


void WriteIV(u32 offset, u32 val, u32 mask)
{
    u32 old = *(u32*)&IV[offset];

    *(u32*)&IV[offset] = (old & ~mask) | (val & mask);

    //printf("AES: IV: "); _printhex(IV, 16);
}

void WriteMAC(u32 offset, u32 val, u32 mask)
{
    u32 old = *(u32*)&MAC[offset];

    *(u32*)&MAC[offset] = (old & ~mask) | (val & mask);

    //printf("AES: MAC: "); _printhex(MAC, 16);
}

void DeriveNormalKey(u32 slot)
{
    const u8 key_const[16] = {0xFF, 0xFE, 0xFB, 0x4E, 0x29, 0x59, 0x02, 0x58, 0x2A, 0x68, 0x0F, 0x5F, 0x1A, 0x4F, 0x3E, 0x79};
    u8 tmp[16];

    //printf("slot%d keyX: ", slot); _printhex(KeyX[slot], 16);
    //printf("slot%d keyY: ", slot); _printhex(KeyY[slot], 16);

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

    //printf("KeyNormal(%d): ", slot); _printhex(KeyNormal[slot], 16);
}

void WriteKeyX(u32 slot, u32 offset, u32 val, u32 mask)
{
    u32 old = *(u32*)&KeyX[slot][offset];

    *(u32*)&KeyX[slot][offset] = (old & ~mask) | (val & mask);

    //printf("KeyX(%d): ", slot); _printhex(KeyX[slot], 16);
}

void WriteKeyY(u32 slot, u32 offset, u32 val, u32 mask)
{
    u32 old = *(u32*)&KeyY[slot][offset];

    *(u32*)&KeyY[slot][offset] = (old & ~mask) | (val & mask);

    //printf("[%08X] KeyY(%d): ", NDS::GetPC(1), slot); _printhex(KeyY[slot], 16);

    if (offset >= 0xC)
    {
        DeriveNormalKey(slot);
    }
}


// utility

void GetModcryptKey(u8* romheader, u8* key)
{
    if ((romheader[0x01C] & 0x04) || (romheader[0x1BF] & 0x80))
    {
        // dev key
        memcpy(key, &romheader[0x000], 16);
        return;
    }

    u8 oldkeys[16*3];
    memcpy(&oldkeys[16*0], KeyX[0], 16);
    memcpy(&oldkeys[16*1], KeyY[0], 16);
    memcpy(&oldkeys[16*2], KeyNormal[0], 16);

    KeyX[0][8] = romheader[0x00C];
    KeyX[0][9] = romheader[0x00D];
    KeyX[0][10] = romheader[0x00E];
    KeyX[0][11] = romheader[0x00F];
    KeyX[0][12] = romheader[0x00F];
    KeyX[0][13] = romheader[0x00E];
    KeyX[0][14] = romheader[0x00D];
    KeyX[0][15] = romheader[0x00C];

    memcpy(KeyY[0], &romheader[0x350], 16);

    DeriveNormalKey(0);
    memcpy(key, KeyNormal[0], 16);

    memcpy(KeyX[0], &oldkeys[16*0], 16);
    memcpy(KeyY[0], &oldkeys[16*1], 16);
    memcpy(KeyNormal[0], &oldkeys[16*2], 16);
}

void ApplyModcrypt(u8* data, u32 len, u8* key, u8* iv)
{
    u8 key_rev[16], iv_rev[16];
    u8 data_rev[16];
    u8 oldkeys[16*2];
    memcpy(&oldkeys[16*0], Ctx.RoundKey, 16);
    memcpy(&oldkeys[16*1], Ctx.Iv, 16);

    Swap16(key_rev, key);
    Swap16(iv_rev, iv);
    AES_init_ctx_iv(&Ctx, key_rev, iv_rev);

    for (u32 i = 0; i < len; i += 16)
    {
        Swap16(data_rev, &data[i]);
        AES_CTR_xcrypt_buffer(&Ctx, data_rev, 16);
        Swap16(&data[i], data_rev);
    }

    memcpy(Ctx.RoundKey, &oldkeys[16*0], 16);
    memcpy(Ctx.Iv, &oldkeys[16*1], 16);
}

}
