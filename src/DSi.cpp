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
#include <string.h>
#include <inttypes.h>
#include "NDS.h"
#include "DSi.h"
#include "ARM.h"
#include "GPU.h"
#include "NDSCart.h"
#include "SPI.h"
#include "DSi_SPI_TSC.h"
#include "Platform.h"

#ifdef JIT_ENABLED
#include "ARMJIT.h"
#include "ARMJIT_Memory.h"
#endif

#include "DSi_NDMA.h"
#include "DSi_I2C.h"
#include "DSi_SD.h"
#include "DSi_AES.h"
#include "DSi_NAND.h"
#include "DSi_DSP.h"
#include "DSi_Camera.h"

#include "tiny-AES-c/aes.hpp"

using namespace Platform;

namespace DSi
{

u16 SCFG_BIOS;
u16 SCFG_Clock9;
u16 SCFG_Clock7;
u32 SCFG_EXT[2];
u32 SCFG_MC;
u16 SCFG_RST;

u8 ARM9iBIOS[0x10000] = { 0 };
u8 ARM7iBIOS[0x10000] = { 0 };

u32 MBK[2][9];

u8* NWRAM_A;
u8* NWRAM_B;
u8* NWRAM_C;

u8* NWRAMMap_A[2][4];
u8* NWRAMMap_B[3][8];
u8* NWRAMMap_C[3][8];

u32 NWRAMStart[2][3];
u32 NWRAMEnd[2][3];
u32 NWRAMMask[2][3];

u32 NDMACnt[2];
DSi_NDMA* NDMAs[8];

std::unique_ptr<DSi_NAND::NANDImage> NANDImage;
DSi_SDHost* SDMMC;
DSi_SDHost* SDIO;

// FIXME: these currently have no effect (and aren't stored in a savestate)
//        ... not that they matter all that much
u8 GPIO_Data;
u8 GPIO_Dir;
u8 GPIO_IEdgeSel;
u8 GPIO_IE;
u8 GPIO_WiFi;


void Set_SCFG_Clock9(u16 val);
void Set_SCFG_MC(u32 val);


bool Init()
{
#ifndef JIT_ENABLED
    NWRAM_A = new u8[NWRAMSize];
    NWRAM_B = new u8[NWRAMSize];
    NWRAM_C = new u8[NWRAMSize];
#endif

    if (!DSi_I2C::Init()) return false;
    if (!DSi_CamModule::Init()) return false;
    if (!DSi_AES::Init()) return false;
    if (!DSi_DSP::Init()) return false;

    NDMAs[0] = new DSi_NDMA(0, 0);
    NDMAs[1] = new DSi_NDMA(0, 1);
    NDMAs[2] = new DSi_NDMA(0, 2);
    NDMAs[3] = new DSi_NDMA(0, 3);
    NDMAs[4] = new DSi_NDMA(1, 0);
    NDMAs[5] = new DSi_NDMA(1, 1);
    NDMAs[6] = new DSi_NDMA(1, 2);
    NDMAs[7] = new DSi_NDMA(1, 3);

    SDMMC = new DSi_SDHost(0);
    SDIO = new DSi_SDHost(1);

    return true;
}

void DeInit()
{
#ifndef JIT_ENABLED
    delete[] NWRAM_A;
    delete[] NWRAM_B;
    delete[] NWRAM_C;

    NWRAM_A = nullptr;
    NWRAM_B = nullptr;
    NWRAM_C = nullptr;
#endif

    DSi_I2C::DeInit();
    DSi_CamModule::DeInit();
    DSi_AES::DeInit();
    DSi_DSP::DeInit();

    for (int i = 0; i < 8; i++)
    {
        delete NDMAs[i];
        NDMAs[i] = nullptr;
    }

    delete SDMMC;
    SDMMC = nullptr;
    delete SDIO;
    SDIO = nullptr;

    NANDImage = nullptr;
    // The NANDImage is cleaned up (and its underlying file closed)
    // as part of unique_ptr's destructor
}

void Reset()
{
    //NDS::ARM9->CP15Write(0x910, 0x0D00000A);
    //NDS::ARM9->CP15Write(0x911, 0x00000020);
    //NDS::ARM9->CP15Write(0x100, NDS::ARM9->CP15Read(0x100) | 0x00050000);

    NDS::MapSharedWRAM(3);

    NDMACnt[0] = 0; NDMACnt[1] = 0;
    for (int i = 0; i < 8; i++) NDMAs[i]->Reset();

    DSi_I2C::Reset();
    DSi_CamModule::Reset();
    DSi_DSP::Reset();

    SDMMC->CloseHandles();
    SDIO->CloseHandles();

    LoadNAND();

    SDMMC->Reset();
    SDIO->Reset();

    DSi_AES::Reset();

    if (Platform::GetConfigBool(Platform::DSi_FullBIOSBoot))
    {
        SCFG_BIOS = 0x0000;
    }
    else
    {
        SCFG_BIOS = 0x0101;
    }
    SCFG_Clock9 = 0x0187; // CHECKME
    SCFG_Clock7 = 0x0187;
    SCFG_EXT[0] = 0x8307F100;
    SCFG_EXT[1] = 0x93FFFB06;
    SCFG_MC = 0x0010 | (~((u32)(NDSCart::Cart != nullptr))&1);//0x0011;
    SCFG_RST = 0;

    DSi_DSP::SetRstLine(false);

    GPIO_Data = 0xff; // these actually initialize to high after reset
    GPIO_Dir = 0x80; // enable sound out, all others input
    GPIO_IEdgeSel = 0;
    GPIO_IE = 0;
    GPIO_WiFi = 0;

    // LCD init flag
    GPU::DispStat[0] |= (1<<6);
    GPU::DispStat[1] |= (1<<6);
}

void Stop()
{
    DSi_CamModule::Stop();
}

void DoSavestate(Savestate* file)
{
    file->Section("DSIG");

    file->Var16(&SCFG_BIOS);
    file->Var16(&SCFG_Clock9);
    file->Var16(&SCFG_Clock7);
    file->VarArray(&SCFG_EXT[0], sizeof(u32)*2);
    file->Var32(&SCFG_MC);
    file->Var16(&SCFG_RST);

    //file->VarArray(ARM9iBIOS, 0x10000);
    //file->VarArray(ARM7iBIOS, 0x10000);

    if (file->Saving)
    {
        file->VarArray(&MBK[0][0], sizeof(u32)*8);
        file->VarArray(&MBK[1][5], sizeof(u32)*3);
        file->Var32(&MBK[0][8]);
    }
    else
    {
        Set_SCFG_Clock9(SCFG_Clock9);
        Set_SCFG_MC(SCFG_MC);
        DSi_DSP::SetRstLine(SCFG_RST & 0x0001);

        MBK[0][8] = 0;
        MBK[1][8] = 0;

        u32 mbk[12];
        file->VarArray(&mbk, sizeof(u32)*12);

        MapNWRAM_A(0, mbk[0] & 0xFF);
        MapNWRAM_A(1, (mbk[0] >> 8) & 0xFF);
        MapNWRAM_A(2, (mbk[0] >> 16) & 0xFF);
        MapNWRAM_A(3, mbk[0] >> 24);

        MapNWRAM_B(0, mbk[1] & 0xFF);
        MapNWRAM_B(1, (mbk[1] >> 8) & 0xFF);
        MapNWRAM_B(2, (mbk[1] >> 16) & 0xFF);
        MapNWRAM_B(3, mbk[1] >> 24);
        MapNWRAM_B(4, mbk[2] & 0xFF);
        MapNWRAM_B(5, (mbk[2] >> 8) & 0xFF);
        MapNWRAM_B(6, (mbk[2] >> 16) & 0xFF);
        MapNWRAM_B(7, mbk[2] >> 24);

        MapNWRAM_C(0, mbk[3] & 0xFF);
        MapNWRAM_C(1, (mbk[3] >> 8) & 0xFF);
        MapNWRAM_C(2, (mbk[3] >> 16) & 0xFF);
        MapNWRAM_C(3, mbk[3] >> 24);
        MapNWRAM_C(4, mbk[4] & 0xFF);
        MapNWRAM_C(5, (mbk[4] >> 8) & 0xFF);
        MapNWRAM_C(6, (mbk[4] >> 16) & 0xFF);
        MapNWRAM_C(7, mbk[4] >> 24);

        MapNWRAMRange(0, 0, mbk[5]);
        MapNWRAMRange(0, 1, mbk[6]);
        MapNWRAMRange(0, 2, mbk[7]);

        MapNWRAMRange(1, 0, mbk[8]);
        MapNWRAMRange(1, 1, mbk[9]);
        MapNWRAMRange(1, 2, mbk[10]);

        mbk[11] &= 0x00FFFF0F;
        MBK[0][8] = mbk[11];
        MBK[1][8] = mbk[11];
    }

    for (int i = 0; i < 8; i++)
        NDMAs[i]->DoSavestate(file);

    DSi_AES::DoSavestate(file);
    DSi_CamModule::DoSavestate(file);
    DSi_DSP::DoSavestate(file);
    DSi_I2C::DoSavestate(file);
    SDMMC->DoSavestate(file);
    SDIO->DoSavestate(file);
}

void SetCartInserted(bool inserted)
{
    if (inserted)
        SCFG_MC &= ~1;
    else
        SCFG_MC |= 1;
}

void DecryptModcryptArea(u32 offset, u32 size, u8* iv)
{
    AES_ctx ctx;
    u8 key[16];
    u8 tmp[16];

    if ((offset == 0) || (size == 0))
        return;

    const NDSHeader& header = NDSCart::Cart->GetHeader();

    if ((header.DSiCryptoFlags & (1<<4)) ||
        (header.AppFlags & (1<<7)))
    {
        // dev key
        const u8* cartrom = NDSCart::Cart->GetROM();
        memcpy(key, &cartrom[0], 16);
    }
    else
    {
        u8 keyX[16], keyY[16];

        *(u32*)&keyX[0] = 0x746E694E;
        *(u32*)&keyX[4] = 0x6F646E65;
        keyX[8]  = header.GameCode[0];
        keyX[9]  = header.GameCode[1];
        keyX[10] = header.GameCode[2];
        keyX[11] = header.GameCode[3];
        keyX[12] = header.GameCode[3];
        keyX[13] = header.GameCode[2];
        keyX[14] = header.GameCode[1];
        keyX[15] = header.GameCode[0];

        memcpy(keyY, header.DSiARM9iHash, 16);

        DSi_AES::DeriveNormalKey(keyX, keyY, tmp);
    }

    Bswap128(key, tmp);
    Bswap128(tmp, iv);
    AES_init_ctx_iv(&ctx, key, tmp);

    // find a matching binary area

    u32 binaryaddr, binarysize;
    u32 roundedsize = (size + 0xF) & ~0xF;

    // CHECKME: GBAtek says the modcrypt area should be the same size, or bigger,
    // than the binary area being considered
    // but I have seen modcrypt areas smaller than the ARM9i binary
#define BINARY_GOOD(name) \
    ((offset >= header.name##ROMOffset) && \
     (offset+roundedsize) <= (header.name##ROMOffset + ((header.name##Size + 0xF) & ~0xF)))

    if (BINARY_GOOD(ARM9))
    {
        binaryaddr = header.ARM9RAMAddress;
        binarysize = header.ARM9Size;
    }
    else if (BINARY_GOOD(ARM7))
    {
        binaryaddr = header.ARM7RAMAddress;
        binarysize = header.ARM7Size;
    }
    else if (BINARY_GOOD(DSiARM9i))
    {
        binaryaddr = header.DSiARM9iRAMAddress;
        binarysize = header.DSiARM9iSize;
    }
    else if (BINARY_GOOD(DSiARM7i))
    {
        binaryaddr = header.DSiARM7iRAMAddress;
        binarysize = header.DSiARM7iSize;
    }
    else
        return;

#undef BINARY_GOOD

    for (u32 i = 0; i < size; i+=16)
    {
        u32 data[4];

        data[0] = ARM9Read32(binaryaddr+i);
        data[1] = ARM9Read32(binaryaddr+i+4);
        data[2] = ARM9Read32(binaryaddr+i+8);
        data[3] = ARM9Read32(binaryaddr+i+12);

        Bswap128(tmp, data);
        AES_CTR_xcrypt_buffer(&ctx, tmp, sizeof(tmp));
        Bswap128(data, tmp);

        ARM9Write32(binaryaddr+i,    data[0]);
        ARM9Write32(binaryaddr+i+4,  data[1]);
        ARM9Write32(binaryaddr+i+8,  data[2]);
        ARM9Write32(binaryaddr+i+12, data[3]);
    }
}

void SetupDirectBoot()
{
    bool dsmode = false;
    NDSHeader& header = NDSCart::Cart->GetHeader();
    const u8* cartrom = NDSCart::Cart->GetROM();
    u32 cartid = NDSCart::Cart->ID();

    // TODO: add controls for forcing DS or DSi mode?
    if (!(header.UnitCode & 0x02))
        dsmode = true;

    if (dsmode)
    {
        SCFG_BIOS = 0x0303;

        // no NWRAM Mapping
        for (int i = 0; i < 4; i++)
            MapNWRAM_A(i, 0);
        for (int i = 0; i < 8; i++)
            MapNWRAM_B(i, 0);
        for (int i = 0; i < 8; i++)
            MapNWRAM_C(i, 0);

        // No NWRAM Window
        for (int i = 0; i < 3; i++)
        {
            MapNWRAMRange(0, i, 0);
            MapNWRAMRange(1, i, 0);
        }

        NDS::MapSharedWRAM(3);

        DSi_SPI_TSC::SetMode(0x00);
        Set_SCFG_Clock9(0x0000);
    }
    else
    {
        SCFG_BIOS = 0x0101;

        // WRAM mapping

        MBK[0][8] = 0;
        MBK[1][8] = 0;

        u32 mbk[12];
        memcpy(mbk, &cartrom[0x180], 12*4);

        MapNWRAM_A(0, mbk[0] & 0xFF);
        MapNWRAM_A(1, (mbk[0] >> 8) & 0xFF);
        MapNWRAM_A(2, (mbk[0] >> 16) & 0xFF);
        MapNWRAM_A(3, mbk[0] >> 24);

        MapNWRAM_B(0, mbk[1] & 0xFF);
        MapNWRAM_B(1, (mbk[1] >> 8) & 0xFF);
        MapNWRAM_B(2, (mbk[1] >> 16) & 0xFF);
        MapNWRAM_B(3, mbk[1] >> 24);
        MapNWRAM_B(4, mbk[2] & 0xFF);
        MapNWRAM_B(5, (mbk[2] >> 8) & 0xFF);
        MapNWRAM_B(6, (mbk[2] >> 16) & 0xFF);
        MapNWRAM_B(7, mbk[2] >> 24);

        MapNWRAM_C(0, mbk[3] & 0xFF);
        MapNWRAM_C(1, (mbk[3] >> 8) & 0xFF);
        MapNWRAM_C(2, (mbk[3] >> 16) & 0xFF);
        MapNWRAM_C(3, mbk[3] >> 24);
        MapNWRAM_C(4, mbk[4] & 0xFF);
        MapNWRAM_C(5, (mbk[4] >> 8) & 0xFF);
        MapNWRAM_C(6, (mbk[4] >> 16) & 0xFF);
        MapNWRAM_C(7, mbk[4] >> 24);

        MapNWRAMRange(0, 0, mbk[5]);
        MapNWRAMRange(0, 1, mbk[6]);
        MapNWRAMRange(0, 2, mbk[7]);

        MapNWRAMRange(1, 0, mbk[8]);
        MapNWRAMRange(1, 1, mbk[9]);
        MapNWRAMRange(1, 2, mbk[10]);

        MBK[0][8] = mbk[11] & 0x00FFFF0F;
        MBK[1][8] = mbk[11] & 0x00FFFF0F;

        NDS::MapSharedWRAM(mbk[11] >> 24);

        if (!(header.AppFlags & (1<<0)))
            DSi_SPI_TSC::SetMode(0x00);
    }

    // setup main RAM data
    // TODO: verify what changes when loading a DS-mode ROM

    if (dsmode)
    {
        for (u32 i = 0; i < 0x170; i+=4)
        {
            u32 tmp = *(u32*)&cartrom[i];
            ARM9Write32(0x027FFE00+i, tmp);
        }

        ARM9Write32(0x027FF800, cartid);
        ARM9Write32(0x027FF804, cartid);
        ARM9Write16(0x027FF808, header.HeaderCRC16);
        ARM9Write16(0x027FF80A, header.SecureAreaCRC16);

        ARM9Write16(0x027FF850, 0x5835);

        ARM9Write32(0x027FFC00, cartid);
        ARM9Write32(0x027FFC04, cartid);
        ARM9Write16(0x027FFC08, header.HeaderCRC16);
        ARM9Write16(0x027FFC0A, header.SecureAreaCRC16);

        ARM9Write16(0x027FFC10, 0x5835);
        ARM9Write16(0x027FFC30, 0xFFFF);
        ARM9Write16(0x027FFC40, 0x0001);
    }
    else
    {
        // CHECKME: some of these are 'only for NDS ROM', presumably
        // only for when loading a cart? (as opposed to DSiWare)

        for (u32 i = 0; i < 0x160; i+=4)
        {
            u32 tmp = *(u32*)&cartrom[i];
            ARM9Write32(0x02FFFA80+i, tmp);
            ARM9Write32(0x02FFFE00+i, tmp);
        }

        for (u32 i = 0; i < 0x1000; i+=4)
        {
            u32 tmp = *(u32*)&cartrom[i];
            ARM9Write32(0x02FFC000+i, tmp);
            ARM9Write32(0x02FFE000+i, tmp);
        }

        if (NANDImage && *NANDImage)
        { // If a NAND image is installed, and it's valid...
            if (DSi_NAND::NANDMount nand = DSi_NAND::NANDMount(*NANDImage))
            {
                DSi_NAND::DSiFirmwareSystemSettings userdata {};
                nand.ReadUserData(userdata);
                for (u32 i = 0; i < 0x128; i+=4)
                    ARM9Write32(0x02000400+i, *(u32*)&userdata.Bytes[0x88+i]);

                DSi_NAND::DSiSerialData hwinfoS {};
                nand.ReadSerialData(hwinfoS);
                DSi_NAND::DSiHardwareInfoN hwinfoN;
                nand.ReadHardwareInfoN(hwinfoN);

                for (u32 i = 0; i < 0x14; i+=4)
                    ARM9Write32(0x02000600+i, *(u32*)&hwinfoN[0x88+i]);

                for (u32 i = 0; i < 0x18; i+=4)
                    ARM9Write32(0x02FFFD68+i, *(u32*)&hwinfoS.Bytes[0x88+i]);
            }
        }

        SPI_Firmware::WifiBoard nwifiver = SPI_Firmware::GetFirmware()->Header().WifiBoard;
        ARM9Write8(0x020005E0, static_cast<u8>(nwifiver));

        // TODO: these should be taken from the wifi firmware in NAND
        // but, hey, this works too.
        if (nwifiver == SPI_Firmware::WifiBoard::W015)
        {
            ARM9Write16(0x020005E2, 0xB57E);
            ARM9Write32(0x020005E4, 0x00500400);
            ARM9Write32(0x020005E8, 0x00500000);
            ARM9Write32(0x020005EC, 0x0002E000);
        }
        else
        {
            ARM9Write16(0x020005E2, 0x5BCA);
            ARM9Write32(0x020005E4, 0x00520000);
            ARM9Write32(0x020005E8, 0x00520000);
            ARM9Write32(0x020005EC, 0x00020000);
        }

        // TODO: the shit at 02FFD7B0..02FFDC00
        // and some other extra shit?

        ARM9Write32(0x02FFFC00, cartid);
        ARM9Write16(0x02FFFC40, 0x0001); // boot indicator

        ARM9Write8(0x02FFFDFA, DSi_BPTWL::GetBootFlag() | 0x80);
        ARM9Write8(0x02FFFDFB, 0x01);
    }

    // TODO: for DS-mode ROMs, switch RAM size here

    u32 arm9start = 0;

    // load the ARM9 secure area
    if (header.ARM9ROMOffset >= 0x4000 && header.ARM9ROMOffset < 0x8000)
    {
        u8 securearea[0x800];
        NDSCart::DecryptSecureArea(securearea);

        for (u32 i = 0; i < 0x800; i+=4)
        {
            ARM9Write32(header.ARM9RAMAddress+i, *(u32*)&securearea[i]);
            arm9start += 4;
        }
    }

    for (u32 i = arm9start; i < header.ARM9Size; i+=4)
    {
        u32 tmp = *(u32*)&cartrom[header.ARM9ROMOffset+i];
        ARM9Write32(header.ARM9RAMAddress+i, tmp);
    }

    for (u32 i = 0; i < header.ARM7Size; i+=4)
    {
        u32 tmp = *(u32*)&cartrom[header.ARM7ROMOffset+i];
        ARM7Write32(header.ARM7RAMAddress+i, tmp);
    }

    if ((!dsmode) && (header.DSiCryptoFlags & (1<<0)))
    {
        // load DSi-specific regions

        for (u32 i = 0; i < header.DSiARM9iSize; i+=4)
        {
            u32 tmp = *(u32*)&cartrom[header.DSiARM9iROMOffset+i];
            ARM9Write32(header.DSiARM9iRAMAddress+i, tmp);
        }

        for (u32 i = 0; i < header.DSiARM7iSize; i+=4)
        {
            u32 tmp = *(u32*)&cartrom[header.DSiARM7iROMOffset+i];
            ARM7Write32(header.DSiARM7iRAMAddress+i, tmp);
        }

        // decrypt any modcrypt areas

        if (header.DSiCryptoFlags & (1<<1))
        {
            DecryptModcryptArea(header.DSiModcrypt1Offset,
                                header.DSiModcrypt1Size,
                                header.DSiARM9Hash);
            DecryptModcryptArea(header.DSiModcrypt2Offset,
                                header.DSiModcrypt2Size,
                                header.DSiARM7Hash);
        }
    }

    NDS::ARM7BIOSProt = 0x20;

    SPI_Firmware::SetupDirectBoot(true);

    NDS::ARM9->CP15Write(0x100, 0x00056078);
    NDS::ARM9->CP15Write(0x200, 0x0000004A);
    NDS::ARM9->CP15Write(0x201, 0x0000004A);
    NDS::ARM9->CP15Write(0x300, 0x0000000A);
    NDS::ARM9->CP15Write(0x502, 0x15111011);
    NDS::ARM9->CP15Write(0x503, 0x05101011);
    NDS::ARM9->CP15Write(0x600, 0x04000033);
    NDS::ARM9->CP15Write(0x601, 0x04000033);
    NDS::ARM9->CP15Write(0x610, 0x02000031);
    NDS::ARM9->CP15Write(0x611, 0x02000031);
    NDS::ARM9->CP15Write(0x620, 0x00000000);
    NDS::ARM9->CP15Write(0x621, 0x00000000);
    NDS::ARM9->CP15Write(0x630, 0x08000033);
    NDS::ARM9->CP15Write(0x631, 0x08000033);
    NDS::ARM9->CP15Write(0x640, 0x0E00001B);
    NDS::ARM9->CP15Write(0x641, 0x0E00001B);
    NDS::ARM9->CP15Write(0x650, 0x00000000);
    NDS::ARM9->CP15Write(0x651, 0x00000000);
    NDS::ARM9->CP15Write(0x660, 0xFFFF001D);
    NDS::ARM9->CP15Write(0x661, 0xFFFF001D);
    NDS::ARM9->CP15Write(0x670, 0x02FFC01B);
    NDS::ARM9->CP15Write(0x671, 0x02FFC01B);
    NDS::ARM9->CP15Write(0x910, 0x0E00000A);
    NDS::ARM9->CP15Write(0x911, 0x00000020);
}

void SoftReset()
{
    // TODO: check exactly what is reset
    // presumably, main RAM isn't reset, since the DSi can be told
    // to boot a specific title this way
    // BPTWL state wouldn't be reset either since BPTWL[0x70] is
    // the warmboot flag

    // also, BPTWL[0x70] could be abused to quickly boot specific titles

#ifdef JIT_ENABLED
    ARMJIT_Memory::Reset();
    ARMJIT::CheckAndInvalidateITCM();
#endif

    NDS::ARM9->Reset();
    NDS::ARM7->Reset();

    NDS::ARM9->CP15Reset();

    NDS::MapSharedWRAM(3);

    // TODO: does the DSP get reset? NWRAM doesn't, so I'm assuming no
    // *HOWEVER*, the bootrom (which does get rerun) does remap NWRAM, and thus
    // the DSP most likely gets reset
    DSi_DSP::Reset();

    SDMMC->CloseHandles();
    SDIO->CloseHandles();

    LoadNAND();

    SDMMC->Reset();
    SDIO->Reset();

    DSi_AES::Reset();

    if (Platform::GetConfigBool(Platform::DSi_FullBIOSBoot))
    {
        SCFG_BIOS = 0x0000;
    }
    else
    {
        SCFG_BIOS = 0x0101;
    }
    SCFG_Clock9 = 0x0187; // CHECKME
    SCFG_Clock7 = 0x0187;
    SCFG_EXT[0] = 0x8307F100;
    SCFG_EXT[1] = 0x93FFFB06;
    SCFG_MC = 0x0010;//0x0011;
    // TODO: is this actually reset?
    SCFG_RST = 0;
    DSi_DSP::SetRstLine(false);


    // LCD init flag
    GPU::DispStat[0] |= (1<<6);
    GPU::DispStat[1] |= (1<<6);
}

bool LoadNAND()
{
    if (!NANDImage)
    {
        Log(LogLevel::Error, "No NAND image loaded\n");
        return false;
    }
    Log(LogLevel::Info, "Loading DSi NAND\n");

    DSi_NAND::NANDMount nandmount(*NANDImage);
    if (!nandmount)
    {
        Log(LogLevel::Error, "Failed to load DSi NAND\n");
        return false;
    }

    FileHandle* nand = NANDImage->GetFile();

    // Make sure NWRAM is accessible.
    // The Bits are set to the startup values in Reset() and we might
    // still have them on default (0) or some bits cleared by the previous
    // run.
    SCFG_EXT[0] |= (1 << 25);
    SCFG_EXT[1] |= (1 << 25);

    memset(NWRAM_A, 0, NWRAMSize);
    memset(NWRAM_B, 0, NWRAMSize);
    memset(NWRAM_C, 0, NWRAMSize);

    memset(MBK, 0, sizeof(MBK));
    memset(NWRAMMap_A, 0, sizeof(NWRAMMap_A));
    memset(NWRAMMap_B, 0, sizeof(NWRAMMap_B));
    memset(NWRAMMap_C, 0, sizeof(NWRAMMap_C));
    memset(NWRAMStart, 0, sizeof(NWRAMStart));
    memset(NWRAMEnd, 0, sizeof(NWRAMEnd));
    memset(NWRAMMask, 0, sizeof(NWRAMMask));

    u32 bootparams[8];
    if (Platform::GetConfigBool(Platform::DSi_FullBIOSBoot))
    {
        // TODO: figure out default MBK mapping
        // MBK1..5: disable mappings
        for (int i = 0; i < 8; ++i)
        {
            if (i < 4)
                MapNWRAM_A(i, 0);
            MapNWRAM_B(i, 0);
            MapNWRAM_C(i, 0);
        }
        // MBK6..8: address mappings: nothing mapped
        for (int i = 0; i < 6; ++i)
        {
            MapNWRAMRange(i & 1, i >> 1, 0);
        }
        // MBK9: ARM9 allowed to write
        MBK[0][8] = 0;
        MBK[1][8] = 0;
    }
    else
    {
        FileSeek(nand, 0x220, FileSeekOrigin::Start);
        FileRead(bootparams, 4, 8, nand);

        Log(LogLevel::Debug, "ARM9: offset=%08X size=%08X RAM=%08X size_aligned=%08X\n",
               bootparams[0], bootparams[1], bootparams[2], bootparams[3]);
        Log(LogLevel::Debug, "ARM7: offset=%08X size=%08X RAM=%08X size_aligned=%08X\n",
               bootparams[4], bootparams[5], bootparams[6], bootparams[7]);

        // read and apply new-WRAM settings

        MBK[0][8] = 0;
        MBK[1][8] = 0;

        u32 mbk[12];
        FileSeek(nand, 0x380, FileSeekOrigin::Start);
        FileRead(mbk, 4, 12, nand);

        MapNWRAM_A(0, mbk[0] & 0xFF);
        MapNWRAM_A(1, (mbk[0] >> 8) & 0xFF);
        MapNWRAM_A(2, (mbk[0] >> 16) & 0xFF);
        MapNWRAM_A(3, mbk[0] >> 24);

        MapNWRAM_B(0, mbk[1] & 0xFF);
        MapNWRAM_B(1, (mbk[1] >> 8) & 0xFF);
        MapNWRAM_B(2, (mbk[1] >> 16) & 0xFF);
        MapNWRAM_B(3, mbk[1] >> 24);
        MapNWRAM_B(4, mbk[2] & 0xFF);
        MapNWRAM_B(5, (mbk[2] >> 8) & 0xFF);
        MapNWRAM_B(6, (mbk[2] >> 16) & 0xFF);
        MapNWRAM_B(7, mbk[2] >> 24);

        MapNWRAM_C(0, mbk[3] & 0xFF);
        MapNWRAM_C(1, (mbk[3] >> 8) & 0xFF);
        MapNWRAM_C(2, (mbk[3] >> 16) & 0xFF);
        MapNWRAM_C(3, mbk[3] >> 24);
        MapNWRAM_C(4, mbk[4] & 0xFF);
        MapNWRAM_C(5, (mbk[4] >> 8) & 0xFF);
        MapNWRAM_C(6, (mbk[4] >> 16) & 0xFF);
        MapNWRAM_C(7, mbk[4] >> 24);

        MapNWRAMRange(0, 0, mbk[5]);
        MapNWRAMRange(0, 1, mbk[6]);
        MapNWRAMRange(0, 2, mbk[7]);

        MapNWRAMRange(1, 0, mbk[8]);
        MapNWRAMRange(1, 1, mbk[9]);
        MapNWRAMRange(1, 2, mbk[10]);

        // TODO: find out why it is 0xFF000000
        mbk[11] &= 0x00FFFF0F;
        MBK[0][8] = mbk[11];
        MBK[1][8] = mbk[11];

        // load boot2 binaries

        AES_ctx ctx;
        const u8 boot2key[16] = {0xAD, 0x34, 0xEC, 0xF9, 0x62, 0x6E, 0xC2, 0x3A, 0xF6, 0xB4, 0x6C, 0x00, 0x80, 0x80, 0xEE, 0x98};
        u8 boot2iv[16];
        u8 tmp[16];
        u32 dstaddr;

        *(u32*)&tmp[0] = bootparams[3];
        *(u32*)&tmp[4] = -bootparams[3];
        *(u32*)&tmp[8] = ~bootparams[3];
        *(u32*)&tmp[12] = 0;
        Bswap128(boot2iv, tmp);

        AES_init_ctx_iv(&ctx, boot2key, boot2iv);

        FileSeek(nand, bootparams[0], FileSeekOrigin::Start);
        dstaddr = bootparams[2];
        for (u32 i = 0; i < bootparams[3]; i += 16)
        {
            u32 data[4];
            FileRead(data, 16, 1, nand);

            Bswap128(tmp, data);
            AES_CTR_xcrypt_buffer(&ctx, tmp, 16);
            Bswap128(data, tmp);

            ARM9Write32(dstaddr, data[0]); dstaddr += 4;
            ARM9Write32(dstaddr, data[1]); dstaddr += 4;
            ARM9Write32(dstaddr, data[2]); dstaddr += 4;
            ARM9Write32(dstaddr, data[3]); dstaddr += 4;
        }

        *(u32*)&tmp[0] = bootparams[7];
        *(u32*)&tmp[4] = -bootparams[7];
        *(u32*)&tmp[8] = ~bootparams[7];
        *(u32*)&tmp[12] = 0;
        Bswap128(boot2iv, tmp);

        AES_init_ctx_iv(&ctx, boot2key, boot2iv);

        FileSeek(nand, bootparams[4], FileSeekOrigin::Start);
        dstaddr = bootparams[6];
        for (u32 i = 0; i < bootparams[7]; i += 16)
        {
            u32 data[4];
            FileRead(data, 16, 1, nand);

            Bswap128(tmp, data);
            AES_CTR_xcrypt_buffer(&ctx, tmp, 16);
            Bswap128(data, tmp);

            ARM7Write32(dstaddr, data[0]); dstaddr += 4;
            ARM7Write32(dstaddr, data[1]); dstaddr += 4;
            ARM7Write32(dstaddr, data[2]); dstaddr += 4;
            ARM7Write32(dstaddr, data[3]); dstaddr += 4;
        }
    }

    const DSi_NAND::DSiKey& emmccid = NANDImage->GetEMMCID();
    Log(LogLevel::Debug, "eMMC CID: %08llX%08llX\n", *(const u64*)&emmccid[0], *(const u64*)&emmccid[8]);
    Log(LogLevel::Debug, "Console ID: %" PRIx64 "\n", NANDImage->GetConsoleID());

    if (Platform::GetConfigBool(Platform::DSi_FullBIOSBoot))
    {
        // point CPUs to boot ROM reset vectors
        NDS::ARM9->JumpTo(0xFFFF0000);
        NDS::ARM7->JumpTo(0x00000000);
    }
    else
    {
        u32 eaddr = 0x03FFE6E4;
        ARM7Write32(eaddr+0x00, *(const u32*)&emmccid[0]);
        ARM7Write32(eaddr+0x04, *(const u32*)&emmccid[4]);
        ARM7Write32(eaddr+0x08, *(const u32*)&emmccid[8]);
        ARM7Write32(eaddr+0x0C, *(const u32*)&emmccid[12]);
        ARM7Write16(eaddr+0x2C, 0x0001);
        ARM7Write16(eaddr+0x2E, 0x0001);
        ARM7Write16(eaddr+0x3C, 0x0100);
        ARM7Write16(eaddr+0x3E, 0x40E0);
        ARM7Write16(eaddr+0x42, 0x0001);

        memcpy(&NDS::ARM9->ITCM[0x4400], &ARM9iBIOS[0x87F4], 0x400);
        memcpy(&NDS::ARM9->ITCM[0x4800], &ARM9iBIOS[0x9920], 0x80);
        memcpy(&NDS::ARM9->ITCM[0x4894], &ARM9iBIOS[0x99A0], 0x1048);
        memcpy(&NDS::ARM9->ITCM[0x58DC], &ARM9iBIOS[0xA9E8], 0x1048);

        u8 ARM7Init[0x3C00];
        memset(ARM7Init, 0, 0x3C00);
        memcpy(&ARM7Init[0x0000], &ARM7iBIOS[0x8188], 0x200);
        memcpy(&ARM7Init[0x0200], &ARM7iBIOS[0xB5D8], 0x40);
        memcpy(&ARM7Init[0x0254], &ARM7iBIOS[0xC6D0], 0x1048);
        memcpy(&ARM7Init[0x129C], &ARM7iBIOS[0xD718], 0x1048);

        for (u32 i = 0; i < 0x3C00; i+=4)
            ARM7Write32(0x03FFC400+i, *(u32*)&ARM7Init[i]);

        // repoint the CPUs to the boot2 binaries
        NDS::ARM9->JumpTo(bootparams[2]);
        NDS::ARM7->JumpTo(bootparams[6]);
    }

    // user data is now expected to be patched by the frontend

    return true;
}


void RunNDMAs(u32 cpu)
{
    // TODO: round-robin mode (requires DMA channels to have a subblock delay set)

    if (cpu == 0)
    {
        if (NDS::ARM9Timestamp >= NDS::ARM9Target) return;

        if (!(NDS::CPUStop & 0x80000000)) NDMAs[0]->Run();
        if (!(NDS::CPUStop & 0x80000000)) NDMAs[1]->Run();
        if (!(NDS::CPUStop & 0x80000000)) NDMAs[2]->Run();
        if (!(NDS::CPUStop & 0x80000000)) NDMAs[3]->Run();
    }
    else
    {
        if (NDS::ARM7Timestamp >= NDS::ARM7Target) return;

        NDMAs[4]->Run();
        NDMAs[5]->Run();
        NDMAs[6]->Run();
        NDMAs[7]->Run();
    }
}

void StallNDMAs()
{
    // TODO
}

bool NDMAsInMode(u32 cpu, u32 mode)
{
    cpu <<= 2;
    if (NDMAs[cpu+0]->IsInMode(mode)) return true;
    if (NDMAs[cpu+1]->IsInMode(mode)) return true;
    if (NDMAs[cpu+2]->IsInMode(mode)) return true;
    if (NDMAs[cpu+3]->IsInMode(mode)) return true;
    return false;
}

bool NDMAsRunning(u32 cpu)
{
    cpu <<= 2;
    if (NDMAs[cpu+0]->IsRunning()) return true;
    if (NDMAs[cpu+1]->IsRunning()) return true;
    if (NDMAs[cpu+2]->IsRunning()) return true;
    if (NDMAs[cpu+3]->IsRunning()) return true;
    return false;
}

void CheckNDMAs(u32 cpu, u32 mode)
{
    cpu <<= 2;
    NDMAs[cpu+0]->StartIfNeeded(mode);
    NDMAs[cpu+1]->StartIfNeeded(mode);
    NDMAs[cpu+2]->StartIfNeeded(mode);
    NDMAs[cpu+3]->StartIfNeeded(mode);
}

void StopNDMAs(u32 cpu, u32 mode)
{
    cpu <<= 2;
    NDMAs[cpu+0]->StopIfNeeded(mode);
    NDMAs[cpu+1]->StopIfNeeded(mode);
    NDMAs[cpu+2]->StopIfNeeded(mode);
    NDMAs[cpu+3]->StopIfNeeded(mode);
}


// new WRAM mapping
// TODO: find out what happens upon overlapping slots!!

void MapNWRAM_A(u32 num, u8 val)
{
    // NWRAM Bank A does not allow all bits to be set
    // possible non working combinations are caught by later code, but these are not set-able at all
    val &= ~0x72;

    if (MBK[0][8] & (1 << num))
    {
        Log(LogLevel::Warn, "trying to map NWRAM_A %d to %02X, but it is write-protected (%08X)\n", num, val, MBK[0][8]);
        return;
    }

    int mbkn = 0, mbks = 8*num;

    u8 oldval = (MBK[0][mbkn] >> mbks) & 0xFF;
    if (oldval == val) return;

#ifdef JIT_ENABLED
    ARMJIT_Memory::RemapNWRAM(0);
#endif

    MBK[0][mbkn] &= ~(0xFF << mbks);
    MBK[0][mbkn] |= (val << mbks);
    MBK[1][mbkn] = MBK[0][mbkn];

    // When we only update the mapping on the written MBK, we will
    // have priority of the last written MBK over the others
    // However the hardware has a fixed order. Therefore
    // we need to iterate through them all in a fixed order and update
    // the mapping, so the result is independent on the MBK write order
    for (int part = 0; part < 4; part++)
    {
        NWRAMMap_A[0][part] = nullptr;
        NWRAMMap_A[1][part] = nullptr;
    }
    for (int part = 3; part >= 0; part--)
    {
        u8* ptr = &NWRAM_A[part << 16];

        u8 mVal = (MBK[0][0 + (part >> 2)] >> ((part & 3) * 8)) & 0xFD;
        if (mVal & 0x80)
        {
            NWRAMMap_A[mVal & 0x03][(mVal >> 2) & 0x3] = ptr;
        }
    }
}

void MapNWRAM_B(u32 num, u8 val)
{
    // NWRAM Bank B does not allow all bits to be set
    // possible non working combinations are caught by later code, but these are not set-able at all
    val &= ~0x60;

    if (MBK[0][8] & (1 << (8+num)))
    {
        Log(LogLevel::Warn, "trying to map NWRAM_B %d to %02X, but it is write-protected (%08X)\n", num, val, MBK[0][8]);
        return;
    }

    int mbkn = 1+(num>>2), mbks = 8*(num&3);

    u8 oldval = (MBK[0][mbkn] >> mbks) & 0xFF;
    if (oldval == val) return;

#ifdef JIT_ENABLED
    ARMJIT_Memory::RemapNWRAM(1);
#endif

    MBK[0][mbkn] &= ~(0xFF << mbks);
    MBK[0][mbkn] |= (val << mbks);
    MBK[1][mbkn] = MBK[0][mbkn];

    // When we only update the mapping on the written MBK, we will
    // have priority of the last written MBK over the others
    // However the hardware has a fixed order. Therefore
    // we need to iterate through them all in a fixed order and update
    // the mapping, so the result is independent on the MBK write order
    for (int part = 0; part < 8; part++)
    {
        NWRAMMap_B[0][part] = nullptr;
        NWRAMMap_B[1][part] = nullptr;
        NWRAMMap_B[2][part] = nullptr;
    }
    for (int part = 7; part >= 0; part--)
    {
        u8* ptr = &NWRAM_B[part << 15];

        u8 mVal = (MBK[0][1 + (part >> 2)] >> ((part & 3) * 8)) & 0xFF;
        if (mVal & 0x80)
        {
            if (mVal & 0x02) mVal &= 0xFE;
            NWRAMMap_B[mVal & 0x03][(mVal >> 2) & 0x7] = ptr;
        }
    }
}

void MapNWRAM_C(u32 num, u8 val)
{
    // NWRAM Bank C does not allow all bits to be set
    // possible non working combinations are caught by later code, but these are not set-able at all
    val &= ~0x60;

    if (MBK[0][8] & (1 << (16+num)))
    {
        Log(LogLevel::Warn, "trying to map NWRAM_C %d to %02X, but it is write-protected (%08X)\n", num, val, MBK[0][8]);
        return;
    }

    int mbkn = 3+(num>>2), mbks = 8*(num&3);

    u8 oldval = (MBK[0][mbkn] >> mbks) & 0xFF;
    if (oldval == val) return;

#ifdef JIT_ENABLED
    ARMJIT_Memory::RemapNWRAM(2);
#endif

    MBK[0][mbkn] &= ~(0xFF << mbks);
    MBK[0][mbkn] |= (val << mbks);
    MBK[1][mbkn] = MBK[0][mbkn];

    // When we only update the mapping on the written MBK, we will
    // have priority of the last written MBK over the others
    // However the hardware has a fixed order. Therefore
    // we need to iterate through them all in a fixed order and update
    // the mapping, so the result is independent on the MBK write order
    for (int part = 0; part < 8; part++)
    {
        NWRAMMap_C[0][part] = nullptr;
        NWRAMMap_C[1][part] = nullptr;
        NWRAMMap_C[2][part] = nullptr;
    }
    for (int part = 7; part >= 0; part--)
    {
        u8* ptr = &NWRAM_C[part << 15];

        u8 mVal = MBK[0][3 + (part >> 2)] >> ((part & 3) * 8) & 0xFF;
        if (mVal & 0x80)
        {
            if (mVal & 0x02) mVal &= 0xFE;
            NWRAMMap_C[mVal & 0x03][(mVal >> 2) & 0x7] = ptr;
        }
    }
}

void MapNWRAMRange(u32 cpu, u32 num, u32 val)
{
    // The windowing registers are not writeable in all bits
    // We need to do this before the change test, so we do not
    // get false "was changed" fall throughs
    switch (num)
    {
        case 0:
            val &= ~0xE00FC00F;
            break;
        case 1:
        case 2:
            val &= ~0xE007C007;
            break;
    }

    u32 oldval = MBK[cpu][5+num];
    if (oldval == val) return;

#ifdef JIT_ENABLED
    ARMJIT_Memory::RemapNWRAM(num);
#endif

    MBK[cpu][5+num] = val;

    // Was TODO: What happens when the ranges are 'out of range'????
    // Answer: The actual range is limited to 0x03000000 to 0x03ffffff
    // The NWRAM can not map into the HW Register ranges and is never
    // mapped there. However the end indizes are allowed to have a value
    // exceeding that range, in which the accessed area is just cut
    // at the end of the region
    // since melonDS does this cut by the case switch in the read/write
    // functions, we do not need to care here.
    if (num == 0)
    {
        u32 start = 0x03000000 + (((val >> 4) & 0xFF) << 16);
        u32 end   = 0x03000000 + (((val >> 20) & 0x1FF) << 16);
        u32 size  = (val >> 12) & 0x3;

        Log(LogLevel::Debug, "NWRAM-A: ARM%d range %08X-%08X, size %d\n", cpu?7:9, start, end, size);

        NWRAMStart[cpu][num] = start;
        NWRAMEnd[cpu][num] = end;

        switch (size)
        {
        case 0:
        case 1: NWRAMMask[cpu][num] = 0x0; break;
        case 2: NWRAMMask[cpu][num] = 0x1; break; // CHECKME
        case 3: NWRAMMask[cpu][num] = 0x3; break;
        }
    }
    else
    {
        u32 start = 0x03000000 + (((val >> 3) & 0x1FF) << 15);
        u32 end   = 0x03000000 + (((val >> 19) & 0x3FF) << 15);
        u32 size  = (val >> 12) & 0x3;

        Log(LogLevel::Debug, "NWRAM-%c: ARM%d range %08X-%08X, size %d\n", 'A'+num, cpu?7:9, start, end, size);

        NWRAMStart[cpu][num] = start;
        NWRAMEnd[cpu][num] = end;

        switch (size)
        {
        case 0: NWRAMMask[cpu][num] = 0x0; break;
        case 1: NWRAMMask[cpu][num] = 0x1; break;
        case 2: NWRAMMask[cpu][num] = 0x3; break;
        case 3: NWRAMMask[cpu][num] = 0x7; break;
        }
    }
}

void ApplyNewRAMSize(u32 size)
{
    switch (size)
    {
    case 0:
    case 1:
        NDS::MainRAMMask = 0x3FFFFF;
        Log(LogLevel::Debug, "RAM: 4MB\n");
        break;
    case 2:
    case 3: // TODO: debug console w/ 32MB?
        NDS::MainRAMMask = 0xFFFFFF;
        Log(LogLevel::Debug, "RAM: 16MB\n");
        break;
    }
}


void Set_SCFG_Clock9(u16 val)
{
    NDS::ARM9Timestamp >>= NDS::ARM9ClockShift;
    NDS::ARM9Target    >>= NDS::ARM9ClockShift;

    Log(LogLevel::Debug, "CLOCK9=%04X\n", val);
    SCFG_Clock9 = val & 0x0187;

    if (SCFG_Clock9 & (1<<0)) NDS::ARM9ClockShift = 2;
    else                      NDS::ARM9ClockShift = 1;

    NDS::ARM9Timestamp <<= NDS::ARM9ClockShift;
    NDS::ARM9Target    <<= NDS::ARM9ClockShift;
    NDS::ARM9->UpdateRegionTimings(0x00000, 0x100000);
}

void Set_SCFG_MC(u32 val)
{
    u32 oldslotstatus = SCFG_MC & 0xC;

    val &= 0xFFFF800C;
    if ((val & 0xC) == 0xC) val &= ~0xC; // hax
    if (val & 0x8000) Log(LogLevel::Warn, "SCFG_MC: weird NDS slot swap\n");
    SCFG_MC = (SCFG_MC & ~0xFFFF800C) | val;

    if ((oldslotstatus == 0x0) && ((SCFG_MC & 0xC) == 0x4))
    {
        NDSCart::ResetCart();
    }
}


u8 ARM9Read8(u32 addr)
{
    if ((addr >= 0xFFFF0000) && (!(SCFG_BIOS & (1<<1))))
    {
        if ((addr >= 0xFFFF8000) && (SCFG_BIOS & (1<<0)))
            return 0xFF;

        return *(u8*)&ARM9iBIOS[addr & 0xFFFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x03000000:
        if (SCFG_EXT[0] & (1 << 25))
        {
            if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
            {
                u8* ptr = NWRAMMap_A[0][(addr >> 16) & NWRAMMask[0][0]];
                return ptr ? *(u8*)&ptr[addr & 0xFFFF] : 0;
            }
            if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
            {
                u8* ptr = NWRAMMap_B[0][(addr >> 15) & NWRAMMask[0][1]];
                return ptr ? *(u8*)&ptr[addr & 0x7FFF] : 0;
            }
            if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
            {
                u8* ptr = NWRAMMap_C[0][(addr >> 15) & NWRAMMask[0][2]];
                return ptr ? *(u8*)&ptr[addr & 0x7FFF] : 0;
            }
        }
        return NDS::ARM9Read8(addr);

    case 0x04000000:
        return ARM9IORead8(addr);

    case 0x08000000:
    case 0x09000000:
    case 0x0A000000:
        return (NDS::ExMemCnt[0] & (1<<7)) ? 0 : 0xFF;

    case 0x0C000000:
        return *(u8*)&NDS::MainRAM[addr & NDS::MainRAMMask];
    }

    return NDS::ARM9Read8(addr);
}

u16 ARM9Read16(u32 addr)
{
    addr &= ~0x1;

    if ((addr >= 0xFFFF0000) && (!(SCFG_BIOS & (1<<1))))
    {
        if ((addr >= 0xFFFF8000) && (SCFG_BIOS & (1<<0)))
            return 0xFFFF;

        return *(u16*)&ARM9iBIOS[addr & 0xFFFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x03000000:
        if (SCFG_EXT[0] & (1 << 25))
        {
            if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
            {
                u8* ptr = NWRAMMap_A[0][(addr >> 16) & NWRAMMask[0][0]];
                return ptr ? *(u16*)&ptr[addr & 0xFFFF] : 0;
            }
            if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
            {
                u8* ptr = NWRAMMap_B[0][(addr >> 15) & NWRAMMask[0][1]];
                return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
            }
            if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
            {
                u8* ptr = NWRAMMap_C[0][(addr >> 15) & NWRAMMask[0][2]];
                return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
            }
        }
        return NDS::ARM9Read16(addr);

    case 0x04000000:
        return ARM9IORead16(addr);

    case 0x08000000:
    case 0x09000000:
    case 0x0A000000:
        return (NDS::ExMemCnt[0] & (1<<7)) ? 0 : 0xFFFF;

    case 0x0C000000:
        return *(u16*)&NDS::MainRAM[addr & NDS::MainRAMMask];
    }

    return NDS::ARM9Read16(addr);
}

u32 ARM9Read32(u32 addr)
{
    addr &= ~0x3;

    if ((addr >= 0xFFFF0000) && (!(SCFG_BIOS & (1<<1))))
    {
        if ((addr >= 0xFFFF8000) && (SCFG_BIOS & (1<<0)))
            return 0xFFFFFFFF;
        return *(u32*)&ARM9iBIOS[addr & 0xFFFF];
    }

    switch (addr & 0xFF000000)
    {
    case 0x02000000:
        // HACK to bypass region locking
        // TODO: make optional
        if (addr == 0x02FE71B0) return 0xFFFFFFFF;
        break;

    case 0x03000000:
        if (SCFG_EXT[0] & (1 << 25))
        {
            if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
            {
                u8* ptr = NWRAMMap_A[0][(addr >> 16) & NWRAMMask[0][0]];
                return ptr ? *(u32*)&ptr[addr & 0xFFFF] : 0;
            }
            if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
            {
                u8* ptr = NWRAMMap_B[0][(addr >> 15) & NWRAMMask[0][1]];
                return ptr ? *(u32*)&ptr[addr & 0x7FFF] : 0;
            }
            if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
            {
                u8* ptr = NWRAMMap_C[0][(addr >> 15) & NWRAMMask[0][2]];
                return ptr ? *(u32*)&ptr[addr & 0x7FFF] : 0;
            }
        }
        return NDS::ARM9Read32(addr);

    case 0x04000000:
        return ARM9IORead32(addr);

    case 0x08000000:
    case 0x09000000:
    case 0x0A000000:
        return (NDS::ExMemCnt[0] & (1<<7)) ? 0 : 0xFFFFFFFF;

    case 0x0C000000:
        return *(u32*)&NDS::MainRAM[addr & NDS::MainRAMMask];
    }

    return NDS::ARM9Read32(addr);
}

void ARM9Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF000000)
    {
    case 0x03000000:
        if (SCFG_EXT[0] & (1 << 25))
        {
            if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 14) & (NWRAMMask[0][0] << 2)) | 0x80;
                for (int page = 0; page < 4; page++)
                {
                    unsigned int bankInfo = (MBK[0][0 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_A[page * 0x10000];
                    *(u8*)&ptr[addr & 0xFFFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_NewSharedWRAM_A>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[0][1] << 2)) | 0x80;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[0][1 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_B[page * 0x8000];
                    *(u8*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_NewSharedWRAM_B>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[0][2] << 2)) | 0x80;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[0][3 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_C[page * 0x8000];
                    *(u8*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_NewSharedWRAM_C>(addr);
#endif
                }
                return;
            }
        }
        return NDS::ARM9Write8(addr, val);

    case 0x04000000:
        ARM9IOWrite8(addr, val);
        return;

    case 0x06000000:
        if (!(SCFG_EXT[0] & (1<<13))) return;
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_VRAM>(addr);
#endif
        switch (addr & 0x00E00000)
        {
        case 0x00000000: GPU::WriteVRAM_ABG<u8>(addr, val); return;
        case 0x00200000: GPU::WriteVRAM_BBG<u8>(addr, val); return;
        case 0x00400000: GPU::WriteVRAM_AOBJ<u8>(addr, val); return;
        case 0x00600000: GPU::WriteVRAM_BOBJ<u8>(addr, val); return;
        default: GPU::WriteVRAM_LCDC<u8>(addr, val); return;
        }

    case 0x08000000:
    case 0x09000000:
    case 0x0A000000:
        return;

    case 0x0C000000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u8*)&NDS::MainRAM[addr & NDS::MainRAMMask] = val;
        return;
    }

    return NDS::ARM9Write8(addr, val);
}

void ARM9Write16(u32 addr, u16 val)
{
    addr &= ~0x1;

    switch (addr & 0xFF000000)
    {
    case 0x03000000:
        if (SCFG_EXT[0] & (1 << 25))
        {
            if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 14) & (NWRAMMask[0][0] << 2)) | 0x80;
                for (int page = 0; page < 4; page++)
                {
                    unsigned int bankInfo = (MBK[0][0 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_A[page * 0x10000];
                    *(u16*)&ptr[addr & 0xFFFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_NewSharedWRAM_A>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[0][1] << 2)) | 0x80;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[0][1 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_B[page * 0x8000];
                    *(u16*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_NewSharedWRAM_B>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[0][2] << 2)) | 0x80;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[0][3 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_C[page * 0x8000];
                    *(u16*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_NewSharedWRAM_C>(addr);
#endif
                }
                return;
            }
        }
        return NDS::ARM9Write16(addr, val);

    case 0x04000000:
        ARM9IOWrite16(addr, val);
        return;

    case 0x08000000:
    case 0x09000000:
    case 0x0A000000:
        return;

    case 0x0C000000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u16*)&NDS::MainRAM[addr & NDS::MainRAMMask] = val;
        return;
    }

    return NDS::ARM9Write16(addr, val);
}

void ARM9Write32(u32 addr, u32 val)
{
    addr &= ~0x3;

    switch (addr & 0xFF000000)
    {
    case 0x03000000:
        if (SCFG_EXT[0] & (1 << 25))
        {
            if (addr >= NWRAMStart[0][0] && addr < NWRAMEnd[0][0])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 14) & (NWRAMMask[0][0] << 2)) | 0x80;
                for (int page = 0; page < 4; page++)
                {
                    unsigned int bankInfo = (MBK[0][0 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_A[page * 0x10000];
                    *(u32*)&ptr[addr & 0xFFFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_NewSharedWRAM_A>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[0][1] && addr < NWRAMEnd[0][1])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[0][1] << 2)) | 0x80;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[0][1 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_B[page * 0x8000];
                    *(u32*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_NewSharedWRAM_B>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[0][2] && addr < NWRAMEnd[0][2])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[0][2] << 2)) | 0x80;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[0][3 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_C[page * 0x8000];
                    *(u32*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_NewSharedWRAM_C>(addr);
#endif
                }
                return;
            }
        }
        return NDS::ARM9Write32(addr, val);

    case 0x04000000:
        ARM9IOWrite32(addr, val);
        return;

    case 0x08000000:
    case 0x09000000:
    case 0x0A000000:
        return;

    case 0x0C000000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<0, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u32*)&NDS::MainRAM[addr & NDS::MainRAMMask] = val;
        return;
    }

    return NDS::ARM9Write32(addr, val);
}

