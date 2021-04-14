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

#ifndef NDSCART_H
#define NDSCART_H

#include "types.h"

// required class design
// * interface bits (init/reset/writeregs) static
// * cart-specific bits in class
//  * responding to cart commands
//  * forwarding response data
//  * forwarding cart IRQ (for ie. pokémon typing cart, or when ejecting cart)
//  * responding to SPI commands
//  * saveRAM management (interface between emulated SRAM interface and save file)
//  * DLDI shito (for homebrew)

/*class NDSCart
{
public:
    static bool Init();
    static void DeInit();
    static void Reset;

    static void DoSavestate(Savestate* file);

    static bool LoadROM(const char* path, const char* sram, bool direct);
    static bool LoadROM(const u8* romdata, u32 filelength, const char *sram, bool direct);

    // direct boot support
    static void DecryptSecureArea(u8* out);

    static u16 SPICnt;
    static u32 ROMCnt;

    static u8 ROMCommand[8];

    static bool CartInserted;
    static u8* CartROM;
    static u32 CartROMSize;
    static u32 CartID;

    //

private:
    // private parts go here
};*/

namespace NDSCart
{

// CartCommon -- base code shared by all cart types
class CartCommon
{
public:
    CartCommon(u8* rom, u32 len, u32 chipid);
    virtual ~CartCommon();

    virtual void Reset();
    virtual void SetupDirectBoot();

    virtual void DoSavestate(Savestate* file);

    virtual void LoadSave(const char* path, u32 type);
    virtual void RelocateSave(const char* path, bool write);
    virtual void FlushSRAMFile();

    virtual int ROMCommandStart(u8* cmd, u8* data, u32 len);
    virtual void ROMCommandFinish(u8* cmd, u8* data, u32 len);

    virtual u8 SPIWrite(u8 val, u32 pos, bool last);

protected:
    void ReadROM(u32 addr, u32 len, u8* data, u32 offset);

    u8* ROM;
    u32 ROMLength;
    u32 ChipID;
    bool IsDSi;

    int CmdEncMode;
    int DataEncMode;
};

// CartRetail -- regular retail cart (ROM, SPI SRAM)
class CartRetail : public CartCommon
{
public:
    CartRetail(u8* rom, u32 len, u32 chipid);
    virtual ~CartRetail();

    virtual void Reset();

    virtual void DoSavestate(Savestate* file);

    virtual void LoadSave(const char* path, u32 type);
    virtual void RelocateSave(const char* path, bool write);
    virtual void FlushSRAMFile();

    virtual int ROMCommandStart(u8* cmd, u8* data, u32 len);

    virtual u8 SPIWrite(u8 val, u32 pos, bool last);

protected:
    void ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset);

    u8 SRAMWrite_EEPROMTiny(u8 val, u32 pos, bool last);
    u8 SRAMWrite_EEPROM(u8 val, u32 pos, bool last);
    u8 SRAMWrite_FLASH(u8 val, u32 pos, bool last);

    u8* SRAM;
    u32 SRAMLength;
    u32 SRAMType;

    char SRAMPath[1024];
    bool SRAMFileDirty;

    u8 SRAMCmd;
    u32 SRAMAddr;
    u8 SRAMStatus;
};

// CartRetailNAND -- retail cart with NAND SRAM (WarioWare DIY, Jam with the Band, ...)
class CartRetailNAND : public CartRetail
{
public:
    CartRetailNAND(u8* rom, u32 len, u32 chipid);
    ~CartRetailNAND();

    void Reset();

    void DoSavestate(Savestate* file);

    void LoadSave(const char* path, u32 type);

    int ROMCommandStart(u8* cmd, u8* data, u32 len);
    void ROMCommandFinish(u8* cmd, u8* data, u32 len);

    u8 SPIWrite(u8 val, u32 pos, bool last);

private:
    u32 SRAMBase;
    u32 SRAMWindow;

    u8 SRAMWriteBuffer[0x800];
    u32 SRAMWritePos;
};

// CartRetailIR -- SPI IR device and SRAM
class CartRetailIR : public CartRetail
{
public:
    CartRetailIR(u8* rom, u32 len, u32 chipid);
    ~CartRetailIR();

    void Reset();

    void DoSavestate(Savestate* file);

    u8 SPIWrite(u8 val, u32 pos, bool last);

private:
    u8 IRCmd;
};

// CartRetailBT - Pokémon Typing Adventure (SPI BT controller)
class CartRetailBT : public CartRetail
{
public:
    CartRetailBT(u8* rom, u32 len, u32 chipid);
    ~CartRetailBT();

    void Reset();

    void DoSavestate(Savestate* file);

    u8 SPIWrite(u8 val, u32 pos, bool last);
};

// CartHomebrew -- homebrew 'cart' (no SRAM, DLDI)
class CartHomebrew : public CartCommon
{
public:
    CartHomebrew(u8* rom, u32 len, u32 chipid);
    ~CartHomebrew();

    void Reset();

    void DoSavestate(Savestate* file);

    int ROMCommandStart(u8* cmd, u8* data, u32 len);
    void ROMCommandFinish(u8* cmd, u8* data, u32 len);

private:
    void ApplyDLDIPatch(const u8* patch, u32 len);
    void ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset);

    FILE* SDFile;
};

extern u16 SPICnt;
extern u32 ROMCnt;

extern u8 ROMCommand[8];

extern u8* CartROM;
extern u32 CartROMSize;

extern u32 CartID;

bool Init();
void DeInit();
void Reset();

void DoSavestate(Savestate* file);

void DecryptSecureArea(u8* out);
bool LoadROM(const char* path, const char* sram, bool direct);
bool LoadROM(const u8* romdata, u32 filelength, const char *sram, bool direct);

void FlushSRAMFile();

void RelocateSave(const char* path, bool write);

int ImportSRAM(const u8* data, u32 length);

void ResetCart();

void WriteROMCnt(u32 val);
u32 ReadROMData();
void WriteROMData(u32 val);

void WriteSPICnt(u16 val);
u8 ReadSPIData();
void WriteSPIData(u8 val);

void ROMPrepareData(u32 param);
void ROMEndTransfer(u32 param);
void SPITransferDone(u32 param);

}

#endif
