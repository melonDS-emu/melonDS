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

#include "DSi.h"
#include "DSi_AES.h"
#include "DSi_NAND.h"

#include "sha1/sha1.hpp"
#include "tiny-AES-c/aes.hpp"

#include "fatfs/ff.h"


namespace DSi_NAND
{

u8 FATIV[16];
u8 FATKey[16];

u8 ESKey[16];


void Init()
{
    // init NAND crypto

    SHA1_CTX sha;
    u8 tmp[20];
    u8 keyX[16], keyY[16];

    SHA1Init(&sha);
    SHA1Update(&sha, DSi::eMMC_CID, 16);
    SHA1Final(tmp, &sha);

    DSi_AES::Swap16(FATIV, tmp);

    *(u32*)&keyX[0] = (u32)DSi::ConsoleID;
    *(u32*)&keyX[4] = (u32)DSi::ConsoleID ^ 0x24EE6906;
    *(u32*)&keyX[8] = (u32)(DSi::ConsoleID >> 32) ^ 0xE65B601D;
    *(u32*)&keyX[12] = (u32)(DSi::ConsoleID >> 32);

    *(u32*)&keyY[0] = 0x0AB9DC76;
    *(u32*)&keyY[4] = 0xBD4DC4D3;
    *(u32*)&keyY[8] = 0x202DDD1D;
    *(u32*)&keyY[12] = 0xE1A00005;

    DSi_AES::DeriveNormalKey(keyX, keyY, tmp);
    DSi_AES::Swap16(FATKey, tmp);


    *(u32*)&keyX[0] = 0x4E00004A;
    *(u32*)&keyX[4] = 0x4A00004E;
    *(u32*)&keyX[8] = (u32)(DSi::ConsoleID >> 32) ^ 0xC80C4B72;
    *(u32*)&keyX[12] = (u32)DSi::ConsoleID;

    memcpy(keyY, &DSi::ARM7iBIOS[0x8308], 16);

    DSi_AES::DeriveNormalKey(keyX, keyY, tmp);
    DSi_AES::Swap16(ESKey, tmp);
}


void SetupFATCrypto(AES_ctx* ctx, u32 ctr)
{
    u8 iv[16];
    memcpy(iv, FATIV, 16);

    u32 res;
    res = iv[15] + (ctr & 0xFF);
    iv[15] = (res & 0xFF);
    res = iv[14] + ((ctr >> 8) & 0xFF) + (res >> 8);
    iv[14] = (res & 0xFF);
    res = iv[13] + ((ctr >> 16) & 0xFF) + (res >> 8);
    iv[13] = (res & 0xFF);
    res = iv[12] + (ctr >> 24) + (res >> 8);
    iv[12] = (res & 0xFF);
    iv[11] += (res >> 8);
    for (int i = 10; i >= 0; i--)
    {
        if (iv[i+1] == 0) iv[i]++;
        else break;
    }

    AES_init_ctx_iv(ctx, FATKey, iv);
}

u32 ReadFATBlock(u64 addr, u32 len, u8* buf)
{
    u32 ctr = (u32)(addr >> 4);

    AES_ctx ctx;
    SetupFATCrypto(&ctx, ctr);

    fseek(DSi::SDMMCFile, addr, SEEK_SET);
    u32 res = fread(buf, len, 1, DSi::SDMMCFile);
    if (!res) return 0;

    for (u32 i = 0; i < len; i += 16)
    {
        u8 tmp[16];
        DSi_AES::Swap16(tmp, &buf[i]);
        AES_CTR_xcrypt_buffer(&ctx, tmp, 16);
        DSi_AES::Swap16(&buf[i], tmp);
    }

    return len;
}

u32 WriteFATBlock(u64 addr, u32 len, u8* buf)
{
    u32 ctr = (u32)(addr >> 4);

    AES_ctx ctx;
    SetupFATCrypto(&ctx, ctr);

    fseek(DSi::SDMMCFile, addr, SEEK_SET);

    for (u32 s = 0; s < len; s += 0x200)
    {
        u8 tempbuf[0x200];

        for (u32 i = 0; i < 0x200; i += 16)
        {
            u8 tmp[16];
            DSi_AES::Swap16(tmp, &buf[s+i]);
            AES_CTR_xcrypt_buffer(&ctx, tmp, 16);
            DSi_AES::Swap16(&tempbuf[i], tmp);
        }

        u32 res = fwrite(tempbuf, 0x200, 1, DSi::SDMMCFile);
        if (!res) return 0;
    }

    return len;
}


UINT FF_ReadNAND(BYTE* buf, LBA_t sector, UINT num)
{
    // TODO: allow selecting other partitions?
    u64 baseaddr = 0x10EE00;

    u64 blockaddr = baseaddr + (sector * 0x200ULL);

    u32 res = ReadFATBlock(blockaddr, num*0x200, buf);
    return res >> 9;
}

UINT FF_WriteNAND(BYTE* buf, LBA_t sector, UINT num)
{
    // TODO: allow selecting other partitions?
    u64 baseaddr = 0x10EE00;

    u64 blockaddr = baseaddr + (sector * 0x200ULL);

    u32 res = WriteFATBlock(blockaddr, num*0x200, buf);
    return res >> 9;
}


bool ESEncrypt(u8* data, u32 len)
{
    AES_ctx ctx;
    u8 iv[16];
    u8 mac[16];

    iv[0] = 0x02;
    for (int i = 0; i < 12; i++) iv[1+i] = data[len+0x1C-i];
    iv[13] = 0x00;
    iv[14] = 0x00;
    iv[15] = 0x01;

    AES_init_ctx_iv(&ctx, ESKey, iv);

    u32 blklen = (len + 0xF) & ~0xF;
    mac[0] = 0x3A;
    for (int i = 1; i < 13; i++) mac[i] = iv[i];
    mac[13] = (blklen >> 16) & 0xFF;
    mac[14] = (blklen >> 8) & 0xFF;
    mac[15] = blklen & 0xFF;

    AES_ECB_encrypt(&ctx, mac);

    u32 coarselen = len & ~0xF;
    for (u32 i = 0; i < coarselen; i += 16)
    {
        u8 tmp[16];

        DSi_AES::Swap16(tmp, &data[i]);

        for (int i = 0; i < 16; i++) mac[i] ^= tmp[i];
        AES_CTR_xcrypt_buffer(&ctx, tmp, 16);
        AES_ECB_encrypt(&ctx, mac);

        DSi_AES::Swap16(&data[i], tmp);
    }

    u32 remlen = len - coarselen;
    if (remlen)
    {
        u8 rem[16];

        memset(rem, 0, 16);

        for (int i = 0; i < remlen; i++)
            rem[15-i] = data[coarselen+i];

        for (int i = 0; i < 16; i++) mac[i] ^= rem[i];
        AES_CTR_xcrypt_buffer(&ctx, rem, 16);
        AES_ECB_encrypt(&ctx, mac);

        for (int i = 0; i < remlen; i++)
            data[coarselen+i] = rem[15-i];
    }

    ctx.Iv[13] = 0x00;
    ctx.Iv[14] = 0x00;
    ctx.Iv[15] = 0x00;
    AES_CTR_xcrypt_buffer(&ctx, mac, 16);

    for (int i = 0; i < 16; i++)
        data[len+i] = mac[15-i];

    u8 footer[16];

    iv[0] = 0x00;
    iv[1] = 0x00;
    iv[2] = 0x00;
    for (int i = 0; i < 12; i++) iv[3+i] = data[len+0x1C-i];
    iv[15] = 0x00;

    footer[15] = 0x3A;
    footer[2] = (len >> 16) & 0xFF;
    footer[1] = (len >> 8) & 0xFF;
    footer[0] = len & 0xFF;

    AES_ctx_set_iv(&ctx, iv);
    AES_CTR_xcrypt_buffer(&ctx, footer, 16);

    data[len+0x10] = footer[15];
    data[len+0x1D] = footer[2];
    data[len+0x1E] = footer[1];
    data[len+0x1F] = footer[0];

    return true;
}

bool ESDecrypt(u8* data, u32 len)
{
    AES_ctx ctx;
    u8 iv[16];
    u8 mac[16];

    iv[0] = 0x02;
    for (int i = 0; i < 12; i++) iv[1+i] = data[len+0x1C-i];
    iv[13] = 0x00;
    iv[14] = 0x00;
    iv[15] = 0x01;

    AES_init_ctx_iv(&ctx, ESKey, iv);

    u32 blklen = (len + 0xF) & ~0xF;
    mac[0] = 0x3A;
    for (int i = 1; i < 13; i++) mac[i] = iv[i];
    mac[13] = (blklen >> 16) & 0xFF;
    mac[14] = (blklen >> 8) & 0xFF;
    mac[15] = blklen & 0xFF;

    AES_ECB_encrypt(&ctx, mac);

    u32 coarselen = len & ~0xF;
    for (u32 i = 0; i < coarselen; i += 16)
    {
        u8 tmp[16];

        DSi_AES::Swap16(tmp, &data[i]);

        AES_CTR_xcrypt_buffer(&ctx, tmp, 16);
        for (int i = 0; i < 16; i++) mac[i] ^= tmp[i];
        AES_ECB_encrypt(&ctx, mac);

        DSi_AES::Swap16(&data[i], tmp);
    }

    u32 remlen = len - coarselen;
    if (remlen)
    {
        u8 rem[16];

        u32 ivnum = (coarselen >> 4) + 1;
        iv[13] = (ivnum >> 16) & 0xFF;
        iv[14] = (ivnum >> 8) & 0xFF;
        iv[15] = ivnum & 0xFF;

        memset(rem, 0, 16);
        AES_ctx_set_iv(&ctx, iv);
        AES_CTR_xcrypt_buffer(&ctx, rem, 16);

        for (int i = 0; i < remlen; i++)
            rem[15-i] = data[coarselen+i];

        AES_ctx_set_iv(&ctx, iv);
        AES_CTR_xcrypt_buffer(&ctx, rem, 16);
        for (int i = 0; i < 16; i++) mac[i] ^= rem[i];
        AES_ECB_encrypt(&ctx, mac);

        for (int i = 0; i < remlen; i++)
            data[coarselen+i] = rem[15-i];
    }

    ctx.Iv[13] = 0x00;
    ctx.Iv[14] = 0x00;
    ctx.Iv[15] = 0x00;
    AES_CTR_xcrypt_buffer(&ctx, mac, 16);

    u8 footer[16];

    iv[0] = 0x00;
    iv[1] = 0x00;
    iv[2] = 0x00;
    for (int i = 0; i < 12; i++) iv[3+i] = data[len+0x1C-i];
    iv[15] = 0x00;

    for (int i = 0; i < 16; i++)
        footer[15-i] = data[len+0x10+i];

    AES_ctx_set_iv(&ctx, iv);
    AES_CTR_xcrypt_buffer(&ctx, footer, 16);

    data[len+0x10] = footer[15];
    data[len+0x1D] = footer[2];
    data[len+0x1E] = footer[1];
    data[len+0x1F] = footer[0];

    u32 footerlen = footer[0] | (footer[1] << 8) | (footer[2] << 16);
    if (footerlen != len)
    {
        printf("ESDecrypt: bad length %d (expected %d)\n", len, footerlen);
        return false;
    }

    for (int i = 0; i < 16; i++)
    {
        if (data[len+i] != mac[15-i])
        {
            printf("ESDecrypt: bad MAC\n");
            return false;
        }
    }

    return true;
}


void PatchTSC()
{
    ff_disk_open(FF_ReadNAND, FF_WriteNAND);

    FRESULT res;
    FATFS fs;
    res = f_mount(&fs, "0:", 0);
    if (res != FR_OK)
    {
        printf("NAND mounting failed: %d\n", res);
        f_unmount("0:");
        ff_disk_close();
        return;
    }

    for (int i = 0; i < 2; i++)
    {
        char filename[64];
        sprintf(filename, "0:/shared1/TWLCFG%d.dat", i);

        FIL file;
        res = f_open(&file, filename, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
        if (res != FR_OK)
        {
            printf("NAND: editing file %s failed: %d\n", filename, res);
            continue;
        }

        u8 contents[0x1B0];
        u32 nres;
        f_lseek(&file, 0);
        f_read(&file, contents, 0x1B0, &nres);

        // fix touchscreen coords
        *(u16*)&contents[0xB8] = 0;
        *(u16*)&contents[0xBA] = 0;
        contents[0xBC] = 0;
        contents[0xBD] = 0;
        *(u16*)&contents[0xBE] = 255<<4;
        *(u16*)&contents[0xC0] = 191<<4;
        contents[0xC2] = 255;
        contents[0xC3] = 191;

        SHA1_CTX sha;
        SHA1Init(&sha);
        SHA1Update(&sha, &contents[0x88], 0x128);
        SHA1Final(&contents[0], &sha);

        f_lseek(&file, 0);
        f_write(&file, contents, 0x1B0, &nres);

        f_close(&file);
    }

    f_unmount("0:");
    ff_disk_close();
}


void debug_listfiles(char* path)
{
    DIR dir;
    FILINFO info;
    FRESULT res;

    res = f_opendir(&dir, path);
    if (res) return;

    for (;;)
    {
        res = f_readdir(&dir, &info);
        if (res) return;
        if (!info.fname[0]) return;

        char fullname[1024];
        sprintf(fullname, "%s/%s", path, info.fname);
        printf("[%c] %s\n", (info.fattrib&AM_DIR)?'D':'F', fullname);

        if (info.fattrib & AM_DIR)
        {
            debug_listfiles(fullname);
        }
    }
}

void debug_dumpfile(char* path, char* out)
{
    FIL file;
    FILE* fout;
    FRESULT res;

    res = f_open(&file, path, FA_OPEN_EXISTING | FA_READ);
    if (res) return;

    u32 len = f_size(&file);
    printf("%s: len=%d\n", path, len);

    fout = fopen(out, "wb");

    u8 buf[0x200];
    for (u32 i = 0; i < len; i += 0x200)
    {
        u32 blocklen;
        if ((i + 0x200) > len)
            blocklen = len - i;
        else
            blocklen = 0x200;

        u32 burp;
        f_read(&file, buf, blocklen, &burp);
        fwrite(buf, blocklen, 1, fout);
    }

    fclose(fout);
    f_close(&file);
}

void CreateTicket(char* path, u32 titleid0, u32 titleid1, u8 version)
{
    FIL file;
    FRESULT res;
    u32 nwrite;

    res = f_open(&file, path, FA_CREATE_NEW | FA_WRITE);
    if (res != FR_OK)
    {
        printf("CreateTicket: failed to create file (%d)\n", res);
        return;
    }

    u8 ticket[0x2C4];
    memset(ticket, 0, 0x2C4);

    // signature, atleast make it look like we tried :P
    *(u32*)&ticket[0x000] = 0x01000100;
    strcpy((char*)&ticket[0x140], "Root-CA00000001-XS00000006");

    *(u32*)&ticket[0x1DC] = titleid0;
    *(u32*)&ticket[0x1E0] = titleid1;
    ticket[0x1E6] = version;

    memset(&ticket[0x222], 0xFF, 0x20);

    ESEncrypt(ticket, 0x2A4);

    f_write(&file, ticket, 0x2C4, &nwrite);

    f_close(&file);
}

void CreateSaveFile(char* path, u32 len)
{
    if (len < 0x200) return;
    if (len > 0x8000000) return;

    u32 clustersize, maxfiles, totsec16, fatsz16;

    if (len < 573440)
    {
        clustersize = 512;
        maxfiles = 16;
    }
    else if (len < 5472256)
    {
        clustersize = 2048;
        maxfiles = 256;
    }
    else
    {
        clustersize = 4096;
        maxfiles = 256;
    }

    if (len <= 0x4000)        fatsz16 = 1;
    else if (len <= 0x200000) fatsz16 = 3;
    else                      fatsz16 = 6;

    if (len == 0x4000) totsec16 = 27;
    else               totsec16 = len >> 9;

    FIL file;
    FRESULT res;
    u32 nwrite;

    res = f_open(&file, path, FA_CREATE_NEW | FA_WRITE);
    if (res != FR_OK)
    {
        printf("CreateSaveFile: failed to create file (%d)\n", res);
        return;
    }

    u8* data = new u8[len];
    memset(data, 0, len);

    // create FAT header
    data[0x000] = 0xE9;
    memcpy(&data[0x003], "MSWIN4.1", 8);
    *(u16*)&data[0x00B] = 512; // bytes per sector
    data[0x00D] = clustersize >> 9;
    *(u16*)&data[0x00E] = 1; // reserved sectors
    data[0x010] = 2; // num FATs
    *(u16*)&data[0x011] = maxfiles << 1;
    *(u16*)&data[0x013] = totsec16;
    data[0x015] = 0xF8;
    *(u16*)&data[0x016] = fatsz16;
    data[0x024] = 0x07;
    data[0x026] = 0x29;
    *(u32*)&data[0x027] = 305419896;
    memcpy(&data[0x02B], "VOLUMELABEL", 11);
    memcpy(&data[0x036], "FAT12   ", 8);
    *(u16*)&data[0x1FE] = 0xAA55;

    f_write(&file, data, len, &nwrite);

    f_close(&file);
    delete[] data;
}

void ImportTest()
{
    char* tmdfile = "treasure.tmd";
    char* appfile = "treasure.nds";

    ff_disk_open(FF_ReadNAND, FF_WriteNAND);

    FRESULT res;
    FATFS fs;
    res = f_mount(&fs, "0:", 0);
    if (res != FR_OK)
    {
        printf("NAND mounting failed: %d\n", res);
        f_unmount("0:");
        ff_disk_close();
        return;
    }

    /*debug_listfiles("0:");

    debug_dumpfile("0:/sys/log/sysmenu.log", "sysmenu.log");
    debug_dumpfile("0:/sys/log/product.log", "product.log");

    f_unmount("0:");
    ff_disk_close();
    return;*/

    //debug_dumpfile("0:/title/00030004/4b4e4448/content/00000002.app", "00000002.app");
    //debug_dumpfile("0:/title/00030004/4b4e4448/content/title.tmd", "title.tmd");
    //f_unlink("0:/title/00030004/4b475556/data/public.sav");
    //f_unlink("0:/ticket/00030004/4b475556.tik");
    //debug_dumpfile("0:/title/00030004/4b475556/content/title.tmd", "flipnote.tmd");
    //f_unlink("0:/title/00030004/4b475556/content/title.tmd");
    /*f_unmount("0:");
    ff_disk_close();
return;*/
    u8 tmd[0x208];
    {
        FILE* f = fopen(tmdfile, "rb");
        fread(tmd, 0x208, 1, f);
        fclose(f);
    }

    u8 header[0x1000];
    {
        FILE* f = fopen(appfile, "rb");
        fread(header, 0x1000, 1, f);
        fclose(f);
    }

    u32 version = (tmd[0x1E4] << 24) | (tmd[0x1E5] << 16) | (tmd[0x1E6] << 8) | tmd[0x1E7];
    printf(".app version: %08x\n", version);

    u32 titleid0 = (tmd[0x18C] << 24) | (tmd[0x18D] << 16) | (tmd[0x18E] << 8) | tmd[0x18F];
    u32 titleid1 = (tmd[0x190] << 24) | (tmd[0x191] << 16) | (tmd[0x192] << 8) | tmd[0x193];
    printf("Title ID: %08x/%08x\n", titleid0, titleid1);

    {
        DIR ticketdir;
        FILINFO info;

        char fname[128];
        FIL file;
        FRESULT res;
        u32 nwrite;

        /*res = f_opendir(&ticketdir, "0:/ticket/00030004");
        printf("dir res: %d\n", res);

        res = f_readdir(&ticketdir, &info);
        if (res)
        {
            printf("bad read res: %d\n", res);
        }

        if (!info.fname[0])
        {
            printf("VERY BAD!! DIR IS EMPTY\n");
            // TODO ERROR MANAGEMENT!!
        }

        printf("- %s\n", info.fname);
        char fname[128];
        sprintf(fname, "0:/ticket/00030004/%s", info.fname);

        f_closedir(&ticketdir);

        FIL file;
        res = f_open(&file, fname, FA_OPEN_EXISTING | FA_READ);
        //

        u8 ticket[0x2C4];
        u32 nread;
        f_read(&file, ticket, 0x2C4, &nread);
        //

        f_close(&file);

        ESDecrypt(ticket, 0x2A4);

        // change title ID
        *(u32*)&ticket[0x1DC] = *(u32*)&tmd[0x18C];
        *(u32*)&ticket[0x1E0] = *(u32*)&tmd[0x190];

        printf("ass console ID: %08X\n", *(u32*)&tmd[0x1D8]);
        *(u32*)&tmd[0x1D8] = 0;

        ESEncrypt(ticket, 0x2A4);*/

        // insert shit!

        // ticket

        sprintf(fname, "0:/ticket/%08x/%08x.tik", titleid0, titleid1);
        printf("TICKET: %s\n", fname);
        /*res = f_open(&file, fname, FA_CREATE_NEW | FA_WRITE);
        printf("TICKET: %d\n", res);

        f_write(&file, ticket, 0x2C4, &nread);
        //

        f_close(&file);*/

        CreateTicket(fname, *(u32*)&tmd[0x18C], *(u32*)&tmd[0x190], header[0x1E]);

        printf("----- POST TICKET:\n");
    debug_listfiles("0:");

        // folder

        sprintf(fname, "0:/title/%08x/%08x", titleid0, titleid1);
        res = f_mkdir(fname);
        printf("DIR0 RES=%d\n", res);
        sprintf(fname, "0:/title/%08x/%08x/content", titleid0, titleid1);
        res = f_mkdir(fname);
        printf("DIR1 RES=%d\n", res);
        sprintf(fname, "0:/title/%08x/%08x/data", titleid0, titleid1);
        res = f_mkdir(fname);
        printf("DIR2 RES=%d\n", res);

        printf("----- POST DIRS:\n");
    debug_listfiles("0:");

        // data

        sprintf(fname, "0:/title/%08x/%08x/data/public.sav", titleid0, titleid1);
        CreateSaveFile(fname, *(u32*)&header[0x238]);

        sprintf(fname, "0:/title/%08x/%08x/data/private.sav", titleid0, titleid1);
        CreateSaveFile(fname, *(u32*)&header[0x23C]);

        if (header[0x1BF] & 0x04)
        {
            // custom banner file
            sprintf(fname, "0:/title/%08x/%08x/data/banner.sav", titleid0, titleid1);
            res = f_open(&file, fname, FA_CREATE_NEW | FA_WRITE);

            u8 bannersav[0x4000];
            memset(bannersav, 0, 0x4000);
            f_write(&file, bannersav, 0x4000, &nwrite);

            f_close(&file);
        }

        printf("----- POST SAVE:\n");
    debug_listfiles("0:");

        // TMD

        sprintf(fname, "0:/title/%08x/%08x/content/title.tmd", titleid0, titleid1);
        printf("TMD: %s\n", fname);
        res = f_open(&file, fname, FA_CREATE_NEW | FA_WRITE);
        printf("TMD: %d\n", res);

        f_write(&file, tmd, 0x208, &nwrite);
        //

        f_close(&file);

        printf("----- POST TMD:\n");
    debug_listfiles("0:");

        // executable

        sprintf(fname, "0:/title/%08x/%08x/content/%08x.app", titleid0, titleid1, version);
        printf("APP: %s\n", fname);
        res = f_open(&file, fname, FA_CREATE_NEW | FA_WRITE);
        printf("APP: %d\n", res);

        FILE* app = fopen(appfile, "rb");
        fseek(app, 0, SEEK_END);
        u32 applen = (u32)ftell(app);
        fseek(app, 0, SEEK_SET);

        for (u32 i = 0; i < applen; i += 0x200)
        {
            u8 data[0x200];

            u32 lenread = fread(data, 1, 0x200, app);
            f_write(&file, data, lenread, &nwrite);
        }

        fclose(app);
        f_close(&file);
    }

    printf("----- POST INSERTION:\n");
    debug_listfiles("0:");

    f_unmount("0:");
    ff_disk_close();
}

}