bool ARM9GetMemRegion(u32 addr, bool write, NDS::MemRegion* region)
{
    switch (addr & 0xFF000000)
    {
    case 0x02000000:
    case 0x0C000000:
        region->Mem = NDS::MainRAM;
        region->Mask = NDS::MainRAMMask;
        return true;
    }

    if ((addr & 0xFFFF0000) == 0xFFFF0000 && !write)
    {
        if (SCFG_BIOS & (1<<1))
        {
            if (addr >= 0xFFFF1000)
            {
                region->Mem = NULL;
                return false;
            }

            region->Mem = NDS::ARM9BIOS;
            region->Mask = 0xFFF;
        }
        else
        {
            region->Mem = ARM9iBIOS;
            region->Mask = 0xFFFF;
        }
        return true;
    }

    region->Mem = NULL;
    return false;
}



u8 ARM7Read8(u32 addr)
{
    if ((addr < 0x00010000) && (!(SCFG_BIOS & (1<<9))))
    {
        if ((addr >= 0x00008000) && (SCFG_BIOS & (1<<8)))
            return 0xFF;
        if (NDS::ARM7->R[15] >= 0x00010000)
            return 0xFF;
        if (addr < NDS::ARM7BIOSProt && NDS::ARM7->R[15] >= NDS::ARM7BIOSProt)
            return 0xFF;

        return *(u8*)&ARM7iBIOS[addr & 0xFFFF];
    }

    switch (addr & 0xFF800000)
    {
    case 0x03000000:
    case 0x03800000:
        if (SCFG_EXT[1] & (1 << 25))
        {
            if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
            {
                u8* ptr = NWRAMMap_A[1][(addr >> 16) & NWRAMMask[1][0]];
                return ptr ? *(u8*)&ptr[addr & 0xFFFF] : 0;
            }
            if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
            {
                u8* ptr = NWRAMMap_B[1][(addr >> 15) & NWRAMMask[1][1]];
                return ptr ? *(u8*)&ptr[addr & 0x7FFF] : 0;
            }
            if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
            {
                u8* ptr = NWRAMMap_C[1][(addr >> 15) & NWRAMMask[1][2]];
                return ptr ? *(u8*)&ptr[addr & 0x7FFF] : 0;
            }
        }
        return NDS::ARM7Read8(addr);

    case 0x04000000:
        return ARM7IORead8(addr);

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
    case 0x0A000000:
    case 0x0A800000:
        return (NDS::ExMemCnt[0] & (1<<7)) ? 0xFF : 0;

    case 0x0C000000:
    case 0x0C800000:
        return *(u8*)&NDS::MainRAM[addr & NDS::MainRAMMask];
    }

    return NDS::ARM7Read8(addr);
}

