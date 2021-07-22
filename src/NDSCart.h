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
#include "NDS_Header.h"

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
    virtual int ImportSRAM(const u8* data, u32 length);
    virtual void FlushSRAMFile();

    virtual int ROMCommandStart(u8* cmd, u8* data, u32 len);
    virtual void ROMCommandFinish(u8* cmd, u8* data, u32 len);

    virtual u8 SPIWrite(u8 val, u32 pos, bool last);

protected:
    void ReadROM(u32 addr, u32 len, u8* data, u32 offset);

    void SetIRQ();

    u8* ROM;
    u32 ROMLength;
    u32 ChipID;
    bool IsDSi;
    bool DSiMode;
    u32 DSiBase;

    u32 CmdEncMode;
    u32 DataEncMode;
};

// CartRetail -- regular retail cart (ROM, SPI SRAM)
class CartRetail : public CartCommon
{
public:
    CartRetail(u8* rom, u32 len, u32 chipid);
    virtual ~CartRetail() override;

    virtual void Reset() override;

    virtual void DoSavestate(Savestate* file) override;

    virtual void LoadSave(const char* path, u32 type) override;
    virtual void RelocateSave(const char* path, bool write) override;
    virtual int ImportSRAM(const u8* data, u32 length) override;
    virtual void FlushSRAMFile() override;

    virtual int ROMCommandStart(u8* cmd, u8* data, u32 len) override;

    virtual u8 SPIWrite(u8 val, u32 pos, bool last) override;

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
    ~CartRetailNAND() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void LoadSave(const char* path, u32 type) override;
    int ImportSRAM(const u8* data, u32 length) override;

    int ROMCommandStart(u8* cmd, u8* data, u32 len) override;
    void ROMCommandFinish(u8* cmd, u8* data, u32 len) override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;

private:
    void BuildSRAMID();

    u32 SRAMBase;
    u32 SRAMWindow;

    u8 SRAMWriteBuffer[0x800];
    u32 SRAMWritePos;
};

// CartRetailIR -- SPI IR device and SRAM
class CartRetailIR : public CartRetail
{
public:
    CartRetailIR(u8* rom, u32 len, u32 chipid, u32 irversion);
    ~CartRetailIR() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;

private:
    u32 IRVersion;
    u8 IRCmd;
};

// CartRetailBT - Pokémon Typing Adventure (SPI BT controller)
class CartRetailBT : public CartRetail
{
public:
    CartRetailBT(u8* rom, u32 len, u32 chipid);
    ~CartRetailBT() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    u8 SPIWrite(u8 val, u32 pos, bool last) override;
};

// CartHomebrew -- homebrew 'cart' (no SRAM, DLDI)
class CartHomebrew : public CartCommon
{
public:
    CartHomebrew(u8* rom, u32 len, u32 chipid);
    ~CartHomebrew() override;

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    int ROMCommandStart(u8* cmd, u8* data, u32 len) override;
    void ROMCommandFinish(u8* cmd, u8* data, u32 len) override;

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

extern NDSHeader Header;
extern NDSBanner Banner;

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
