/*
    Copyright 2016-2022 melonDS team

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
#include <codecvt>

#include "DSi.h"
#include "DSi_AES.h"
#include "DSi_NAND.h"
#include "Platform.h"

#include "sha1/sha1.hpp"
#include "tiny-AES-c/aes.hpp"

#include "fatfs/ff.h"

using namespace Platform;

namespace DSi_NAND
{

FileHandle* CurFile;
FATFS CurFS;

u8 eMMC_CID[16];
u64 ConsoleID;

u8 FATIV[16];
u8 FATKey[16];

u8 ESKey[16];


UINT FF_ReadNAND(BYTE* buf, LBA_t sector, UINT num);
UINT FF_WriteNAND(BYTE* buf, LBA_t sector, UINT num);


bool Init(u8* es_keyY)
{
    CurFile = nullptr;

    std::string nandpath = Platform::GetConfigString(Platform::DSi_NANDPath);
    std::string instnand = nandpath + Platform::InstanceFileSuffix();

    FileHandle* nandfile = Platform::OpenLocalFile(instnand, FileMode::ReadWriteExisting);
    if ((!nandfile) && (Platform::InstanceID() > 0))
    {
        FileHandle* orig = Platform::OpenLocalFile(nandpath, FileMode::Read);
        if (!orig)
        {
            Log(LogLevel::Error, "Failed to open DSi NAND\n");
            return false;
        }

        long len = FileLength(orig);

        nandfile = Platform::OpenLocalFile(instnand, FileMode::ReadWrite);
        if (nandfile)
        {
            u8* tmpbuf = new u8[0x10000];
            for (long i = 0; i < len; i+=0x10000)
            {
                long blklen = 0x10000;
                if ((i+blklen) > len) blklen = len-i;

                FileRead(tmpbuf, blklen, 1, orig);
                FileWrite(tmpbuf, blklen, 1, nandfile);
            }
            delete[] tmpbuf;
        }

        Platform::CloseFile(orig);
        Platform::CloseFile(nandfile);

        nandfile = Platform::OpenLocalFile(instnand, FileMode::ReadWriteExisting);
    }

    if (!nandfile)
        return false;

    u64 nandlen = FileLength(nandfile);

    ff_disk_open(FF_ReadNAND, FF_WriteNAND, (LBA_t)(nandlen>>9));

    FRESULT res;
    res = f_mount(&CurFS, "0:", 0);
    if (res != FR_OK)
    {
        Log(LogLevel::Error, "NAND mounting failed: %d\n", res);
        f_unmount("0:");
        ff_disk_close();
        return false;
    }

    // read the nocash footer

    FileSeek(nandfile, -0x40, FileSeekOrigin::End);

    char nand_footer[16];
    const char* nand_footer_ref = "DSi eMMC CID/CPU";
    FileRead(nand_footer, 1, 16, nandfile);
    if (memcmp(nand_footer, nand_footer_ref, 16))
    {
        // There is another copy of the footer at 000FF800h for the case
        // that by external tools the image was cut off
        // See https://problemkaputt.de/gbatek.htm#dsisdmmcimages
        FileSeek(nandfile, 0x000FF800, FileSeekOrigin::Start);
        FileRead(nand_footer, 1, 16, nandfile);
        if (memcmp(nand_footer, nand_footer_ref, 16))
        {
            Log(LogLevel::Error, "ERROR: NAND missing nocash footer\n");
            CloseFile(nandfile);
            f_unmount("0:");
            ff_disk_close();
            return false;
        }
    }

    FileRead(eMMC_CID, 1, 16, nandfile);
    FileRead(&ConsoleID, 1, 8, nandfile);

    // init NAND crypto

    SHA1_CTX sha;
    u8 tmp[20];
    u8 keyX[16], keyY[16];

    SHA1Init(&sha);
    SHA1Update(&sha, eMMC_CID, 16);
    SHA1Final(tmp, &sha);

    Bswap128(FATIV, tmp);

    *(u32*)&keyX[0] = (u32)ConsoleID;
    *(u32*)&keyX[4] = (u32)ConsoleID ^ 0x24EE6906;
    *(u32*)&keyX[8] = (u32)(ConsoleID >> 32) ^ 0xE65B601D;
    *(u32*)&keyX[12] = (u32)(ConsoleID >> 32);

    *(u32*)&keyY[0] = 0x0AB9DC76;
    *(u32*)&keyY[4] = 0xBD4DC4D3;
    *(u32*)&keyY[8] = 0x202DDD1D;
    *(u32*)&keyY[12] = 0xE1A00005;

    DSi_AES::DeriveNormalKey(keyX, keyY, tmp);
    Bswap128(FATKey, tmp);


    *(u32*)&keyX[0] = 0x4E00004A;
    *(u32*)&keyX[4] = 0x4A00004E;
    *(u32*)&keyX[8] = (u32)(ConsoleID >> 32) ^ 0xC80C4B72;
    *(u32*)&keyX[12] = (u32)ConsoleID;

    memcpy(keyY, es_keyY, 16);

    DSi_AES::DeriveNormalKey(keyX, keyY, tmp);
    Bswap128(ESKey, tmp);

    CurFile = nandfile;
    return true;
}

void DeInit()
{
    f_unmount("0:");
    ff_disk_close();

    if (CurFile) CloseFile(CurFile);
    CurFile = nullptr;
}


FileHandle* GetFile()
{
    return CurFile;
}


void GetIDs(u8* emmc_cid, u64& consoleid)
{
    memcpy(emmc_cid, eMMC_CID, 16);
    consoleid = ConsoleID;
}


void SetupFATCrypto(AES_ctx* ctx, u32 ctr)
{
    u8 iv[16];
    memcpy(iv, FATIV, sizeof(iv));

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

    FileSeek(CurFile, addr, FileSeekOrigin::Start);
    u32 res = FileRead(buf, len, 1, CurFile);
    if (!res) return 0;

    for (u32 i = 0; i < len; i += 16)
    {
        u8 tmp[16];
        Bswap128(tmp, &buf[i]);
        AES_CTR_xcrypt_buffer(&ctx, tmp, sizeof(tmp));
        Bswap128(&buf[i], tmp);
    }

    return len;
}

u32 WriteFATBlock(u64 addr, u32 len, u8* buf)
{
    u32 ctr = (u32)(addr >> 4);

    AES_ctx ctx;
    SetupFATCrypto(&ctx, ctr);

    FileSeek(CurFile, addr, FileSeekOrigin::Start);

    for (u32 s = 0; s < len; s += 0x200)
    {
        u8 tempbuf[0x200];

        for (u32 i = 0; i < 0x200; i += 16)
        {
            u8 tmp[16];
            Bswap128(tmp, &buf[s+i]);
            AES_CTR_xcrypt_buffer(&ctx, tmp, sizeof(tmp));
            Bswap128(&tempbuf[i], tmp);
        }

        u32 res = FileWrite(tempbuf, sizeof(tempbuf), 1, CurFile);
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

        Bswap128(tmp, &data[i]);

        for (int i = 0; i < 16; i++) mac[i] ^= tmp[i];
        AES_CTR_xcrypt_buffer(&ctx, tmp, 16);
        AES_ECB_encrypt(&ctx, mac);

        Bswap128(&data[i], tmp);
    }

    u32 remlen = len - coarselen;
    if (remlen)
    {
        u8 rem[16];

        memset(rem, 0, 16);
        for (int i = 0; i < remlen; i++)
            rem[15-i] = data[coarselen+i];

        for (int i = 0; i < 16; i++) mac[i] ^= rem[i];
        AES_CTR_xcrypt_buffer(&ctx, rem, sizeof(rem));
        AES_ECB_encrypt(&ctx, mac);

        for (int i = 0; i < remlen; i++)
            data[coarselen+i] = rem[15-i];
    }

    ctx.Iv[13] = 0x00;
    ctx.Iv[14] = 0x00;
    ctx.Iv[15] = 0x00;
    AES_CTR_xcrypt_buffer(&ctx, mac, sizeof(mac));

    Bswap128(&data[len], mac);

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
    AES_CTR_xcrypt_buffer(&ctx, footer, sizeof(footer));

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

        Bswap128(tmp, &data[i]);

        AES_CTR_xcrypt_buffer(&ctx, tmp, sizeof(tmp));
        for (int i = 0; i < 16; i++) mac[i] ^= tmp[i];
        AES_ECB_encrypt(&ctx, mac);

        Bswap128(&data[i], tmp);
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

    Bswap128(footer, &data[len+0x10]);

    AES_ctx_set_iv(&ctx, iv);
    AES_CTR_xcrypt_buffer(&ctx, footer, sizeof(footer));

    data[len+0x10] = footer[15];
    data[len+0x1D] = footer[2];
    data[len+0x1E] = footer[1];
    data[len+0x1F] = footer[0];

    u32 footerlen = footer[0] | (footer[1] << 8) | (footer[2] << 16);
    if (footerlen != len)
    {
        Log(LogLevel::Error, "ESDecrypt: bad length %d (expected %d)\n", len, footerlen);
        return false;
    }

    for (int i = 0; i < 16; i++)
    {
        if (data[len+i] != mac[15-i])
        {
            Log(LogLevel::Warn, "ESDecrypt: bad MAC\n");
            return false;
        }
    }

    return true;
}


void ReadHardwareInfo(u8* dataS, u8* dataN)
{
    FF_FIL file;
    FRESULT res;
    u32 nread;

    res = f_open(&file, "0:/sys/HWINFO_S.dat", FA_OPEN_EXISTING | FA_READ);
    if (res == FR_OK)
    {
        f_read(&file, dataS, 0xA4, &nread);
        f_close(&file);
    }

    res = f_open(&file, "0:/sys/HWINFO_N.dat", FA_OPEN_EXISTING | FA_READ);
    if (res == FR_OK)
    {
        f_read(&file, dataN, 0x9C, &nread);
        f_close(&file);
    }
}


void ReadUserData(u8* data)
{
    FF_FIL file;
    FRESULT res;
    u32 nread;

    FF_FIL f1, f2;
    int v1, v2;

    res = f_open(&f1, "0:/shared1/TWLCFG0.dat", FA_OPEN_EXISTING | FA_READ);
    if (res != FR_OK)
        v1 = -1;
    else
    {
        u8 tmp;
        f_lseek(&f1, 0x81);
        f_read(&f1, &tmp, 1, &nread);
        v1 = tmp;
    }

    res = f_open(&f2, "0:/shared1/TWLCFG1.dat", FA_OPEN_EXISTING | FA_READ);
    if (res != FR_OK)
        v2 = -1;
    else
    {
        u8 tmp;
        f_lseek(&f2, 0x81);
        f_read(&f2, &tmp, 1, &nread);
        v2 = tmp;
    }

    if (v1 < 0 && v2 < 0) return;

    if (v2 > v1)
    {
        file = f2;
        f_close(&f1);
    }
    else
    {
        file = f1;
        f_close(&f2);
    }

    f_lseek(&file, 0);
    f_read(&file, data, 0x1B0, &nread);
    f_close(&file);
}

void PatchUserData()
{
    FRESULT res;

    for (int i = 0; i < 2; i++)
    {
        char filename[64];
        sprintf(filename, "0:/shared1/TWLCFG%d.dat", i);

        FF_FIL file;
        res = f_open(&file, filename, FA_OPEN_EXISTING | FA_READ | FA_WRITE);
        if (res != FR_OK)
        {
            Log(LogLevel::Error, "NAND: editing file %s failed: %d\n", filename, res);
            continue;
        }

        u8 contents[0x1B0];
        u32 nres;
        f_lseek(&file, 0);
        f_read(&file, contents, 0x1B0, &nres);

        // override user settings, if needed
        if (Platform::GetConfigBool(Platform::Firm_OverrideSettings))
        {
            // setting up username
            std::string orig_username = Platform::GetConfigString(Platform::Firm_Username);
            std::u16string username = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(orig_username);
            size_t usernameLength = std::min(username.length(), (size_t) 10);
            memset(contents + 0xD0, 0, 11 * sizeof(char16_t));
            memcpy(contents + 0xD0, username.data(), usernameLength * sizeof(char16_t));

            // setting language
            contents[0x8E] = Platform::GetConfigInt(Platform::Firm_Language);

            // setting up color
            contents[0xCC] = Platform::GetConfigInt(Platform::Firm_Color);

            // setting up birthday
            contents[0xCE] = Platform::GetConfigInt(Platform::Firm_BirthdayMonth);
            contents[0xCF] = Platform::GetConfigInt(Platform::Firm_BirthdayDay);

            // setup message
            std::string orig_message = Platform::GetConfigString(Platform::Firm_Message);
            std::u16string message = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.from_bytes(orig_message);
            size_t messageLength = std::min(message.length(), (size_t) 26);
            memset(contents + 0xE6, 0, 27 * sizeof(char16_t));
            memcpy(contents + 0xE6, message.data(), messageLength * sizeof(char16_t));

            // TODO: make other items configurable?
        }

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
}


void debug_listfiles(const char* path)
{
    FF_DIR dir;
    FF_FILINFO info;
    FRESULT res;

    res = f_opendir(&dir, path);
    if (res != FR_OK) return;

    for (;;)
    {
        res = f_readdir(&dir, &info);
        if (res != FR_OK) break;
        if (!info.fname[0]) break;

        char fullname[512];
        sprintf(fullname, "%s/%s", path, info.fname);
        Log(LogLevel::Debug, "[%c] %s\n", (info.fattrib&AM_DIR)?'D':'F', fullname);

        if (info.fattrib & AM_DIR)
        {
            debug_listfiles(fullname);
        }
    }

    f_closedir(&dir);
}

bool ImportFile(const char* path, const u8* data, size_t len)
{
    if (!data || !len || !path)
        return false;

    FF_FIL file;
    FRESULT res;

    res = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        return false;
    }

    u8 buf[0x1000];
    for (u32 i = 0; i < len; i += sizeof(buf))
    { // For each block in the file...
        u32 blocklen;
        if ((i + sizeof(buf)) > len)
            blocklen = len - i;
        else
            blocklen = sizeof(buf);

        u32 nwrite;
        memcpy(buf, data + i, blocklen);
        f_write(&file, buf, blocklen, &nwrite);
    }

    f_close(&file);
    Log(LogLevel::Debug, "Imported file from memory to %s\n", path);

    return true;
}

bool ImportFile(const char* path, const char* in)
{
    FF_FIL file;
    FILE* fin;
    FRESULT res;

    fin = fopen(in, "rb");
    if (!fin)
        return false;

    fseek(fin, 0, SEEK_END);
    u32 len = (u32)ftell(fin);
    fseek(fin, 0, SEEK_SET);

    res = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        fclose(fin);
        return false;
    }

    u8 buf[0x1000];
    for (u32 i = 0; i < len; i += sizeof(buf))
    {
        u32 blocklen;
        if ((i + sizeof(buf)) > len)
            blocklen = len - i;
        else
            blocklen = sizeof(buf);

        u32 nwrite;
        fread(buf, blocklen, 1, fin);
        f_write(&file, buf, blocklen, &nwrite);
    }

    fclose(fin);
    f_close(&file);

    Log(LogLevel::Debug, "Imported file from %s to %s\n", in, path);

    return true;
}

bool ExportFile(const char* path, const char* out)
{
    FF_FIL file;
    FILE* fout;
    FRESULT res;

    res = f_open(&file, path, FA_OPEN_EXISTING | FA_READ);
    if (res != FR_OK)
        return false;

    u32 len = f_size(&file);

    fout = fopen(out, "wb");
    if (!fout)
    {
        f_close(&file);
        return false;
    }

    u8 buf[0x1000];
    for (u32 i = 0; i < len; i += 0x1000)
    {
        u32 blocklen;
        if ((i + 0x1000) > len)
            blocklen = len - i;
        else
            blocklen = 0x1000;

        u32 nread;
        f_read(&file, buf, blocklen, &nread);
        fwrite(buf, blocklen, 1, fout);
    }

    fclose(fout);
    f_close(&file);

    Log(LogLevel::Debug, "Exported file from %s to %s\n", path, out);

    return true;
}

void RemoveFile(const char* path)
{
    FF_FILINFO info;
    FRESULT res = f_stat(path, &info);
    if (res != FR_OK) return;

    if (info.fattrib & AM_RDO)
        f_chmod(path, 0, AM_RDO);

    f_unlink(path);
    Log(LogLevel::Debug, "Removed file at %s\n", path);
}

void RemoveDir(const char* path)
{
    FF_DIR dir;
    FF_FILINFO info;
    FRESULT res;

    res = f_stat(path, &info);
    if (res != FR_OK) return;

    if (info.fattrib & AM_RDO)
        f_chmod(path, 0, AM_RDO);

    res = f_opendir(&dir, path);
    if (res != FR_OK) return;

    std::vector<std::string> dirlist;
    std::vector<std::string> filelist;

    for (;;)
    {
        res = f_readdir(&dir, &info);
        if (res != FR_OK) break;
        if (!info.fname[0]) break;

        char fullname[512];
        sprintf(fullname, "%s/%s", path, info.fname);

        if (info.fattrib & AM_RDO)
            f_chmod(path, 0, AM_RDO);

        if (info.fattrib & AM_DIR)
            dirlist.push_back(fullname);
        else
            filelist.push_back(fullname);
    }

    f_closedir(&dir);

    for (std::vector<std::string>::iterator it = dirlist.begin(); it != dirlist.end(); it++)
    {
        const char* path = (*it).c_str();
        RemoveDir(path);
    }

    for (std::vector<std::string>::iterator it = filelist.begin(); it != filelist.end(); it++)
    {
        const char* path = (*it).c_str();
        f_unlink(path);
    }

    f_unlink(path);
    Log(LogLevel::Debug, "Removed directory at %s\n", path);
}


u32 GetTitleVersion(u32 category, u32 titleid)
{
    FRESULT res;
    char path[256];
    sprintf(path, "0:/title/%08x/%08x/content/title.tmd", category, titleid);
    FF_FIL file;
    res = f_open(&file, path, FA_OPEN_EXISTING | FA_READ);
    if (res != FR_OK)
        return 0xFFFFFFFF;

    u32 version;
    u32 nread;
    f_lseek(&file, 0x1E4);
    f_read(&file, &version, 4, &nread);
    version = (version >> 24) | ((version & 0xFF0000) >> 8) | ((version & 0xFF00) << 8) | (version << 24);

    f_close(&file);
    return version;
}

void ListTitles(u32 category, std::vector<u32>& titlelist)
{
    FRESULT res;
    FF_DIR titledir;
    char path[256];

    sprintf(path, "0:/title/%08x", category);
    res = f_opendir(&titledir, path);
    if (res != FR_OK)
    {
        Log(LogLevel::Warn, "NAND: !! no title dir (%s)\n", path);
        return;
    }

    for (;;)
    {
        FF_FILINFO info;
        f_readdir(&titledir, &info);
        if (!info.fname[0])
            break;

        if (strlen(info.fname) != 8)
            continue;

        u32 titleid;
        if (sscanf(info.fname, "%08x", &titleid) < 1)
            continue;

        u32 version = GetTitleVersion(category, titleid);
        if (version == 0xFFFFFFFF)
            continue;

        sprintf(path, "0:/title/%08x/%08x/content/%08x.app", category, titleid, version);
        FF_FILINFO appinfo;
        res = f_stat(path, &appinfo);
        if (res != FR_OK)
            continue;
        if (appinfo.fattrib & AM_DIR)
            continue;
        if (appinfo.fsize < 0x4000)
            continue;

        // title is good, add it to the list
        titlelist.push_back(titleid);
    }

    f_closedir(&titledir);
}

bool TitleExists(u32 category, u32 titleid)
{
    char path[256];
    sprintf(path, "0:/title/%08x/%08x/content/title.tmd", category, titleid);

    FRESULT res = f_stat(path, nullptr);
    return (res == FR_OK);
}

void GetTitleInfo(u32 category, u32 titleid, u32& version, NDSHeader* header, NDSBanner* banner)
{
    version = GetTitleVersion(category, titleid);
    if (version == 0xFFFFFFFF)
        return;

    FRESULT res;

    char path[256];
    sprintf(path, "0:/title/%08x/%08x/content/%08x.app", category, titleid, version);
    FF_FIL file;
    res = f_open(&file, path, FA_OPEN_EXISTING | FA_READ);
    if (res != FR_OK)
        return;

    u32 nread;
    f_read(&file, header, sizeof(NDSHeader), &nread);

    if (banner)
    {
        u32 banneraddr = header->BannerOffset;
        if (!banneraddr)
        {
            memset(banner, 0, sizeof(NDSBanner));
        }
        else
        {
            f_lseek(&file, banneraddr);
            f_read(&file, banner, sizeof(NDSBanner), &nread);
        }
    }

    f_close(&file);
}


bool CreateTicket(const char* path, u32 titleid0, u32 titleid1, u8 version)
{
    FF_FIL file;
    FRESULT res;
    u32 nwrite;

    res = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        Log(LogLevel::Error, "CreateTicket: failed to create file (%d)\n", res);
        return false;
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

    return true;
}

bool CreateSaveFile(const char* path, u32 len)
{
    if (len == 0) return true;
    if (len < 0x200) return false;
    if (len > 0x8000000) return false;

    u32 clustersize, maxfiles, totsec16, fatsz16;

    // CHECKME!
    // code inspired from https://github.com/Epicpkmn11/NTM/blob/master/arm9/src/sav.c
    const u16 sectorsize = 0x200;

    // fit maximum sectors for the size
    const u16 maxsectors = len / sectorsize;
    u16 tracksize = 1;
    u16 headcount = 1;
    u16 totsec16next = 0;
    while (totsec16next <= maxsectors)
    {
        totsec16next = tracksize * (headcount + 1) * (headcount + 1);
        if (totsec16next <= maxsectors)
        {
            headcount++;
            totsec16 = totsec16next;

            tracksize++;
            totsec16next = tracksize * headcount * headcount;
            if (totsec16next <= maxsectors)
            {
                totsec16 = totsec16next;
            }
        }
    }
    totsec16next = (tracksize + 1) * headcount * headcount;
    if (totsec16next <= maxsectors)
    {
        tracksize++;
        totsec16 = totsec16next;
    }

    maxfiles = len < 0x8C000 ? 0x20 : 0x200;
    clustersize = (totsec16 > (8 << 10)) ? 8 : (totsec16 > (1 << 10) ? 4 : 1);

    #define ALIGN(v, a) (((v) % (a)) ? ((v) + (a) - ((v) % (a))) : (v))
    u16 totalclusters = ALIGN(totsec16, clustersize) / clustersize;
    u32 fatbytes = (ALIGN(totalclusters, 2) / 2) * 3; // 2 sectors -> 3 byte
    fatsz16 = ALIGN(fatbytes, sectorsize) / sectorsize;

    FF_FIL file;
    FRESULT res;
    u32 nwrite;

    res = f_open(&file, path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        Log(LogLevel::Error, "CreateSaveFile: failed to create file (%d)\n", res);
        return false;
    }

    u8* data = new u8[len];
    memset(data, 0, len);

    // create FAT header
    data[0x000] = 0xE9;
    memcpy(&data[0x003], "MSWIN4.1", 8);
    *(u16*)&data[0x00B] = sectorsize; // bytes per sector
    data[0x00D] = clustersize;
    *(u16*)&data[0x00E] = 1; // reserved sectors
    data[0x010] = 2; // num FATs
    *(u16*)&data[0x011] = maxfiles;
    *(u16*)&data[0x013] = totsec16;
    data[0x015] = 0xF8;
    *(u16*)&data[0x016] = fatsz16;
    *(u16*)&data[0x018] = tracksize;
    *(u16*)&data[0x01A] = headcount;
    data[0x024] = 0x05;
    data[0x026] = 0x29;
    *(u32*)&data[0x027] = 0x12345678;
    memcpy(&data[0x02B], "VOLUMELABEL", 11);
    memcpy(&data[0x036], "FAT12   ", 8);
    *(u16*)&data[0x1FE] = 0xAA55;

    f_write(&file, data, len, &nwrite);

    f_close(&file);
    delete[] data;

    return true;
}

bool InitTitleFileStructure(const NDSHeader& header, const DSi_TMD::TitleMetadata& tmd, bool readonly)
{
    u32 titleid0 = tmd.GetCategory();
    u32 titleid1 = tmd.GetID();
    FRESULT res;
    FF_DIR ticketdir;
    FF_FILINFO info;

    char fname[128];
    FF_FIL file;
    u32 nwrite;

    // ticket
    sprintf(fname, "0:/ticket/%08x", titleid0);
    f_mkdir(fname);

    sprintf(fname, "0:/ticket/%08x/%08x.tik", titleid0, titleid1);
    if (!CreateTicket(fname, tmd.GetCategoryNoByteswap(), tmd.GetIDNoByteswap(), header.ROMVersion))
        return false;

    if (readonly) f_chmod(fname, AM_RDO, AM_RDO);

    // folder

    sprintf(fname, "0:/title/%08x", titleid0);
    f_mkdir(fname);
    sprintf(fname, "0:/title/%08x/%08x", titleid0, titleid1);
    f_mkdir(fname);
    sprintf(fname, "0:/title/%08x/%08x/content", titleid0, titleid1);
    f_mkdir(fname);
    sprintf(fname, "0:/title/%08x/%08x/data", titleid0, titleid1);
    f_mkdir(fname);

    // data

    sprintf(fname, "0:/title/%08x/%08x/data/public.sav", titleid0, titleid1);
    if (!CreateSaveFile(fname, header.DSiPublicSavSize))
        return false;

    sprintf(fname, "0:/title/%08x/%08x/data/private.sav", titleid0, titleid1);
    if (!CreateSaveFile(fname, header.DSiPrivateSavSize))
        return false;

    if (header.AppFlags & 0x04)
    {
        // custom banner file
        sprintf(fname, "0:/title/%08x/%08x/data/banner.sav", titleid0, titleid1);
        res = f_open(&file, fname, FA_CREATE_ALWAYS | FA_WRITE);
        if (res != FR_OK)
        {
            Log(LogLevel::Error, "ImportTitle: failed to create banner.sav (%d)\n", res);
            return false;
        }

        u8 bannersav[0x4000];
        memset(bannersav, 0, sizeof(bannersav));
        f_write(&file, bannersav, sizeof(bannersav), &nwrite);

        f_close(&file);
    }

    // TMD

    sprintf(fname, "0:/title/%08x/%08x/content/title.tmd", titleid0, titleid1);
    res = f_open(&file, fname, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        Log(LogLevel::Error, "ImportTitle: failed to create TMD (%d)\n", res);
        return false;
    }

    f_write(&file, &tmd, sizeof(DSi_TMD::TitleMetadata), &nwrite);

    f_close(&file);

    if (readonly) f_chmod(fname, AM_RDO, AM_RDO);

    return true;
}

bool ImportTitle(const char* appfile, const DSi_TMD::TitleMetadata& tmd, bool readonly)
{
    NDSHeader header {};
    {
        FILE* f = fopen(appfile, "rb");
        if (!f) return false;
        fread(&header, sizeof(header), 1, f);
        fclose(f);
    }

    u32 version = tmd.Contents.GetVersion();
    Log(LogLevel::Info, ".app version: %08x\n", version);

    u32 titleid0 = tmd.GetCategory();
    u32 titleid1 = tmd.GetID();
    Log(LogLevel::Info, "Title ID: %08x/%08x\n", titleid0, titleid1);

    if (!InitTitleFileStructure(header, tmd, readonly))
    {
        Log(LogLevel::Error, "ImportTitle: failed to initialize file structure for imported title\n");
        return false;
    }

    // executable

    char fname[128];
    sprintf(fname, "0:/title/%08x/%08x/content/%08x.app", titleid0, titleid1, version);
    if (!ImportFile(fname, appfile))
    {
        Log(LogLevel::Error, "ImportTitle: failed to create executable\n");
        return false;
    }

    if (readonly) f_chmod(fname, AM_RDO, AM_RDO);

    return true;
}

bool ImportTitle(const u8* app, size_t appLength, const DSi_TMD::TitleMetadata& tmd, bool readonly)
{
    if (!app || appLength < sizeof(NDSHeader))
        return false;

    NDSHeader header {};
    memcpy(&header, app, sizeof(header));

    u32 version = tmd.Contents.GetVersion();
    Log(LogLevel::Info, ".app version: %08x\n", version);

    u32 titleid0 = tmd.GetCategory();
    u32 titleid1 = tmd.GetID();
    Log(LogLevel::Info, "Title ID: %08x/%08x\n", titleid0, titleid1);

    if (!InitTitleFileStructure(header, tmd, readonly))
    {
        Log(LogLevel::Error, "ImportTitle: failed to initialize file structure for imported title\n");
        return false;
    }

    // executable

    char fname[128];
    sprintf(fname, "0:/title/%08x/%08x/content/%08x.app", titleid0, titleid1, version);
    if (!ImportFile(fname, app, appLength))
    {
        Log(LogLevel::Error, "ImportTitle: failed to create executable\n");
        return false;
    }

    if (readonly) f_chmod(fname, AM_RDO, AM_RDO);

    return true;
}

void DeleteTitle(u32 category, u32 titleid)
{
    char fname[128];

    sprintf(fname, "0:/ticket/%08x/%08x.tik", category, titleid);
    RemoveFile(fname);

    sprintf(fname, "0:/title/%08x/%08x", category, titleid);
    RemoveDir(fname);
}

u32 GetTitleDataMask(u32 category, u32 titleid)
{
    u32 version;
    NDSHeader header;

    GetTitleInfo(category, titleid, version, &header, nullptr);
    if (version == 0xFFFFFFFF)
        return 0;

    u32 ret = 0;
    if (header.DSiPublicSavSize != 0)  ret |= (1 << TitleData_PublicSav);
    if (header.DSiPrivateSavSize != 0) ret |= (1 << TitleData_PrivateSav);
    if (header.AppFlags & 0x04)        ret |= (1 << TitleData_BannerSav);

    return ret;
}

bool ImportTitleData(u32 category, u32 titleid, int type, const char* file)
{
    char fname[128];

    switch (type)
    {
    case TitleData_PublicSav:
        sprintf(fname, "0:/title/%08x/%08x/data/public.sav", category, titleid);
        break;

    case TitleData_PrivateSav:
        sprintf(fname, "0:/title/%08x/%08x/data/private.sav", category, titleid);
        break;

    case TitleData_BannerSav:
        sprintf(fname, "0:/title/%08x/%08x/data/banner.sav", category, titleid);
        break;

    default:
        return false;
    }

    RemoveFile(fname);
    return ImportFile(fname, file);
}

bool ExportTitleData(u32 category, u32 titleid, int type, const char* file)
{
    char fname[128];

    switch (type)
    {
    case TitleData_PublicSav:
        sprintf(fname, "0:/title/%08x/%08x/data/public.sav", category, titleid);
        break;

    case TitleData_PrivateSav:
        sprintf(fname, "0:/title/%08x/%08x/data/private.sav", category, titleid);
        break;

    case TitleData_BannerSav:
        sprintf(fname, "0:/title/%08x/%08x/data/banner.sav", category, titleid);
        break;

    default:
        return false;
    }

    return ExportFile(fname, file);
}

}