u16 ARM7Read16(u32 addr)
{
    addr &= ~0x1;

    if ((addr < 0x00010000) && (!(SCFG_BIOS & (1<<9))))
    {
        if ((addr >= 0x00008000) && (SCFG_BIOS & (1<<8)))
            return 0xFFFF;
        if (NDS::ARM7->R[15] >= 0x00010000)
            return 0xFFFF;
        if (addr < NDS::ARM7BIOSProt && NDS::ARM7->R[15] >= NDS::ARM7BIOSProt)
            return 0xFFFF;

        return *(u16*)&ARM7iBIOS[addr & 0xFFFF];
    }

    switch (addr & 0xFF800000)
    {
    case 0x03000000:
    case 0x03800000:
        if (SCFG_EXT[1] & (1 << 25))
        {
            if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
            {
                u8* ptr = NWRAMMap_A[1][(addr >> 16) & NWRAMMask[1][0]];
                return ptr ? *(u16*)&ptr[addr & 0xFFFF] : 0;
            }
            if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
            {
                u8* ptr = NWRAMMap_B[1][(addr >> 15) & NWRAMMask[1][1]];
                return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
            }
            if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
            {
                u8* ptr = NWRAMMap_C[1][(addr >> 15) & NWRAMMask[1][2]];
                return ptr ? *(u16*)&ptr[addr & 0x7FFF] : 0;
            }
        }
        return NDS::ARM7Read16(addr);

    case 0x04000000:
        return ARM7IORead16(addr);

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
    case 0x0A000000:
    case 0x0A800000:
        return (NDS::ExMemCnt[0] & (1<<7)) ? 0xFFFF : 0;

    case 0x0C000000:
    case 0x0C800000:
        return *(u16*)&NDS::MainRAM[addr & NDS::MainRAMMask];
    }

    return NDS::ARM7Read16(addr);
}

