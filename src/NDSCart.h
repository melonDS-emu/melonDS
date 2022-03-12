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

#ifndef NDSCART_H
#define NDSCART_H

#include <string>

#include "types.h"
#include "NDS_Header.h"
#include "FATStorage.h"

namespace NDSCart
{

// CartCommon -- base code shared by all cart types
class CartCommon
{
public:
    CartCommon(u8* rom, u32 len, u32 chipid);
    virtual ~CartCommon();

    virtual u32 Type() { return 0x001; }
    virtual u32 Checksum();

    virtual void Reset();
    virtual void SetupDirectBoot(std::string romname);

    virtual void DoSavestate(Savestate* file);

    virtual void SetupSave(u32 type);
    virtual void LoadSave(const u8* savedata, u32 savelen);

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

    virtual u32 Type() override { return 0x101; }

    virtual void Reset() override;

    virtual void DoSavestate(Savestate* file) override;

    virtual void SetupSave(u32 type) override;
    virtual void LoadSave(const u8* savedata, u32 savelen) override;

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

    u8 SRAMCmd;
    u32 SRAMAddr;
    u32 SRAMFirstAddr;
    u8 SRAMStatus;
};

// CartRetailNAND -- retail cart with NAND SRAM (WarioWare DIY, Jam with the Band, ...)
class CartRetailNAND : public CartRetail
{
public:
    CartRetailNAND(u8* rom, u32 len, u32 chipid);
    ~CartRetailNAND() override;

    virtual u32 Type() override { return 0x102; }

    void Reset() override;

    void DoSavestate(Savestate* file) override;

    void LoadSave(const u8* savedata, u32 savelen) override;

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

    virtual u32 Type() override { return 0x103; }

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

    virtual u32 Type() override { return 0x104; }

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

    virtual u32 Type() override { return 0x201; }

    void Reset() override;
    void SetupDirectBoot(std::string romname) override;

    void DoSavestate(Savestate* file) override;

    int ROMCommandStart(u8* cmd, u8* data, u32 len) override;
    void ROMCommandFinish(u8* cmd, u8* data, u32 len) override;

private:
    void ApplyDLDIPatchAt(u8* binary, u32 dldioffset, const u8* patch, u32 patchlen, bool readonly);
    void ApplyDLDIPatch(const u8* patch, u32 patchlen, bool readonly);
    void ReadROM_B7(u32 addr, u32 len, u8* data, u32 offset);

    FATStorage* SD;
    bool ReadOnly;
};

extern u16 SPICnt;
extern u32 ROMCnt;

extern u8 ROMCommand[8];

extern bool CartInserted;
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

bool LoadROM(const u8* romdata, u32 romlen);
void LoadSave(const u8* savedata, u32 savelen);
void SetupDirectBoot(std::string romname);

void EjectCart();

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