u32 ARM7Read32(u32 addr)
{
    addr &= ~0x3;

    if ((addr < 0x00010000) && (!(SCFG_BIOS & (1<<9))))
    {
        if ((addr >= 0x00008000) && (SCFG_BIOS & (1<<8)))
            return 0xFFFFFFFF;
        if (NDS::ARM7->R[15] >= 0x00010000)
            return 0xFFFFFFFF;
        if (addr < NDS::ARM7BIOSProt && NDS::ARM7->R[15] >= NDS::ARM7BIOSProt)
            return 0xFFFFFFFF;

        return *(u32*)&ARM7iBIOS[addr & 0xFFFF];
    }

    switch (addr & 0xFF800000)
    {
    case 0x03000000:
    case 0x03800000:
        if (SCFG_EXT[1] & (1 << 25))
        {
            if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
            {
                u8* ptr = NWRAMMap_A[1][(addr >> 16) & NWRAMMask[1][0]];
                return ptr ? *(u32*)&ptr[addr & 0xFFFF] : 0;
            }
            if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
            {
                u8* ptr = NWRAMMap_B[1][(addr >> 15) & NWRAMMask[1][1]];
                return ptr ? *(u32*)&ptr[addr & 0x7FFF] : 0;
            }
            if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
            {
                u8* ptr = NWRAMMap_C[1][(addr >> 15) & NWRAMMask[1][2]];
                return ptr ? *(u32*)&ptr[addr & 0x7FFF] : 0;
            }
        }
        return NDS::ARM7Read32(addr);

    case 0x04000000:
        return ARM7IORead32(addr);

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
    case 0x0A000000:
    case 0x0A800000:
        return (NDS::ExMemCnt[0] & (1<<7)) ? 0xFFFFFFFF : 0;

    case 0x0C000000:
    case 0x0C800000:
        return *(u32*)&NDS::MainRAM[addr & NDS::MainRAMMask];
    }

    return NDS::ARM7Read32(addr);
}

void ARM7Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF800000)
    {
    case 0x03000000:
    case 0x03800000:
        if (SCFG_EXT[1] & (1 << 25))
        {
            if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 14) & (NWRAMMask[1][0] << 2)) | 0x81;
                for (int page = 0; page < 4; page++)
                {
                    unsigned int bankInfo = (MBK[1][0 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_A[page * 0x10000];
                    *(u8*)&ptr[addr & 0xFFFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_NewSharedWRAM_A>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[1][1] << 2)) | 0x81;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[1][1 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_B[page * 0x8000];
                    *(u8*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_NewSharedWRAM_B>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[1][2] << 2)) | 0x81;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[1][3 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_C[page * 0x8000];
                    *(u8*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_NewSharedWRAM_C>(addr);
#endif
                }
                return;
            }
        }
        return NDS::ARM7Write8(addr, val);

    case 0x04000000:
        ARM7IOWrite8(addr, val);
        return;

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
    case 0x0A000000:
    case 0x0A800000:
        return;

    case 0x0C000000:
    case 0x0C800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u8*)&NDS::MainRAM[addr & NDS::MainRAMMask] = val;
        return;
    }

    return NDS::ARM7Write8(addr, val);
}

void ARM7Write16(u32 addr, u16 val)
{
    addr &= ~0x1;

    switch (addr & 0xFF800000)
    {
    case 0x03000000:
    case 0x03800000:
        if (SCFG_EXT[1] & (1 << 25))
        {
            if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 14) & (NWRAMMask[1][0] << 2)) | 0x81;
                for (int page = 0; page < 4; page++)
                {
                    unsigned int bankInfo = (MBK[1][0 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_A[page * 0x10000];
                    *(u16*)&ptr[addr & 0xFFFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_NewSharedWRAM_A>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[1][1] << 2)) | 0x81;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[1][1 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_B[page * 0x8000];
                    *(u16*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_NewSharedWRAM_B>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[1][2] << 2)) | 0x81;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[1][3 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_C[page * 0x8000];
                    *(u16*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_NewSharedWRAM_C>(addr);
#endif
                }
                return;
            }
        }
        return NDS::ARM7Write16(addr, val);

    case 0x04000000:
        ARM7IOWrite16(addr, val);
        return;

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
    case 0x0A000000:
    case 0x0A800000:
        return;

    case 0x0C000000:
    case 0x0C800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u16*)&NDS::MainRAM[addr & NDS::MainRAMMask] = val;
        return;
    }

    return NDS::ARM7Write16(addr, val);
}

void ARM7Write32(u32 addr, u32 val)
{
    addr &= ~0x3;

    switch (addr & 0xFF800000)
    {
    case 0x03000000:
    case 0x03800000:
        if (SCFG_EXT[1] & (1 << 25))
        {
            if (addr >= NWRAMStart[1][0] && addr < NWRAMEnd[1][0])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 14) & (NWRAMMask[1][0] << 2)) | 0x81;
                for (int page = 0; page < 4; page++)
                {
                    unsigned int bankInfo = (MBK[1][0 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_A[page * 0x10000];
                    *(u32*)&ptr[addr & 0xFFFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_NewSharedWRAM_A>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[1][1] && addr < NWRAMEnd[1][1])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[1][1] << 2)) | 0x81;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[1][1 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_B[page * 0x8000];
                    *(u32*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_NewSharedWRAM_B>(addr);
#endif
                }
                return;
            }
            if (addr >= NWRAMStart[1][2] && addr < NWRAMEnd[1][2])
            {
                // Write to a bank is special, as it writes to all
                // parts that are mapped and not just the highest priority
                // See http://melonds.kuribo64.net/board/thread.php?pid=3974#3974
                // so we need to iterate through all parts and write to all mapped here
                unsigned int destPartSetting = ((addr >> 13) & (NWRAMMask[1][2] << 2)) | 0x81;
                for (int page = 0; page < 8; page++)
                {
                    unsigned int bankInfo = (MBK[1][3 + (page / 4)] >> ((page % 4) * 8)) & 0xff;
                    if (bankInfo != destPartSetting)
                        continue;
                    u8* ptr = &NWRAM_C[page * 0x8000];
                    *(u32*)&ptr[addr & 0x7FFF] = val;
#ifdef JIT_ENABLED
                    ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_NewSharedWRAM_C>(addr);
#endif
                }
                return;
            }
        }
        return NDS::ARM7Write32(addr, val);

    case 0x04000000:
        ARM7IOWrite32(addr, val);
        return;

    case 0x08000000:
    case 0x08800000:
    case 0x09000000:
    case 0x09800000:
    case 0x0A000000:
    case 0x0A800000:
        return;

    case 0x0C000000:
    case 0x0C800000:
#ifdef JIT_ENABLED
        ARMJIT::CheckAndInvalidate<1, ARMJIT_Memory::memregion_MainRAM>(addr);
#endif
        *(u32*)&NDS::MainRAM[addr & NDS::MainRAMMask] = val;
        return;
    }

    return NDS::ARM7Write32(addr, val);
}

bool ARM7GetMemRegion(u32 addr, bool write, NDS::MemRegion* region)
{
    switch (addr & 0xFF800000)
    {
    case 0x02000000:
    case 0x02800000:
    case 0x0C000000:
    case 0x0C800000:
        region->Mem = NDS::MainRAM;
        region->Mask = NDS::MainRAMMask;
        return true;
    }

    // BIOS. ARM7 PC has to be within range.
    /*if (addr < 0x00010000 && !write)
    {
        if (NDS::ARM7->R[15] < 0x00010000 && (addr >= NDS::ARM7BIOSProt || NDS::ARM7->R[15] < NDS::ARM7BIOSProt))
        {
            region->Mem = NDS::ARM7BIOS;
            region->Mask = 0xFFFF;
            return true;
        }
    }*/

    region->Mem = NULL;
    return false;
}




#define CASE_READ8_16BIT(addr, val) \
    case (addr): return (val) & 0xFF; \
    case (addr+1): return (val) >> 8;

#define CASE_READ8_32BIT(addr, val) \
    case (addr): return (val) & 0xFF; \
    case (addr+1): return ((val) >> 8) & 0xFF; \
    case (addr+2): return ((val) >> 16) & 0xFF; \
    case (addr+3): return (val) >> 24;

#define CASE_READ16_32BIT(addr, val) \
    case (addr): return (val) & 0xFFFF; \
    case (addr+2): return (val) >> 16;

u8 ARM9IORead8(u32 addr)
{
    switch (addr)
    {
    case 0x04004000: return SCFG_BIOS & 0xFF;
    case 0x04004006: return SCFG_RST  & 0xFF;

    CASE_READ8_32BIT(0x04004040, MBK[0][0])
    CASE_READ8_32BIT(0x04004044, MBK[0][1])
    CASE_READ8_32BIT(0x04004048, MBK[0][2])
    CASE_READ8_32BIT(0x0400404C, MBK[0][3])
    CASE_READ8_32BIT(0x04004050, MBK[0][4])
    CASE_READ8_32BIT(0x04004054, MBK[0][5])
    CASE_READ8_32BIT(0x04004058, MBK[0][6])
    CASE_READ8_32BIT(0x0400405C, MBK[0][7])
    CASE_READ8_32BIT(0x04004060, MBK[0][8])
    }

    if ((addr & 0xFFFFFF00) == 0x04004200)
    {
        if (!(SCFG_EXT[0] & (1<<17))) return 0;
        return DSi_CamModule::Read8(addr);
    }

    if ((addr & 0xFFFFFF00) == 0x04004300)
    {
        if (!(SCFG_EXT[0] & (1<<18))) return 0;
        return DSi_DSP::Read8(addr);
    }

    return NDS::ARM9IORead8(addr);
}

u16 ARM9IORead16(u32 addr)
{
    switch (addr)
    {
    case 0x04004000: return SCFG_BIOS & 0xFF;
    case 0x04004004: return SCFG_Clock9;
    case 0x04004006: return SCFG_RST;
    case 0x04004010: return SCFG_MC & 0xFFFF;

    CASE_READ16_32BIT(0x04004040, MBK[0][0])
    CASE_READ16_32BIT(0x04004044, MBK[0][1])
    CASE_READ16_32BIT(0x04004048, MBK[0][2])
    CASE_READ16_32BIT(0x0400404C, MBK[0][3])
    CASE_READ16_32BIT(0x04004050, MBK[0][4])
    CASE_READ16_32BIT(0x04004054, MBK[0][5])
    CASE_READ16_32BIT(0x04004058, MBK[0][6])
    CASE_READ16_32BIT(0x0400405C, MBK[0][7])
    CASE_READ16_32BIT(0x04004060, MBK[0][8])
    }

    if ((addr & 0xFFFFFF00) == 0x04004200)
    {
        if (!(SCFG_EXT[0] & (1<<17))) return 0;
        return DSi_CamModule::Read16(addr);
    }

    if ((addr & 0xFFFFFF00) == 0x04004300)
    {
        if (!(SCFG_EXT[0] & (1<<18))) return 0;
        return DSi_DSP::Read16(addr);
    }

    return NDS::ARM9IORead16(addr);
}

u32 ARM9IORead32(u32 addr)
{
    switch (addr)
    {
    case 0x04004000: return SCFG_BIOS & 0xFF;
    case 0x04004004: return SCFG_Clock9 | ((u32)SCFG_RST << 16);
    case 0x04004008: return SCFG_EXT[0];
    case 0x04004010: return SCFG_MC & 0xFFFF;

    case 0x04004040: return MBK[0][0];
    case 0x04004044: return MBK[0][1];
    case 0x04004048: return MBK[0][2];
    case 0x0400404C: return MBK[0][3];
    case 0x04004050: return MBK[0][4];
    case 0x04004054: return MBK[0][5];
    case 0x04004058: return MBK[0][6];
    case 0x0400405C: return MBK[0][7];
    case 0x04004060: return MBK[0][8];

    case 0x04004100: return NDMACnt[0];
    case 0x04004104: return NDMAs[0]->SrcAddr;
    case 0x04004108: return NDMAs[0]->DstAddr;
    case 0x0400410C: return NDMAs[0]->TotalLength;
    case 0x04004110: return NDMAs[0]->BlockLength;
    case 0x04004114: return NDMAs[0]->SubblockTimer;
    case 0x04004118: return NDMAs[0]->FillData;
    case 0x0400411C: return NDMAs[0]->Cnt;
    case 0x04004120: return NDMAs[1]->SrcAddr;
    case 0x04004124: return NDMAs[1]->DstAddr;
    case 0x04004128: return NDMAs[1]->TotalLength;
    case 0x0400412C: return NDMAs[1]->BlockLength;
    case 0x04004130: return NDMAs[1]->SubblockTimer;
    case 0x04004134: return NDMAs[1]->FillData;
    case 0x04004138: return NDMAs[1]->Cnt;
    case 0x0400413C: return NDMAs[2]->SrcAddr;
    case 0x04004140: return NDMAs[2]->DstAddr;
    case 0x04004144: return NDMAs[2]->TotalLength;
    case 0x04004148: return NDMAs[2]->BlockLength;
    case 0x0400414C: return NDMAs[2]->SubblockTimer;
    case 0x04004150: return NDMAs[2]->FillData;
    case 0x04004154: return NDMAs[2]->Cnt;
    case 0x04004158: return NDMAs[3]->SrcAddr;
    case 0x0400415C: return NDMAs[3]->DstAddr;
    case 0x04004160: return NDMAs[3]->TotalLength;
    case 0x04004164: return NDMAs[3]->BlockLength;
    case 0x04004168: return NDMAs[3]->SubblockTimer;
    case 0x0400416C: return NDMAs[3]->FillData;
    case 0x04004170: return NDMAs[3]->Cnt;
    }

    if ((addr & 0xFFFFFF00) == 0x04004200)
    {
        if (!(SCFG_EXT[0] & (1<<17))) return 0;
        return DSi_CamModule::Read32(addr);
    }

    if ((addr & 0xFFFFFF00) == 0x04004300)
    {
        if (!(SCFG_EXT[0] & (1<<18))) return 0;
        return DSi_DSP::Read32(addr);
    }

    return NDS::ARM9IORead32(addr);
}

void ARM9IOWrite8(u32 addr, u8 val)
{
    switch (addr)
    {
    case 0x04000301:
        // TODO: OPTIONAL PERFORMANCE HACK
        // the DSi ARM9 BIOS has a bug where the IRQ wait function attempts to use (ARM7-only) HALTCNT
        // effectively causing it to wait in a busy loop.
        // for better DSi performance, we can implement an actual IRQ wait here.
        // in practice this would only matter when running DS software in DSi mode (ie already a hack).
        // DSi software does not use the BIOS IRQ wait function.
        //if (val == 0x80 && NDS::ARM9->R[15] == 0xFFFF0268) NDS::ARM9->Halt(1);
        return;

    case 0x04004006:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        SCFG_RST = (SCFG_RST & 0xFF00) | val;
        DSi_DSP::SetRstLine(val & 1);
        return;

    case 0x04004040:
    case 0x04004041:
    case 0x04004042:
    case 0x04004043:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_A(addr & 3, val);
        return;
    case 0x04004044:
    case 0x04004045:
    case 0x04004046:
    case 0x04004047:
    case 0x04004048:
    case 0x04004049:
    case 0x0400404A:
    case 0x0400404B:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_B((addr - 0x04) & 7, val);
        return;
    case 0x0400404C:
    case 0x0400404D:
    case 0x0400404E:
    case 0x0400404F:
    case 0x04004050:
    case 0x04004051:
    case 0x04004052:
    case 0x04004053:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_C((addr-0x0C) & 7, val);
        return;
    }

    if ((addr & 0xFFFFFF00) == 0x04004200)
    {
        if (!(SCFG_EXT[0] & (1<<17))) return;
        return DSi_CamModule::Write8(addr, val);
    }

    if ((addr & 0xFFFFFF00) == 0x04004300)
    {
        if (!(SCFG_EXT[0] & (1<<18))) return;
        return DSi_DSP::Write8(addr, val);
    }

    return NDS::ARM9IOWrite8(addr, val);
}

void ARM9IOWrite16(u32 addr, u16 val)
{
    switch (addr)
    {
    case 0x04004004:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        Set_SCFG_Clock9(val);
        return;

    case 0x04004006:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        SCFG_RST = val;
        DSi_DSP::SetRstLine(val & 1);
        return;

    case 0x04004040:
    case 0x04004042:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_A((addr & 2), val & 0xFF);
        MapNWRAM_A((addr & 2) + 1, val >> 8);
        return;

    case 0x04004044:
    case 0x04004046:
    case 0x04004048:
    case 0x0400404A:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_B(((addr - 0x04) & 6), val & 0xFF);
        MapNWRAM_B(((addr - 0x04) & 6) + 1, val >> 8);
        return;
    case 0x0400404C:
    case 0x0400404E:
    case 0x04004050:
    case 0x04004052:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_C(((addr - 0x0C) & 6), val & 0xFF);
        MapNWRAM_C(((addr - 0x0C) & 6) + 1, val >> 8);
        return;
    }

    if ((addr & 0xFFFFFF00) == 0x04004200)
    {
        if (!(SCFG_EXT[0] & (1<<17))) return;
        return DSi_CamModule::Write16(addr, val);
    }

    if ((addr & 0xFFFFFF00) == 0x04004300)
    {
        if (!(SCFG_EXT[0] & (1<<18))) return;
        return DSi_DSP::Write16(addr, val);
    }

    return NDS::ARM9IOWrite16(addr, val);
}

void ARM9IOWrite32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x04004004:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        Set_SCFG_Clock9(val & 0xFFFF);
        SCFG_RST = val >> 16;
        DSi_DSP::SetRstLine((val >> 16) & 1);
        break;

    case 0x04004008:
        {
            if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
                return;
            u32 oldram = (SCFG_EXT[0] >> 14) & 0x3;
            u32 newram = (val >> 14) & 0x3;

            SCFG_EXT[0] &= ~0x8007F19F;
            SCFG_EXT[0] |= (val & 0x8007F19F);
            SCFG_EXT[1] &= ~0x0000F080;
            SCFG_EXT[1] |= (val & 0x0000F080);
            Log(LogLevel::Debug, "SCFG_EXT = %08X / %08X (val9 %08X)\n", SCFG_EXT[0], SCFG_EXT[1], val);
            /*switch ((SCFG_EXT[0] >> 14) & 0x3)
            {
            case 0:
            case 1:
                NDS::MainRAMMask = 0x3FFFFF;
                printf("RAM: 4MB\n");
                //baziderp=true;
                break;
            case 2:
            case 3: // TODO: debug console w/ 32MB?
                NDS::MainRAMMask = 0xFFFFFF;
                printf("RAM: 16MB\n");
                break;
            }*/
            // HAX!!
            // a change to the RAM size setting is supposed to apply immediately (it does so on hardware)
            // however, doing so will cause DS-mode app startup to break, because the change happens while the ARM7
            // is still busy clearing/relocating shit
            //if (newram != oldram)
            //    NDS::ScheduleEvent(NDS::Event_DSi_RAMSizeChange, false, 512*512*512, ApplyNewRAMSize, newram);
            Log(LogLevel::Debug, "from %08X, ARM7 %08X, %08X\n", NDS::GetPC(0), NDS::GetPC(1), NDS::ARM7->R[1]);
        }
        return;

    case 0x04004040:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_A(0, val & 0xFF);
        MapNWRAM_A(1, (val >> 8) & 0xFF);
        MapNWRAM_A(2, (val >> 16) & 0xFF);
        MapNWRAM_A(3, val >> 24);
        return;
    case 0x04004044:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_B(0, val & 0xFF);
        MapNWRAM_B(1, (val >> 8) & 0xFF);
        MapNWRAM_B(2, (val >> 16) & 0xFF);
        MapNWRAM_B(3, val >> 24);
        return;
    case 0x04004048:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_B(4, val & 0xFF);
        MapNWRAM_B(5, (val >> 8) & 0xFF);
        MapNWRAM_B(6, (val >> 16) & 0xFF);
        MapNWRAM_B(7, val >> 24);
        return;
    case 0x0400404C:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_C(0, val & 0xFF);
        MapNWRAM_C(1, (val >> 8) & 0xFF);
        MapNWRAM_C(2, (val >> 16) & 0xFF);
        MapNWRAM_C(3, val >> 24);
        return;
    case 0x04004050:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAM_C(4, val & 0xFF);
        MapNWRAM_C(5, (val >> 8) & 0xFF);
        MapNWRAM_C(6, (val >> 16) & 0xFF);
        MapNWRAM_C(7, val >> 24);
        return;
    case 0x04004054:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAMRange(0, 0, val);
        return;
    case 0x04004058:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAMRange(0, 1, val);
        return;
    case 0x0400405C:
        if (!(SCFG_EXT[0] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAMRange(0, 2, val);
        return;

    case 0x04004100: NDMACnt[0] = val & 0x800F0000; return;
    case 0x04004104: NDMAs[0]->SrcAddr = val & 0xFFFFFFFC; return;
    case 0x04004108: NDMAs[0]->DstAddr = val & 0xFFFFFFFC; return;
    case 0x0400410C: NDMAs[0]->TotalLength = val & 0x0FFFFFFF; return;
    case 0x04004110: NDMAs[0]->BlockLength = val & 0x00FFFFFF; return;
    case 0x04004114: NDMAs[0]->SubblockTimer = val & 0x0003FFFF; return;
    case 0x04004118: NDMAs[0]->FillData = val; return;
    case 0x0400411C: NDMAs[0]->WriteCnt(val); return;
    case 0x04004120: NDMAs[1]->SrcAddr = val & 0xFFFFFFFC; return;
    case 0x04004124: NDMAs[1]->DstAddr = val & 0xFFFFFFFC; return;
    case 0x04004128: NDMAs[1]->TotalLength = val & 0x0FFFFFFF; return;
    case 0x0400412C: NDMAs[1]->BlockLength = val & 0x00FFFFFF; return;
    case 0x04004130: NDMAs[1]->SubblockTimer = val & 0x0003FFFF; return;
    case 0x04004134: NDMAs[1]->FillData = val; return;
    case 0x04004138: NDMAs[1]->WriteCnt(val); return;
    case 0x0400413C: NDMAs[2]->SrcAddr = val & 0xFFFFFFFC; return;
    case 0x04004140: NDMAs[2]->DstAddr = val & 0xFFFFFFFC; return;
    case 0x04004144: NDMAs[2]->TotalLength = val & 0x0FFFFFFF; return;
    case 0x04004148: NDMAs[2]->BlockLength = val & 0x00FFFFFF; return;
    case 0x0400414C: NDMAs[2]->SubblockTimer = val & 0x0003FFFF; return;
    case 0x04004150: NDMAs[2]->FillData = val; return;
    case 0x04004154: NDMAs[2]->WriteCnt(val); return;
    case 0x04004158: NDMAs[3]->SrcAddr = val & 0xFFFFFFFC; return;
    case 0x0400415C: NDMAs[3]->DstAddr = val & 0xFFFFFFFC; return;
    case 0x04004160: NDMAs[3]->TotalLength = val & 0x0FFFFFFF; return;
    case 0x04004164: NDMAs[3]->BlockLength = val & 0x00FFFFFF; return;
    case 0x04004168: NDMAs[3]->SubblockTimer = val & 0x0003FFFF; return;
    case 0x0400416C: NDMAs[3]->FillData = val; return;
    case 0x04004170: NDMAs[3]->WriteCnt(val); return;
    }

    if ((addr & 0xFFFFFF00) == 0x04004200)
    {
        if (!(SCFG_EXT[0] & (1<<17))) return;
        return DSi_CamModule::Write32(addr, val);
    }

    if ((addr & 0xFFFFFF00) == 0x04004300)
    {
        if (!(SCFG_EXT[0] & (1<<18))) return;
        return DSi_DSP::Write32(addr, val);
    }

    return NDS::ARM9IOWrite32(addr, val);
}


u8 ARM7IORead8(u32 addr)
{

    switch (addr)
    {
    case 0x04004000:
        return SCFG_BIOS & 0xFF;
    case 0x04004001: return SCFG_BIOS >> 8;
    case 0x04004002: return 0; // SCFG_ROMWE, always 0

    CASE_READ8_32BIT(0x04004040, MBK[1][0])
    CASE_READ8_32BIT(0x04004044, MBK[1][1])
    CASE_READ8_32BIT(0x04004048, MBK[1][2])
    CASE_READ8_32BIT(0x0400404C, MBK[1][3])
    CASE_READ8_32BIT(0x04004050, MBK[1][4])
    CASE_READ8_32BIT(0x04004054, MBK[1][5])
    CASE_READ8_32BIT(0x04004058, MBK[1][6])
    CASE_READ8_32BIT(0x0400405C, MBK[1][7])
    CASE_READ8_32BIT(0x04004060, MBK[1][8])

    case 0x04004500: return DSi_I2C::ReadData();
    case 0x04004501: return DSi_I2C::Cnt;

    case 0x04004D00: if (SCFG_BIOS & (1<<10)) return 0; return NANDImage->GetConsoleID() & 0xFF;
    case 0x04004D01: if (SCFG_BIOS & (1<<10)) return 0; return (NANDImage->GetConsoleID() >> 8) & 0xFF;
    case 0x04004D02: if (SCFG_BIOS & (1<<10)) return 0; return (NANDImage->GetConsoleID() >> 16) & 0xFF;
    case 0x04004D03: if (SCFG_BIOS & (1<<10)) return 0; return (NANDImage->GetConsoleID() >> 24) & 0xFF;
    case 0x04004D04: if (SCFG_BIOS & (1<<10)) return 0; return (NANDImage->GetConsoleID() >> 32) & 0xFF;
    case 0x04004D05: if (SCFG_BIOS & (1<<10)) return 0; return (NANDImage->GetConsoleID() >> 40) & 0xFF;
    case 0x04004D06: if (SCFG_BIOS & (1<<10)) return 0; return (NANDImage->GetConsoleID() >> 48) & 0xFF;
    case 0x04004D07: if (SCFG_BIOS & (1<<10)) return 0; return NANDImage->GetConsoleID() >> 56;
    case 0x04004D08: return 0;

    case 0x4004700: return DSi_DSP::SNDExCnt;
    case 0x4004701: return DSi_DSP::SNDExCnt >> 8;

    case 0x04004C00: return GPIO_Data;
    case 0x04004C01: return GPIO_Dir;
    case 0x04004C02: return GPIO_IEdgeSel;
    case 0x04004C03: return GPIO_IE;
    case 0x04004C04: return GPIO_WiFi & 0xff;
    case 0x04004C05: return GPIO_WiFi >> 8;
    }

    return NDS::ARM7IORead8(addr);
}

u16 ARM7IORead16(u32 addr)
{
    switch (addr)
    {
    case 0x04000218: return NDS::IE2;
    case 0x0400021C: return NDS::IF2;

    case 0x04004000: return SCFG_BIOS;
    case 0x04004002: return 0; // SCFG_ROMWE, always 0
    case 0x04004004: return SCFG_Clock7;
    case 0x04004006: return 0; // JTAG register
    case 0x04004010: return SCFG_MC & 0xFFFF;

    CASE_READ16_32BIT(0x04004040, MBK[1][0])
    CASE_READ16_32BIT(0x04004044, MBK[1][1])
    CASE_READ16_32BIT(0x04004048, MBK[1][2])
    CASE_READ16_32BIT(0x0400404C, MBK[1][3])
    CASE_READ16_32BIT(0x04004050, MBK[1][4])
    CASE_READ16_32BIT(0x04004054, MBK[1][5])
    CASE_READ16_32BIT(0x04004058, MBK[1][6])
    CASE_READ16_32BIT(0x0400405C, MBK[1][7])
    CASE_READ16_32BIT(0x04004060, MBK[1][8])

    case 0x04004D00: if (SCFG_BIOS & (1<<10)) return 0; return NANDImage->GetConsoleID() & 0xFFFF;
    case 0x04004D02: if (SCFG_BIOS & (1<<10)) return 0; return (NANDImage->GetConsoleID() >> 16) & 0xFFFF;
    case 0x04004D04: if (SCFG_BIOS & (1<<10)) return 0; return (NANDImage->GetConsoleID() >> 32) & 0xFFFF;
    case 0x04004D06: if (SCFG_BIOS & (1<<10)) return 0; return NANDImage->GetConsoleID() >> 48;
    case 0x04004D08: return 0;

    case 0x4004700: return DSi_DSP::SNDExCnt;

    case 0x04004C00: return GPIO_Data | ((u16)GPIO_Dir << 8);
    case 0x04004C02: return GPIO_IEdgeSel | ((u16)GPIO_IE << 8);
    case 0x04004C04: return GPIO_WiFi;
    }

    if (addr >= 0x04004800 && addr < 0x04004A00)
    {
        return SDMMC->Read(addr);
    }
    if (addr >= 0x04004A00 && addr < 0x04004C00)
    {
        return SDIO->Read(addr);
    }

    return NDS::ARM7IORead16(addr);
}

u32 ARM7IORead32(u32 addr)
{
    switch (addr)
    {
    case 0x04000218: return NDS::IE2;
    case 0x0400021C: return NDS::IF2;

    case 0x04004000: return SCFG_BIOS;
    case 0x04004008: return SCFG_EXT[1];
    case 0x04004010: return SCFG_MC;

    case 0x04004040: return MBK[1][0];
    case 0x04004044: return MBK[1][1];
    case 0x04004048: return MBK[1][2];
    case 0x0400404C: return MBK[1][3];
    case 0x04004050: return MBK[1][4];
    case 0x04004054: return MBK[1][5];
    case 0x04004058: return MBK[1][6];
    case 0x0400405C: return MBK[1][7];
    case 0x04004060: return MBK[1][8];

    case 0x04004100: return NDMACnt[1];
    case 0x04004104: return NDMAs[4]->SrcAddr;
    case 0x04004108: return NDMAs[4]->DstAddr;
    case 0x0400410C: return NDMAs[4]->TotalLength;
    case 0x04004110: return NDMAs[4]->BlockLength;
    case 0x04004114: return NDMAs[4]->SubblockTimer;
    case 0x04004118: return NDMAs[4]->FillData;
    case 0x0400411C: return NDMAs[4]->Cnt;
    case 0x04004120: return NDMAs[5]->SrcAddr;
    case 0x04004124: return NDMAs[5]->DstAddr;
    case 0x04004128: return NDMAs[5]->TotalLength;
    case 0x0400412C: return NDMAs[5]->BlockLength;
    case 0x04004130: return NDMAs[5]->SubblockTimer;
    case 0x04004134: return NDMAs[5]->FillData;
    case 0x04004138: return NDMAs[5]->Cnt;
    case 0x0400413C: return NDMAs[6]->SrcAddr;
    case 0x04004140: return NDMAs[6]->DstAddr;
    case 0x04004144: return NDMAs[6]->TotalLength;
    case 0x04004148: return NDMAs[6]->BlockLength;
    case 0x0400414C: return NDMAs[6]->SubblockTimer;
    case 0x04004150: return NDMAs[6]->FillData;
    case 0x04004154: return NDMAs[6]->Cnt;
    case 0x04004158: return NDMAs[7]->SrcAddr;
    case 0x0400415C: return NDMAs[7]->DstAddr;
    case 0x04004160: return NDMAs[7]->TotalLength;
    case 0x04004164: return NDMAs[7]->BlockLength;
    case 0x04004168: return NDMAs[7]->SubblockTimer;
    case 0x0400416C: return NDMAs[7]->FillData;
    case 0x04004170: return NDMAs[7]->Cnt;

    case 0x04004400: return DSi_AES::ReadCnt();
    case 0x0400440C: return DSi_AES::ReadOutputFIFO();

    case 0x04004D00: if (SCFG_BIOS & (1<<10)) return 0; return NANDImage->GetConsoleID() & 0xFFFFFFFF;
    case 0x04004D04: if (SCFG_BIOS & (1<<10)) return 0; return NANDImage->GetConsoleID() >> 32;
    case 0x04004D08: return 0;

    case 0x4004700:
        Log(LogLevel::Debug, "32-Bit SNDExCnt read? %08X\n", NDS::ARM7->R[15]);
        return DSi_DSP::SNDExCnt;
    }

    if (addr >= 0x04004800 && addr < 0x04004A00)
    {
        if (addr == 0x0400490C) return SDMMC->ReadFIFO32();
        return SDMMC->Read(addr) | (SDMMC->Read(addr+2) << 16);
    }
    if (addr >= 0x04004A00 && addr < 0x04004C00)
    {
        if (addr == 0x04004B0C) return SDIO->ReadFIFO32();
        return SDIO->Read(addr) | (SDIO->Read(addr+2) << 16);
    }

    return NDS::ARM7IORead32(addr);
}

void ARM7IOWrite8(u32 addr, u8 val)
{
    switch (addr)
    {
    case 0x04004000:
        if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        SCFG_BIOS |= (val & 0x03);
        return;
    case 0x04004001:
        if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        SCFG_BIOS |= ((val & 0x07) << 8);
        return;
    case 0x04004002:
        // SCFG_ROMWE. ignored, as it always reads as 0
        return;
    case 0x04004060:
    case 0x04004061:
    case 0x04004062:
    case 0x04004063:
    {
        if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        u32 tmp = MBK[0][8];
        tmp &= ~(0xff << ((addr % 4) * 8));
        tmp |= (val << ((addr % 4) * 8));
        MBK[0][8] = tmp & 0x00FFFF0F;
        MBK[1][8] = MBK[0][8];
        return;
    }

    case 0x04004500: DSi_I2C::WriteData(val); return;
    case 0x04004501: DSi_I2C::WriteCnt(val); return;

    case 0x4004700:
        DSi_DSP::WriteSNDExCnt((u16)val | (DSi_DSP::SNDExCnt & 0xFF00));
        return;
    case 0x4004701:
        DSi_DSP::WriteSNDExCnt(((u16)val << 8) | (DSi_DSP::SNDExCnt & 0x00FF));
        return;

    case 0x04004C00:
        GPIO_Data = val;
        return;
    case 0x04004C01:
        GPIO_Dir = val;
        return;
    case 0x04004C02:
        GPIO_IEdgeSel = val;
        return;
    case 0x04004C03:
        GPIO_IE = val;
        return;
    case 0x04004C04:
        GPIO_WiFi = val | (GPIO_WiFi & 0xff00);
        return;
    }

    if (addr >= 0x04004420 && addr < 0x04004430)
    {
        u32 shift = (addr&3)*8;
        addr -= 0x04004420;
        addr &= ~3;
        DSi_AES::WriteIV(addr, (u32)val << shift, 0xFF << shift);
        return;
    }
    if (addr >= 0x04004430 && addr < 0x04004440)
    {
        u32 shift = (addr&3)*8;
        addr -= 0x04004430;
        addr &= ~3;
        DSi_AES::WriteMAC(addr, (u32)val << shift, 0xFF << shift);
        return;
    }
    if (addr >= 0x04004440 && addr < 0x04004500)
    {
        u32 shift = (addr&3)*8;
        addr -= 0x04004440;
        addr &= ~3;

        int n = 0;
        while (addr >= 0x30) { addr -= 0x30; n++; }

        switch (addr >> 4)
        {
        case 0: DSi_AES::WriteKeyNormal(n, addr&0xF, (u32)val << shift, 0xFF << shift); return;
        case 1: DSi_AES::WriteKeyX(n, addr&0xF, (u32)val << shift, 0xFF << shift); return;
        case 2: DSi_AES::WriteKeyY(n, addr&0xF, (u32)val << shift, 0xFF << shift); return;
        }
    }

    return NDS::ARM7IOWrite8(addr, val);
}

void ARM7IOWrite16(u32 addr, u16 val)
{
    switch (addr)
    {
        case 0x04000218: NDS::IE2 = (val & 0x7FF7); NDS::UpdateIRQ(1); return;
        case 0x0400021C: NDS::IF2 &= ~(val & 0x7FF7); NDS::UpdateIRQ(1); return;

        case 0x04004000:
            if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
                return;
            SCFG_BIOS |= (val & 0x0703);
            return;
        case 0x04004002:
            // SCFG_ROMWE. ignored, as it always reads as 0
            return;
        case 0x04004004:
            if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
                return;
            SCFG_Clock7 = val & 0x0187;
            return;
        case 0x04004010:
            if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
                return;
            Set_SCFG_MC((SCFG_MC & 0xFFFF0000) | val);
            return;
        case 0x04004060:
        case 0x04004062:
            if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
                return;
            {
                u32 tmp = MBK[0][8];
                tmp &= ~(0xffff << ((addr % 4) * 8));
                tmp |= (val << ((addr % 4) * 8));
                MBK[0][8] = tmp & 0x00FFFF0F;
                MBK[1][8] = MBK[0][8];
            }
            return;

        case 0x04004406:
            DSi_AES::WriteBlkCnt(val<<16);
            return;

        case 0x4004700:
            DSi_DSP::WriteSNDExCnt(val);
            return;

        case 0x04004C00:
            GPIO_Data = val & 0xff;
            GPIO_Dir = val >> 8;
            return;
        case 0x04004C02:
            GPIO_IEdgeSel = val & 0xff;
            GPIO_IE = val >> 8;
            return;
        case 0x04004C04:
            GPIO_WiFi = val;
            return;
    }

    if (addr >= 0x04004420 && addr < 0x04004430)
    {
        u32 shift = (addr&1)*16;
        addr -= 0x04004420;
        addr &= ~1;
        DSi_AES::WriteIV(addr, (u32)val << shift, 0xFFFF << shift);
        return;
    }
    if (addr >= 0x04004430 && addr < 0x04004440)
    {
        u32 shift = (addr&1)*16;
        addr -= 0x04004430;
        addr &= ~1;
        DSi_AES::WriteMAC(addr, (u32)val << shift, 0xFFFF << shift);
        return;
    }
    if (addr >= 0x04004440 && addr < 0x04004500)
    {
        u32 shift = (addr&1)*16;
        addr -= 0x04004440;
        addr &= ~1;

        int n = 0;
        while (addr >= 0x30) { addr -= 0x30; n++; }

        switch (addr >> 4)
        {
        case 0: DSi_AES::WriteKeyNormal(n, addr&0xF, (u32)val << shift, 0xFFFF << shift); return;
        case 1: DSi_AES::WriteKeyX(n, addr&0xF, (u32)val << shift, 0xFFFF << shift); return;
        case 2: DSi_AES::WriteKeyY(n, addr&0xF, (u32)val << shift, 0xFFFF << shift); return;
        }
    }

    if (addr >= 0x04004800 && addr < 0x04004A00)
    {
        SDMMC->Write(addr, val);
        return;
    }
    if (addr >= 0x04004A00 && addr < 0x04004C00)
    {
        SDIO->Write(addr, val);
        return;
    }

    return NDS::ARM7IOWrite16(addr, val);
}

void ARM7IOWrite32(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0x04000218: NDS::IE2 = (val & 0x7FF7); NDS::UpdateIRQ(1); return;
    case 0x0400021C: NDS::IF2 &= ~(val & 0x7FF7); NDS::UpdateIRQ(1); return;

    case 0x04004000:
        if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        SCFG_BIOS |= (val & 0x0703);
        return;
    case 0x04004008:
        if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        SCFG_EXT[0] &= ~0x03000000;
        SCFG_EXT[0] |= (val & 0x03000000);
        SCFG_EXT[1] &= ~0x93FF0F07;
        SCFG_EXT[1] |= (val & 0x93FF0F07);
        Log(LogLevel::Debug, "SCFG_EXT = %08X / %08X (val7 %08X)\n", SCFG_EXT[0], SCFG_EXT[1], val);
        return;
    case 0x04004010:
        if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        Set_SCFG_MC(val);
        return;

    case 0x04004054:
        if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAMRange(1, 0, val);
        return;
    case 0x04004058:
        if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAMRange(1, 1, val);
        return;
    case 0x0400405C:
        if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        MapNWRAMRange(1, 2, val);
        return;
    case 0x04004060:
        if (!(SCFG_EXT[1] & (1 << 31))) /* no access to SCFG Registers if disabled*/
            return;
        val &= 0x00FFFF0F;
        MBK[0][8] = val;
        MBK[1][8] = val;
        return;

    case 0x04004100: NDMACnt[1] = val & 0x800F0000; return;
    case 0x04004104: NDMAs[4]->SrcAddr = val & 0xFFFFFFFC; return;
    case 0x04004108: NDMAs[4]->DstAddr = val & 0xFFFFFFFC; return;
    case 0x0400410C: NDMAs[4]->TotalLength = val & 0x0FFFFFFF; return;
    case 0x04004110: NDMAs[4]->BlockLength = val & 0x00FFFFFF; return;
    case 0x04004114: NDMAs[4]->SubblockTimer = val & 0x0003FFFF; return;
    case 0x04004118: NDMAs[4]->FillData = val; return;
    case 0x0400411C: NDMAs[4]->WriteCnt(val); return;
    case 0x04004120: NDMAs[5]->SrcAddr = val & 0xFFFFFFFC; return;
    case 0x04004124: NDMAs[5]->DstAddr = val & 0xFFFFFFFC; return;
    case 0x04004128: NDMAs[5]->TotalLength = val & 0x0FFFFFFF; return;
    case 0x0400412C: NDMAs[5]->BlockLength = val & 0x00FFFFFF; return;
    case 0x04004130: NDMAs[5]->SubblockTimer = val & 0x0003FFFF; return;
    case 0x04004134: NDMAs[5]->FillData = val; return;
    case 0x04004138: NDMAs[5]->WriteCnt(val); return;
    case 0x0400413C: NDMAs[6]->SrcAddr = val & 0xFFFFFFFC; return;
    case 0x04004140: NDMAs[6]->DstAddr = val & 0xFFFFFFFC; return;
    case 0x04004144: NDMAs[6]->TotalLength = val & 0x0FFFFFFF; return;
    case 0x04004148: NDMAs[6]->BlockLength = val & 0x00FFFFFF; return;
    case 0x0400414C: NDMAs[6]->SubblockTimer = val & 0x0003FFFF; return;
    case 0x04004150: NDMAs[6]->FillData = val; return;
    case 0x04004154: NDMAs[6]->WriteCnt(val); return;
    case 0x04004158: NDMAs[7]->SrcAddr = val & 0xFFFFFFFC; return;
    case 0x0400415C: NDMAs[7]->DstAddr = val & 0xFFFFFFFC; return;
    case 0x04004160: NDMAs[7]->TotalLength = val & 0x0FFFFFFF; return;
    case 0x04004164: NDMAs[7]->BlockLength = val & 0x00FFFFFF; return;
    case 0x04004168: NDMAs[7]->SubblockTimer = val & 0x0003FFFF; return;
    case 0x0400416C: NDMAs[7]->FillData = val; return;
    case 0x04004170: NDMAs[7]->WriteCnt(val); return;

    case 0x04004400: DSi_AES::WriteCnt(val); return;
    case 0x04004404: DSi_AES::WriteBlkCnt(val); return;
    case 0x04004408: DSi_AES::WriteInputFIFO(val); return;

    case 0x4004700:
        Log(LogLevel::Debug, "32-Bit SNDExCnt write? %08X %08X\n", val, NDS::ARM7->R[15]);
        DSi_DSP::WriteSNDExCnt(val);
        return;
    }

    if (addr >= 0x04004420 && addr < 0x04004430)
    {
        addr -= 0x04004420;
        DSi_AES::WriteIV(addr, val, 0xFFFFFFFF);
        return;
    }
    if (addr >= 0x04004430 && addr < 0x04004440)
    {
        addr -= 0x04004430;
        DSi_AES::WriteMAC(addr, val, 0xFFFFFFFF);
        return;
    }
    if (addr >= 0x04004440 && addr < 0x04004500)
    {
        addr -= 0x04004440;
        int n = 0;
        while (addr >= 0x30) { addr -= 0x30; n++; }

        switch (addr >> 4)
        {
        case 0: DSi_AES::WriteKeyNormal(n, addr&0xF, val, 0xFFFFFFFF); return;
        case 1: DSi_AES::WriteKeyX(n, addr&0xF, val, 0xFFFFFFFF); return;
        case 2: DSi_AES::WriteKeyY(n, addr&0xF, val, 0xFFFFFFFF); return;
        }
    }

    if (addr >= 0x04004800 && addr < 0x04004A00)
    {
        if (addr == 0x0400490C) { SDMMC->WriteFIFO32(val); return; }
        SDMMC->Write(addr, val & 0xFFFF);
        SDMMC->Write(addr+2, val >> 16);
        return;
    }
    if (addr >= 0x04004A00 && addr < 0x04004C00)
    {
        if (addr == 0x04004B0C) { SDIO->WriteFIFO32(val); return; }
        SDIO->Write(addr, val & 0xFFFF);
        SDIO->Write(addr+2, val >> 16);
        return;
    }


    if (addr >= 0x04004300 && addr <= 0x04004400)
    {
        DSi_DSP::Write32(addr, val);
        return;
    }

    return NDS::ARM7IOWrite32(addr, val);
}

}
