/*
    Copyright 2016-2025 melonDS team

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

#include <string.h>
#include "NDS.h"
#include "GPU.h"

#include "ARMJIT.h"

#include "GPU_Soft.h"

namespace melonDS
{
using Platform::Log;
using Platform::LogLevel;

#define LINE_CYCLES  (355*6)
#define HBLANK_CYCLES (48+(256*6))
#define FRAME_CYCLES  (LINE_CYCLES * 263)

enum
{
    LCD_StartHBlank = 0,
    LCD_StartScanline,
    LCD_FinishFrame,
};

// flags for VRAM blocks that can serve for display captures
// each block is 32K, thus each of banks A/B/C/D contains 4 blocks
enum
{
    CBFlag_IsCapture    = (1<<15), // the contents of this block are a display capture
    CBFlag_Complete     = (1<<14), // this block contains a complete capture (not in progress)
    CBFlag_Synced       = (1<<13), // this block has been synced back to emulated VRAM
};


/*
    VRAM invalidation tracking

    - we want to know when a VRAM region used for graphics changed
    - for some regions unmapping is mandatory to modify them (Texture, TexPal and ExtPal) and
        we don't want to completely invalidate them every time they're unmapped and remapped

    For this reason we don't track the dirtyness per mapping region, but instead per VRAM bank
    with VRAMDirty.

    This is more or less a description of VRAMTrackingSet::DeriveState
        Each time before the memory is read two things could have happened
        to each 16kb piece (16kb is the smallest unit in which mappings can
        be made thus also the size VRAMMap_* use):
            - this piece was remapped compared to last time we checked,
                which means this location in memory is invalid.
            - this piece wasn't remapped, which means we need to check whether
                it was changed. This can be archived by checking VRAMDirty.
                VRAMDirty need to be reset for the respective VRAM bank.
*/

GPU::GPU(melonDS::NDS& nds, std::unique_ptr<Renderer>&& renderer) noexcept :
    NDS(nds),
    GPU2D_A(0, *this),
    GPU2D_B(1, *this),
    GPU3D(*this)
{
    NDS.RegisterEventFuncs(Event_LCD, this,
    {
        MakeEventThunk(GPU, StartHBlank),
        MakeEventThunk(GPU, StartScanline),
        MakeEventThunk(GPU, FinishFrame)
    });
    NDS.RegisterEventFuncs(Event_DisplayFIFO, this, {MakeEventThunk(GPU, DisplayFIFO)});

    SetRenderer(std::move(renderer));
}

GPU::~GPU() noexcept
{
    // All unique_ptr fields are automatically cleaned up

    NDS.UnregisterEventFuncs(Event_LCD);
    NDS.UnregisterEventFuncs(Event_DisplayFIFO);
}

void GPU::ResetVRAMCache() noexcept
{
    for (int i = 0; i < 9; i++)
        VRAMDirty[i] = NonStupidBitField<128*1024/VRAMDirtyGranularity>();

    VRAMDirty_ABG.Reset();
    VRAMDirty_BBG.Reset();
    VRAMDirty_AOBJ.Reset();
    VRAMDirty_BOBJ.Reset();
    VRAMDirty_ABGExtPal.Reset();
    VRAMDirty_BBGExtPal.Reset();
    VRAMDirty_AOBJExtPal.Reset();
    VRAMDirty_BOBJExtPal.Reset();
    VRAMDirty_Texture.Reset();
    VRAMDirty_TexPal.Reset();

    memset(VRAMFlat_ABG, 0, sizeof(VRAMFlat_ABG));
    memset(VRAMFlat_BBG, 0, sizeof(VRAMFlat_BBG));
    memset(VRAMFlat_AOBJ, 0, sizeof(VRAMFlat_AOBJ));
    memset(VRAMFlat_BOBJ, 0, sizeof(VRAMFlat_BOBJ));
    memset(VRAMFlat_ABGExtPal, 0, sizeof(VRAMFlat_ABGExtPal));
    memset(VRAMFlat_BBGExtPal, 0, sizeof(VRAMFlat_BBGExtPal));
    memset(VRAMFlat_AOBJExtPal, 0, sizeof(VRAMFlat_AOBJExtPal));
    memset(VRAMFlat_BOBJExtPal, 0, sizeof(VRAMFlat_BOBJExtPal));
    memset(VRAMFlat_Texture, 0, sizeof(VRAMFlat_Texture));
    memset(VRAMFlat_TexPal, 0, sizeof(VRAMFlat_TexPal));
}

void GPU::Reset() noexcept
{
    ScreensEnabled = false;
    ScreenSwap = false;

    VCount = 0;
    VCountOverride = false;
    NextVCount = 0;
    TotalScanlines = 0;

    DispStat[0] = 0;
    DispStat[1] = 0;
    VMatch[0] = 0;
    VMatch[1] = 0;

    memset(Palette, 0, 2*1024);
    memset(OAM, 0, 2*1024);

    memset(VRAM_A, 0, 128*1024);
    memset(VRAM_B, 0, 128*1024);
    memset(VRAM_C, 0, 128*1024);
    memset(VRAM_D, 0, 128*1024);
    memset(VRAM_E, 0,  64*1024);
    memset(VRAM_F, 0,  16*1024);
    memset(VRAM_G, 0,  16*1024);
    memset(VRAM_H, 0,  32*1024);
    memset(VRAM_I, 0,  16*1024);

    memset(VRAMCNT, 0, 9);
    VRAMSTAT = 0;

    VRAMMap_LCDC = 0;

    memset(VRAMMap_ABG, 0, sizeof(VRAMMap_ABG));
    memset(VRAMMap_AOBJ, 0, sizeof(VRAMMap_AOBJ));
    memset(VRAMMap_BBG, 0, sizeof(VRAMMap_BBG));
    memset(VRAMMap_BOBJ, 0, sizeof(VRAMMap_BOBJ));

    memset(VRAMMap_ABGExtPal, 0, sizeof(VRAMMap_ABGExtPal));
    VRAMMap_AOBJExtPal = 0;
    memset(VRAMMap_BBGExtPal, 0, sizeof(VRAMMap_BBGExtPal));
    VRAMMap_BOBJExtPal = 0;

    memset(VRAMMap_Texture, 0, sizeof(VRAMMap_Texture));
    memset(VRAMMap_TexPal, 0, sizeof(VRAMMap_TexPal));

    VRAMMap_ARM7[0] = 0;
    VRAMMap_ARM7[1] = 0;

    memset(VRAMPtr_ABG, 0, sizeof(VRAMPtr_ABG));
    memset(VRAMPtr_AOBJ, 0, sizeof(VRAMPtr_AOBJ));
    memset(VRAMPtr_BBG, 0, sizeof(VRAMPtr_BBG));
    memset(VRAMPtr_BOBJ, 0, sizeof(VRAMPtr_BOBJ));

    memset(VRAMCaptureBlockFlags, 0, sizeof(VRAMCaptureBlockFlags));

    memset(VRAMCBF_ABG, 0, sizeof(VRAMCBF_ABG));
    memset(VRAMCBF_AOBJ, 0, sizeof(VRAMCBF_AOBJ));
    memset(VRAMCBF_BBG, 0, sizeof(VRAMCBF_BBG));
    memset(VRAMCBF_BOBJ, 0, sizeof(VRAMCBF_BOBJ));

    GPU2D_A.Reset();
    GPU2D_B.Reset();
    GPU3D.Reset();

    Rend->Reset();

    ResetVRAMCache();

    OAMDirty = 0x3;
    PaletteDirty = 0x5F;
}

void GPU::Stop() noexcept
{
    Rend->Stop();
}

void GPU::DoSavestate(Savestate* file) noexcept
{
    file->Section("GPUG");

    Rend->PreSavestate();

    if (file->Saving)
    {
        SyncAllVRAMCaptures();
    }

    memset(VRAMCaptureBlockFlags, 0, sizeof(VRAMCaptureBlockFlags));

    file->VarBool(&ScreensEnabled);
    file->VarBool(&ScreenSwap);

    file->Var16(&VCount);
    file->VarBool(&VCountOverride);
    file->Var16(&NextVCount);
    file->Var16(&TotalScanlines);

    file->Var16(&DispStat[0]);
    file->Var16(&DispStat[1]);
    file->Var16(&VMatch[0]);
    file->Var16(&VMatch[1]);

    file->VarArray(DispFIFO, sizeof(DispFIFO));
    file->Var8(&DispFIFOReadPtr);
    file->Var8(&DispFIFOWritePtr);
    file->VarArray(DispFIFOBuffer, sizeof(DispFIFOBuffer));

    file->Var16(&MasterBrightnessA);
    file->Var16(&MasterBrightnessB);

    file->Var32(&CaptureCnt);
    file->VarBool(&CaptureEnable);

    file->VarArray(Palette, 2*1024);
    file->VarArray(OAM, 2*1024);

    file->VarArray(VRAM_A, 128*1024);
    file->VarArray(VRAM_B, 128*1024);
    file->VarArray(VRAM_C, 128*1024);
    file->VarArray(VRAM_D, 128*1024);
    file->VarArray(VRAM_E,  64*1024);
    file->VarArray(VRAM_F,  16*1024);
    file->VarArray(VRAM_G,  16*1024);
    file->VarArray(VRAM_H,  32*1024);
    file->VarArray(VRAM_I,  16*1024);

    file->VarArray(VRAMCNT, 9);
    file->Var8(&VRAMSTAT);

    file->Var32(&VRAMMap_LCDC);

    file->VarArray(VRAMMap_ABG, sizeof(VRAMMap_ABG));
    file->VarArray(VRAMMap_AOBJ, sizeof(VRAMMap_AOBJ));
    file->VarArray(VRAMMap_BBG, sizeof(VRAMMap_BBG));
    file->VarArray(VRAMMap_BOBJ, sizeof(VRAMMap_BOBJ));

    file->VarArray(VRAMMap_ABGExtPal, sizeof(VRAMMap_ABGExtPal));
    file->Var32(&VRAMMap_AOBJExtPal);
    file->VarArray(VRAMMap_BBGExtPal, sizeof(VRAMMap_BBGExtPal));
    file->Var32(&VRAMMap_BOBJExtPal);

    file->VarArray(VRAMMap_Texture, sizeof(VRAMMap_Texture));
    file->VarArray(VRAMMap_TexPal, sizeof(VRAMMap_TexPal));

    file->Var32(&VRAMMap_ARM7[0]);
    file->Var32(&VRAMMap_ARM7[1]);

    if (!file->Saving)
    {
        for (int i = 0; i < 0x20; i++)
        {
            VRAMPtr_ABG[i] = GetUniqueBankPtr(VRAMMap_ABG[i], i << 14);
            VRAMCBF_ABG[i] = GetUniqueBankCBF(VRAMMap_ABG[i], i);
        }
        for (int i = 0; i < 0x10; i++)
        {
            VRAMPtr_AOBJ[i] = GetUniqueBankPtr(VRAMMap_AOBJ[i], i << 14);
            VRAMCBF_AOBJ[i] = GetUniqueBankCBF(VRAMMap_AOBJ[i], i);
        }
        for (int i = 0; i < 0x8; i++)
        {
            VRAMPtr_BBG[i] = GetUniqueBankPtr(VRAMMap_BBG[i], i << 14);
            VRAMCBF_BBG[i] = GetUniqueBankCBF(VRAMMap_BBG[i], i);
        }
        for (int i = 0; i < 0x8; i++)
        {
            VRAMPtr_BOBJ[i] = GetUniqueBankPtr(VRAMMap_BOBJ[i], i << 14);
            VRAMCBF_BOBJ[i] = GetUniqueBankCBF(VRAMMap_BOBJ[i], i);
        }
    }

    GPU2D_A.DoSavestate(file);
    GPU2D_B.DoSavestate(file);
    GPU3D.DoSavestate(file);

    if (!file->Saving)
    {
        ResetVRAMCache();
        OAMDirty = 0x3;
        PaletteDirty = 0x5F;
    }

    Rend->PostSavestate();
}


void GPU::SetRenderer(std::unique_ptr<Renderer>&& renderer) noexcept
{
    SyncAllVRAMCaptures();

    bool good = false;
    if (renderer)
    {
        Rend = std::move(renderer);
        if (Rend->Init())
        {
            Rend->Reset();
            good = true;
        }
        else
        {
            // TODO: report error to platform
        }
    }

    if (!good)
    {
        Rend = std::make_unique<SoftRenderer>(NDS);
        Rend->Init();
        Rend->Reset();
    }

    ResetVRAMCache();
    OAMDirty = 0x3;
    PaletteDirty = 0x5F;
}


bool GPU::GetFramebuffers(void** top, void** bottom)
{
    return Rend->GetFramebuffers(top, bottom);
}


u8 GPU::Read8(u32 addr)
{
    u16 ret = Read16(addr & ~0x1);
    if (addr & 0x1)
        return ret >> 8;
    else
        return ret & 0xFF;
}

u16 GPU::Read16(u32 addr)
{
    switch (addr)
    {
        case 0x04000064: return CaptureCnt & 0xFFFF;
        case 0x04000066: return CaptureCnt >> 16;

        case 0x0400006C: return MasterBrightnessA;
        case 0x0400106C: return MasterBrightnessB;
    }

    Log(LogLevel::Debug, "unknown GPU read16 %08X\n", addr);
    return 0;
}

u32 GPU::Read32(u32 addr)
{
    switch (addr)
    {
        case 0x04000064: return CaptureCnt;

        case 0x0400006C: return MasterBrightnessA;
        case 0x0400106C: return MasterBrightnessB;
    }

    Log(LogLevel::Debug, "unknown GPU read32 %08X\n", addr);
    return 0;
}

void GPU::Write8(u32 addr, u8 val)
{
    switch (addr)
    {
        case 0x04000004:
            SetDispStat(0, val, 0x00FF);
            return;
        case 0x04000005:
            SetDispStat(0, val << 8, 0xFF00);
            return;
        case 0x04000006:
            SetVCount(val, 0x00FF);
            return;
        case 0x04000007:
            SetVCount(val << 8, 0xFF00);
            return;

        case 0x04000064:
            CaptureCnt = (CaptureCnt & 0xFFFFFF00) | (val & 0x1F);
            return;
        case 0x04000065:
            CaptureCnt = (CaptureCnt & 0xFFFF00FF) | ((val & 0x1F) << 8);
            return;
        case 0x04000066:
            CaptureCnt = (CaptureCnt & 0xFF00FFFF) | ((val & 0x3F) << 16);
            return;
        case 0x04000067:
            CaptureCnt = (CaptureCnt & 0x00FFFFFF) | ((val & 0xEF) << 24);
            return;

        case 0x04000068:
            DispFIFO[DispFIFOWritePtr] = val * 0x0101;
            return;
        case 0x04000069:
            return;
        case 0x0400006A:
            DispFIFO[DispFIFOWritePtr+1] = val * 0x0101;
            return;
        case 0x0400006B:
            DispFIFOWritePtr += 2;
            DispFIFOWritePtr &= 0xF;
            return;

        case 0x0400006C:
            MasterBrightnessA = (MasterBrightnessA & 0xFF00) | (val & 0x1F);
            return;
        case 0x0400006D:
            MasterBrightnessA = (MasterBrightnessA & 0x00FF) | ((val & 0xC0) << 8);
            return;
        case 0x0400106C:
            MasterBrightnessB = (MasterBrightnessB & 0xFF00) | (val & 0x1F);
            return;
        case 0x0400106D:
            MasterBrightnessB = (MasterBrightnessB & 0x00FF) | ((val & 0xC0) << 8);
            return;
    }

    Log(LogLevel::Debug, "unknown GPU write8 %08X %02X\n", addr, val);
}

void GPU::Write16(u32 addr, u16 val)
{
    switch (addr)
    {
        case 0x04000004:
            SetDispStat(0, val, 0xFFFF);
            return;
        case 0x04000006:
            SetVCount(val, 0xFFFF);
            return;

        case 0x04000064:
            CaptureCnt = (CaptureCnt & 0xFFFF0000) | (val & 0x1F1F);
            return;
        case 0x04000066:
            CaptureCnt = (CaptureCnt & 0xFFFF) | ((val & 0xEF3F) << 16);
            return;

        case 0x04000068:
            DispFIFO[DispFIFOWritePtr] = val;
            return;
        case 0x0400006A:
            DispFIFO[DispFIFOWritePtr+1] = val;
            DispFIFOWritePtr += 2;
            DispFIFOWritePtr &= 0xF;
            return;

        case 0x0400006C:
            MasterBrightnessA = val & 0xC01F;
            return;
        case 0x0400106C:
            MasterBrightnessB = val & 0xC01F;
            return;
    }

    Log(LogLevel::Debug, "unknown GPU write16 %08X %04X\n", addr, val);
}

void GPU::Write32(u32 addr, u32 val)
{
    switch (addr)
    {
        case 0x04000004:
            SetDispStat(0, val & 0xFFFF, 0xFFFF);
            SetVCount(val >> 16, 0xFFFF);
            return;

        case 0x04000064:
            CaptureCnt = val & 0xEF3F1F1F;
            return;

        case 0x04000068:
            DispFIFO[DispFIFOWritePtr] = val & 0xFFFF;
            DispFIFO[DispFIFOWritePtr+1] = val >> 16;
            DispFIFOWritePtr += 2;
            DispFIFOWritePtr &= 0xF;
            return;

        case 0x0400006C:
            MasterBrightnessA = val & 0xC01F;
            return;
        case 0x0400106C:
            MasterBrightnessB = val & 0xC01F;
            return;
    }

    Log(LogLevel::Debug, "unknown GPU write32 %08X %08X\n", addr, val);
}


// VRAM mapping notes
//
// mirroring:
// unmapped range reads zero
// LCD is mirrored every 0x100000 bytes, the gap between each mirror reads zero
// ABG:
//   bank A,B,C,D,E mirror every 0x80000 bytes
//   bank F,G mirror at base+0x8000, mirror every 0x80000 bytes
// AOBJ:
//   bank A,B,E mirror every 0x40000 bytes
//   bank F,G mirror at base+0x8000, mirror every 0x40000 bytes
// BBG:
//   bank C mirrors every 0x20000 bytes
//   bank H mirrors every 0x10000 bytes
//   bank I mirrors at base+0x4000, mirrors every 0x10000 bytes
// BOBJ:
//   bank D mirrors every 0x20000 bytes
//   bank I mirrors every 0x4000 bytes
//
// untested:
//   ARM7 (TODO)
//   extended palette (mirroring doesn't apply)
//   texture/texpal (does mirroring apply?)
//   -> trying to use extpal/texture/texpal with no VRAM mapped.
//      would likely read all black, but has to be tested.
//
// overlap:
// when reading: values are read from each bank and ORed together
// when writing: value is written to each bank

u8* GPU::GetUniqueBankPtr(u32 mask, u32 offset) noexcept
{
    if (!mask || (mask & (mask - 1)) != 0) return NULL;
    int num = __builtin_ctz(mask);
    return &VRAM[num][offset & VRAMMask[num]];
}

const u8* GPU::GetUniqueBankPtr(u32 mask, u32 offset) const noexcept
{
    if (!mask || (mask & (mask - 1)) != 0) return NULL;
    int num = __builtin_ctz(mask);
    return &VRAM[num][offset & VRAMMask[num]];
}

u16* GPU::GetUniqueBankCBF(u32 mask, u32 offset)
{
    //mask &= 0xF;
    if (!mask || (mask & (mask - 1)) != 0) return nullptr;
    if (mask & 0x1F0) return nullptr;
    int num = __builtin_ctz(mask);
    offset = (offset >> 1) & 0x3;
    return &VRAMCaptureBlockFlags[(num << 2) | offset];
}

#define MAP_RANGE(map, base, n)    for (int i = 0; i < n; i++) VRAMMap_##map[(base)+i] |= bankmask;
#define UNMAP_RANGE(map, base, n)  for (int i = 0; i < n; i++) VRAMMap_##map[(base)+i] &= ~bankmask;

#define MAP_RANGE_PTR(map, base, n) \
    for (int i = 0; i < n; i++) { VRAMMap_##map[(base)+i] |= bankmask; VRAMPtr_##map[(base)+i] = GetUniqueBankPtr(VRAMMap_##map[(base)+i], ((base)+i)<<14); }
#define UNMAP_RANGE_PTR(map, base, n) \
    for (int i = 0; i < n; i++) { VRAMMap_##map[(base)+i] &= ~bankmask; VRAMPtr_##map[(base)+i] = GetUniqueBankPtr(VRAMMap_##map[(base)+i], ((base)+i)<<14); }

#define SET_RANGE_CBF(map, base) \
    for (int i = 0; i < 8; i++) { VRAMCBF_##map[(base)+i] = GetUniqueBankCBF(VRAMMap_##map[(base)+i], ((base)+i)); }

void GPU::MapVRAM_AB(u32 bank, u8 cnt) noexcept
{
    cnt &= 0x9B;

    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u8 oldofs = (oldcnt >> 3) & 0x3;
    u8 ofs = (cnt >> 3) & 0x3;
    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // ABG
            UNMAP_RANGE_PTR(ABG, oldofs<<3, 8);
            SET_RANGE_CBF(ABG, oldofs<<3);
            break;

        case 2: // AOBJ
            oldofs &= 0x1;
            UNMAP_RANGE_PTR(AOBJ, oldofs<<3, 8);
            SET_RANGE_CBF(AOBJ, oldofs<<3);
            break;

        case 3: // texture
            VRAMMap_Texture[oldofs] &= ~bankmask;
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // ABG
            MAP_RANGE_PTR(ABG, ofs<<3, 8);
            SET_RANGE_CBF(ABG, ofs<<3);
            break;

        case 2: // AOBJ
            ofs &= 0x1;
            MAP_RANGE_PTR(AOBJ, ofs<<3, 8);
            SET_RANGE_CBF(AOBJ, ofs<<3);
            break;

        case 3: // texture
            VRAMMap_Texture[ofs] |= bankmask;
            break;
        }
    }
}

void GPU::MapVRAM_CD(u32 bank, u8 cnt) noexcept
{
    cnt &= 0x9F;

    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    VRAMSTAT &= ~(1 << (bank-2));

    if (oldcnt == cnt) return;

    u8 oldofs = (oldcnt >> 3) & 0x7;
    u8 ofs = (cnt >> 3) & 0x7;
    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // ABG
            UNMAP_RANGE_PTR(ABG, oldofs<<3, 8);
            SET_RANGE_CBF(ABG, oldofs<<3);
            break;

        case 2: // ARM7 VRAM
            oldofs &= 0x1;
            VRAMMap_ARM7[oldofs] &= ~bankmask;
            break;

        case 3: // texture
            VRAMMap_Texture[oldofs] &= ~bankmask;
            break;

        case 4: // BBG/BOBJ
            if (bank == 2)
            {
                UNMAP_RANGE_PTR(BBG, 0, 8);
                SET_RANGE_CBF(BBG, 0);
            }
            else
            {
                UNMAP_RANGE_PTR(BOBJ, 0, 8);
                SET_RANGE_CBF(BOBJ, 0);
            }
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // ABG
            MAP_RANGE_PTR(ABG, ofs<<3, 8);
            SET_RANGE_CBF(ABG, ofs<<3);
            break;

        case 2: // ARM7 VRAM
            ofs &= 0x1;
            VRAMMap_ARM7[ofs] |= bankmask;
            memset(VRAMDirty[bank].Data, 0xFF, sizeof(VRAMDirty[bank].Data));
            VRAMSTAT |= (1 << (bank-2));
            NDS.JIT.CheckAndInvalidateWVRAM(ofs);
            break;

        case 3: // texture
            VRAMMap_Texture[ofs] |= bankmask;
            break;

        case 4: // BBG/BOBJ
            if (bank == 2)
            {
                MAP_RANGE_PTR(BBG, 0, 8);
                SET_RANGE_CBF(BBG, 0);
            }
            else
            {
                MAP_RANGE_PTR(BOBJ, 0, 8);
                SET_RANGE_CBF(BOBJ, 0);
            }
            break;
        }
    }

    // TODO sync capture blocks if we get mapped to ARM7?
}

void GPU::MapVRAM_E(u32 bank, u8 cnt) noexcept
{
    cnt &= 0x87;

    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // ABG
            UNMAP_RANGE_PTR(ABG, 0, 4);
            SET_RANGE_CBF(ABG, 0);
            break;

        case 2: // AOBJ
            UNMAP_RANGE_PTR(AOBJ, 0, 4);
            SET_RANGE_CBF(AOBJ, 0);
            break;

        case 3: // texture palette
            UNMAP_RANGE(TexPal, 0, 4);
            break;

        case 4: // ABG ext palette
            UNMAP_RANGE(ABGExtPal, 0, 4);
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // ABG
            MAP_RANGE_PTR(ABG, 0, 4);
            SET_RANGE_CBF(ABG, 0);
            break;

        case 2: // AOBJ
            MAP_RANGE_PTR(AOBJ, 0, 4);
            SET_RANGE_CBF(AOBJ, 0);
            break;

        case 3: // texture palette
            MAP_RANGE(TexPal, 0, 4);
            break;

        case 4: // ABG ext palette
            MAP_RANGE(ABGExtPal, 0, 4);
            break;
        }
    }
}

void GPU::MapVRAM_FG(u32 bank, u8 cnt) noexcept
{
    cnt &= 0x9F;

    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u8 oldofs = (oldcnt >> 3) & 0x7;
    u8 ofs = (cnt >> 3) & 0x7;
    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // ABG
            {
                u32 base = (oldofs & 0x1) + ((oldofs & 0x2) << 1);
                VRAMMap_ABG[base] &= ~bankmask;
                VRAMMap_ABG[base + 2] &= ~bankmask;
                VRAMPtr_ABG[base] = GetUniqueBankPtr(VRAMMap_ABG[base], base << 14);
                VRAMPtr_ABG[base + 2] = GetUniqueBankPtr(VRAMMap_ABG[base + 2], (base + 2) << 14);
                VRAMCBF_ABG[base] = GetUniqueBankCBF(VRAMMap_ABG[base], base);
                VRAMCBF_ABG[base + 2] = GetUniqueBankCBF(VRAMMap_ABG[base + 2], base + 2);
            }
            break;

        case 2: // AOBJ
            {
                u32 base = (oldofs & 0x1) + ((oldofs & 0x2) << 1);
                VRAMMap_AOBJ[base] &= ~bankmask;
                VRAMMap_AOBJ[base + 2] &= ~bankmask;
                VRAMPtr_AOBJ[base] = GetUniqueBankPtr(VRAMMap_AOBJ[base], base << 14);
                VRAMPtr_AOBJ[base + 2] = GetUniqueBankPtr(VRAMMap_AOBJ[base + 2], (base + 2) << 14);
                VRAMCBF_AOBJ[base] = GetUniqueBankCBF(VRAMMap_AOBJ[base], base);
                VRAMCBF_AOBJ[base + 2] = GetUniqueBankCBF(VRAMMap_AOBJ[base + 2], base + 2);
            }
            break;

        case 3: // texture palette
            VRAMMap_TexPal[(oldofs & 0x1) + ((oldofs & 0x2) << 1)] &= ~bankmask;
            break;

        case 4: // ABG ext palette
            VRAMMap_ABGExtPal[((oldofs & 0x1) << 1)] &= ~bankmask;
            VRAMMap_ABGExtPal[((oldofs & 0x1) << 1) + 1] &= ~bankmask;
            break;

        case 5: // AOBJ ext palette
            VRAMMap_AOBJExtPal &= ~bankmask;
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x7)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // ABG
            {
                u32 base = (ofs & 0x1) + ((ofs & 0x2) << 1);
                VRAMMap_ABG[base] |= bankmask;
                VRAMMap_ABG[base + 2] |= bankmask;
                VRAMPtr_ABG[base] = GetUniqueBankPtr(VRAMMap_ABG[base], base << 14);
                VRAMPtr_ABG[base + 2] = GetUniqueBankPtr(VRAMMap_ABG[base + 2], (base + 2) << 14);
                VRAMCBF_ABG[base] = GetUniqueBankCBF(VRAMMap_ABG[base], base);
                VRAMCBF_ABG[base + 2] = GetUniqueBankCBF(VRAMMap_ABG[base + 2], base + 2);
            }
            break;

        case 2: // AOBJ
            {
                u32 base = (ofs & 0x1) + ((ofs & 0x2) << 1);
                VRAMMap_AOBJ[base] |= bankmask;
                VRAMMap_AOBJ[base + 2] |= bankmask;
                VRAMPtr_AOBJ[base] = GetUniqueBankPtr(VRAMMap_AOBJ[base], base << 14);
                VRAMPtr_AOBJ[base + 2] = GetUniqueBankPtr(VRAMMap_AOBJ[base + 2], (base + 2) << 14);
                VRAMCBF_AOBJ[base] = GetUniqueBankCBF(VRAMMap_AOBJ[base], base);
                VRAMCBF_AOBJ[base + 2] = GetUniqueBankCBF(VRAMMap_AOBJ[base + 2], base + 2);
            }
            break;

        case 3: // texture palette
            VRAMMap_TexPal[(ofs & 0x1) + ((ofs & 0x2) << 1)] |= bankmask;
            break;

        case 4: // ABG ext palette
            VRAMMap_ABGExtPal[((ofs & 0x1) << 1)] |= bankmask;
            VRAMMap_ABGExtPal[((ofs & 0x1) << 1) + 1] |= bankmask;
            break;

        case 5: // AOBJ ext palette
            VRAMMap_AOBJExtPal |= bankmask;
            break;
        }
    }
}

void GPU::MapVRAM_H(u32 bank, u8 cnt) noexcept
{
    cnt &= 0x83;

    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // BBG
            for (int i : {0, 1, 4, 5})
            {
                VRAMMap_BBG[i] &= ~bankmask;
                VRAMPtr_BBG[i] = GetUniqueBankPtr(VRAMMap_BBG[i], i << 14);
                VRAMCBF_BBG[i] = GetUniqueBankCBF(VRAMMap_BBG[i], i);
            }
            break;

        case 2: // BBG ext palette
            UNMAP_RANGE(BBGExtPal, 0, 4);
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // BBG
            for (int i : {0, 1, 4, 5})
            {
                VRAMMap_BBG[i] |= bankmask;
                VRAMPtr_BBG[i] = GetUniqueBankPtr(VRAMMap_BBG[i], i << 14);
                VRAMCBF_BBG[i] = GetUniqueBankCBF(VRAMMap_BBG[i], i);
            }
            break;

        case 2: // BBG ext palette
            MAP_RANGE(BBGExtPal, 0, 4);
            break;
        }
    }
}

void GPU::MapVRAM_I(u32 bank, u8 cnt) noexcept
{
    cnt &= 0x83;

    u8 oldcnt = VRAMCNT[bank];
    VRAMCNT[bank] = cnt;

    if (oldcnt == cnt) return;

    u32 bankmask = 1 << bank;

    if (oldcnt & (1<<7))
    {
        switch (oldcnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC &= ~bankmask;
            break;

        case 1: // BBG
            for (int i : {2, 3, 6, 7})
            {
                VRAMMap_BBG[i] &= ~bankmask;
                VRAMPtr_BBG[i] = GetUniqueBankPtr(VRAMMap_BBG[i], i << 14);
                VRAMCBF_BBG[i] = GetUniqueBankCBF(VRAMMap_BBG[i], i);
            }
            break;

        case 2: // BOBJ
            UNMAP_RANGE_PTR(BOBJ, 0, 8);
            SET_RANGE_CBF(BOBJ, 0);
            break;

        case 3: // BOBJ ext palette
            VRAMMap_BOBJExtPal &= ~bankmask;
            break;
        }
    }

    if (cnt & (1<<7))
    {
        switch (cnt & 0x3)
        {
        case 0: // LCDC
            VRAMMap_LCDC |= bankmask;
            break;

        case 1: // BBG
            for (int i : {2, 3, 6, 7})
            {
                VRAMMap_BBG[i] |= bankmask;
                VRAMPtr_BBG[i] = GetUniqueBankPtr(VRAMMap_BBG[i], i << 14);
                VRAMCBF_BBG[i] = GetUniqueBankCBF(VRAMMap_BBG[i], i);
            }
            break;

        case 2: // BOBJ
            MAP_RANGE_PTR(BOBJ, 0, 8);
            SET_RANGE_CBF(BOBJ, 0);
            break;

        case 3: // BOBJ ext palette
            VRAMMap_BOBJExtPal |= bankmask;
            break;
        }
    }
}


void GPU::SetPowerCnt(u32 val) noexcept
{
    // POWCNT1 effects:
    // * bit0: asplodes hardware??? not tested.
    // * bit1: disables engine A palette and OAM (zero-filled) (TODO: affects mem timings???)
    // * bit2: disables rendering engine, resets its internal state (polygons and registers)
    // * bit3: disables geometry engine
    // * bit9: disables engine B palette, OAM and rendering (screen turns white)
    // * bit15: screen swap

    GPU2D_A.SetEnabled(val & (1<<1));
    GPU2D_B.SetEnabled(val & (1<<9));
    GPU3D.SetEnabled(val & (1<<3), val & (1<<2));

    ScreenSwap = !!(val & (1<<15));
}


void GPU::SetDispStatIRQ(int cpu, int num)
{
    u16 irqmask = (1 << num);
    u16 enablemask = (1 << (num+3));

    // DISPSTAT IRQs are edge-triggered
    // if the flag was already set, no IRQ will be triggered
    if (DispStat[cpu] & irqmask)
        return;

    DispStat[cpu] |= irqmask;
    if (DispStat[cpu] & enablemask)
        NDS.SetIRQ(cpu, num);
}


bool GPU::UsesDisplayFIFO()
{
    if (((GPU2D_A.DispCnt >> 16) & 0x3) == 3)
        return true;
    if ((CaptureCnt & (1<<25)) && ((CaptureCnt >> 29) & 0x3) != 0)
        return true;

    return false;
}

void GPU::SampleDisplayFIFO(u32 offset, u32 num)
{
    for (u32 i = 0; i < num; i++)
    {
        u16 val = DispFIFO[DispFIFOReadPtr];
        DispFIFOReadPtr++;
        DispFIFOReadPtr &= 0xF;

        DispFIFOBuffer[offset+i] = val;
    }
}

void GPU::DisplayFIFO(u32 x) noexcept
{
    // sample the FIFO
    // as this starts 16 cycles (~3 pixels) before display start,
    // we aren't aligned to the 8-pixel grid
    if (x > 0)
    {
        if (x == 8)
            SampleDisplayFIFO(0, 5);
        else
            SampleDisplayFIFO(x-11, 8);
    }

    if (x < 256)
    {
        // transfer the next 8 pixels
        NDS.CheckDMAs(0, 0x04);
        NDS.ScheduleEvent(Event_DisplayFIFO, true, 6*8, 0, x+8);
    }
    else
        SampleDisplayFIFO(253, 3); // sample the remaining pixels
}


void GPU::StartFrame() noexcept
{
    ScreensEnabled = !!(NDS.PowerControl9 & (1<<0));

    // only run the display FIFO if needed:
    // * if it is used for display or capture
    // * if we have display FIFO DMA
    RunFIFO = UsesDisplayFIFO() || NDS.DMAsInMode(0, 0x04);

    TotalScanlines = 0;
    StartScanline(0);
}

void GPU::StartHBlank(u32 line) noexcept
{
    DispStat[0] |= (1<<1);
    DispStat[1] |= (1<<1);

    bool resetregs = (VCount == 262);

    // note: this should be done around 48 cycles after the scanline start
    GPU2D_A.UpdateRegistersPreDraw(resetregs);
    GPU2D_B.UpdateRegistersPreDraw(resetregs);

    if (VCount < 192)
    {
        // draw
        // note: this should start 48 cycles after the scanline start
        if (line < 192)
            Rend->DrawScanline(line);
        if (line < 191)
            Rend->DrawSprites(line+1);

        NDS.CheckDMAs(0, 0x02);
    }
    else if (VCount == 215)
    {
        Rend->Start3DRendering();
    }
    else if (VCount == 262)
    {
        // sprites are pre-rendered one scanline in advance
        Rend->DrawSprites(0);
    }

    GPU2D_A.UpdateRegistersPostDraw(resetregs);
    GPU2D_B.UpdateRegistersPostDraw(resetregs);

    if (DispStat[0] & (1<<4)) NDS.SetIRQ(0, IRQ_HBlank);
    if (DispStat[1] & (1<<4)) NDS.SetIRQ(1, IRQ_HBlank);

    if ((VCount == 262) || (VCount == 511))
        NDS.ScheduleEvent(Event_LCD, true, (LINE_CYCLES - HBLANK_CYCLES), LCD_FinishFrame, line+1);
    else
        NDS.ScheduleEvent(Event_LCD, true, (LINE_CYCLES - HBLANK_CYCLES), LCD_StartScanline, line+1);
}

void GPU::FinishFrame(u32 lines) noexcept
{
    Rend->SwapBuffers();

    TotalScanlines = lines;

    if (GPU3D.AbortFrame)
    {
        Rend->Restart3DRendering();
        GPU3D.AbortFrame = false;
    }
}

void GPU::BlankFrame() noexcept
{
    /*int backbuf = FrontBuffer ? 0 : 1;
    int fbsize;
    if (GPU3D.IsRendererAccelerated())
        fbsize = (256*3 + 1) * 192;
    else
        fbsize = 256 * 192;

    memset(Framebuffer[backbuf][0].get(), 0, fbsize*4);
    memset(Framebuffer[backbuf][1].get(), 0, fbsize*4);

    FrontBuffer = backbuf;
    AssignFramebuffers();*/
    // TODO do it on the renderer!!

    TotalScanlines = 263;
}

void GPU::StartScanline(u32 line) noexcept
{
    /*
     * order of operations on hardware:
     * 1. VCount is incremented
     * 2. things are done based on the new value (ie. 262 is when the DISPSTAT VBlank flag is cleared)
     * 3. if VCOUNT was written to, the new value is applied
     * 4. VMatch is checked
     *
     * if VCount is set to 263 or more, it will count all the way to 511 and wrap around
     * if the 261->262 transition is skipped, the VBlank flag remains set (until the end of the next frame)
     * -> this suppresses the next VBlank IRQ
     * likely, skipping 191->192 behaves similarly
     *
     * ultimately, messing with VCount can cause a lot of weird shit, seeing as VCount controls
     * a lot of the renderer logic and the LCD sync signals.
     * certain VCount transitions can cause odd effects such as LCDs fading out.
     */

    // clear HBlank flags

    DispStat[0] &= ~(1<<1);
    DispStat[1] &= ~(1<<1);

    // update hardware status

    if (line == 0)
        VCount = 0;
    else
    {
        VCount++;
        VCount &= 0x1FF;
    }

    GPU2D_A.UpdateWindows(VCount);
    GPU2D_B.UpdateWindows(VCount);

    if (VCount >= 2 && VCount < 194)
        NDS.CheckDMAs(0, 0x03);
    else if (VCount == 194)
        NDS.StopDMAs(0, 0x03);

    if ((VCount < 192) && RunFIFO)
        NDS.ScheduleEvent(Event_DisplayFIFO, false, 32, 0, 0);

    if (VCount == 0)
    {
        if (CaptureCnt & (1<<31))
        {
            CaptureEnable = true;
            CheckCaptureStart();
        }
    }
    else if (VCount == 192)
    {
        // VBlank

        SetDispStatIRQ(0, 0);
        SetDispStatIRQ(1, 0);

        if (CaptureEnable)
            CheckCaptureEnd();

        DispFIFOReadPtr = 0;
        DispFIFOWritePtr = 0;

        // in reality rendering already finishes at line 144
        // and games might already start to modify texture memory.
        // That doesn't matter for us because we cache the entire
        // texture memory anyway and only update it before the start
        // of the next frame.
        // So we can give the rasteriser a bit more headroom
        Rend->Finish3DRendering();

        DispStat[0] |= (1<<0);
        DispStat[1] |= (1<<0);

        NDS.StopDMAs(0, 0x04);

        NDS.CheckDMAs(0, 0x01);
        NDS.CheckDMAs(1, 0x11);

        GPU3D.VBlank();

        Rend->VBlank();

        if (CaptureEnable)
        {
            CaptureCnt &= ~(1<<31);
            CaptureEnable = false;
        }
    }
    else if (VCount == 262)
    {
        // VBlank end

        DispStat[0] &= ~(1<<0);
        DispStat[1] &= ~(1<<0);
    }

    // if VCount was written to during the previous scanline, apply the new value

    if (VCountOverride)
    {
        VCount = NextVCount;
        VCountOverride = false;
    }

    // check for VCount match

    if (VCount == VMatch[0])
        SetDispStatIRQ(0, 2);
    else
        DispStat[0] &= ~(1<<2);

    if (VCount == VMatch[1])
        SetDispStatIRQ(1, 2);
    else
        DispStat[1] &= ~(1<<2);

    NDS.ScheduleEvent(Event_LCD, true, HBLANK_CYCLES, LCD_StartHBlank, line);
}


void GPU::Restart3DFrame() noexcept
{
    Rend->Restart3DRendering();
}


/*void GPU::UpdateRegisters(u32 line)
{
    if (line == 0)
    {
        if (CaptureCnt & (1<<31))
            CaptureEnable = true;
    }
    else if (line == 192)
    {
        CaptureCnt &= ~(1<<31);
        CaptureEnable = false;
    }

    GPU2D_A.UpdateRegisters(line);
    GPU2D_B.UpdateRegisters(line);
}*/


void GPU::SetDispStat(u32 cpu, u16 val, u16 mask) noexcept
{
    const u16 ro_mask = 0x0047;

    val &= (mask & ~ro_mask);
    DispStat[cpu] &= (~mask | ro_mask);
    DispStat[cpu] |= val;

    VMatch[cpu] = (DispStat[cpu] >> 8) | ((DispStat[cpu] & 0x80) << 1);
}

void GPU::SetVCount(u16 val, u16 mask) noexcept
{
    // the VCount register is 9 bits wide
    val &= mask & 0x1FF;

    // VCount write is delayed until the next scanline

    // TODO: how does the 3D engine react to VCount writes while it's rendering?
    // 3D engine seems to give up on the current frame in that situation, repeating the last two scanlines
    // TODO: also check the various DMA types that can be involved

    u16 nextvc = (NextVCount & ~mask) | (val & mask);

    GPU3D.AbortFrame |= NextVCount != nextvc;
    NextVCount = nextvc;
    VCountOverride = true;
}

template <u32 Size, u32 MappingGranularity>
NonStupidBitField<Size/VRAMDirtyGranularity> VRAMTrackingSet<Size, MappingGranularity>::DeriveState(const u32* currentMappings, GPU& gpu)
{
    NonStupidBitField<Size/VRAMDirtyGranularity> result;
    u16 banksToBeZeroed = 0;
    for (u32 i = 0; i < Size / MappingGranularity; i++)
    {
        if (currentMappings[i] != Mapping[i])
        {
            result.SetRange(i*VRAMBitsPerMapping, VRAMBitsPerMapping);
            banksToBeZeroed |= currentMappings[i];
            Mapping[i] = currentMappings[i];
        }
        else
        {
            u32 mapping = Mapping[i];

            banksToBeZeroed |= mapping;

            while (mapping != 0)
            {
                u32 num = __builtin_ctz(mapping);
                mapping &= ~(1 << num);

                // hack for **speed**
                // this could probably be done less ugly but then we would rely
                // on the compiler for vectorisation
                static_assert(VRAMDirtyGranularity == 512, "");
                if (MappingGranularity == 16*1024)
                {
                    u32 dirty = ((u32*)gpu.VRAMDirty[num].Data)[i & (gpu.VRAMMask[num] >> 14)];
                    result.Data[i / 2] |= (u64)dirty << ((i&1)*32);
                }
                else if (MappingGranularity == 8*1024)
                {
                    u16 dirty = ((u16*)gpu.VRAMDirty[num].Data)[i & (gpu.VRAMMask[num] >> 13)];
                    result.Data[i / 4] |= (u64)dirty << ((i&3)*16);
                }
                else if (MappingGranularity == 128*1024)
                {
                    result.Data[i * 4 + 0] |= gpu.VRAMDirty[num].Data[0];
                    result.Data[i * 4 + 1] |= gpu.VRAMDirty[num].Data[1];
                    result.Data[i * 4 + 2] |= gpu.VRAMDirty[num].Data[2];
                    result.Data[i * 4 + 3] |= gpu.VRAMDirty[num].Data[3];
                }
                else
                {
                    // welp
                    abort();
                }
            }
        }
    }

    while (banksToBeZeroed != 0)
    {
        u32 num = __builtin_ctz(banksToBeZeroed);
        banksToBeZeroed &= ~(1 << num);
        gpu.VRAMDirty[num].Clear();
    }

    return result;
}

template NonStupidBitField<32*1024/VRAMDirtyGranularity> VRAMTrackingSet<32*1024, 8*1024>::DeriveState(const u32*, GPU& gpu);
template NonStupidBitField<8*1024/VRAMDirtyGranularity> VRAMTrackingSet<8*1024, 8*1024>::DeriveState(const u32*, GPU& gpu);
template NonStupidBitField<512*1024/VRAMDirtyGranularity> VRAMTrackingSet<512*1024, 128*1024>::DeriveState(const u32*, GPU& gpu);
template NonStupidBitField<128*1024/VRAMDirtyGranularity> VRAMTrackingSet<128*1024, 16*1024>::DeriveState(const u32*, GPU& gpu);
template NonStupidBitField<256*1024/VRAMDirtyGranularity> VRAMTrackingSet<256*1024, 16*1024>::DeriveState(const u32*, GPU& gpu);
template NonStupidBitField<512*1024/VRAMDirtyGranularity> VRAMTrackingSet<512*1024, 16*1024>::DeriveState(const u32*, GPU& gpu);



bool GPU::MakeVRAMFlat_TextureCoherent(NonStupidBitField<512*1024/VRAMDirtyGranularity>& dirty) noexcept
{
    return CopyLinearVRAM<128*1024>(VRAMFlat_Texture, VRAMMap_Texture, dirty, &GPU::ReadVRAM_Texture<u64>);
}
bool GPU::MakeVRAMFlat_TexPalCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty) noexcept
{
    return CopyLinearVRAM<16*1024>(VRAMFlat_TexPal, VRAMMap_TexPal, dirty, &GPU::ReadVRAM_TexPal<u64>);
}

bool GPU::MakeVRAMFlat_ABGCoherent(NonStupidBitField<512*1024/VRAMDirtyGranularity>& dirty) noexcept
{
    return CopyLinearVRAM<16*1024>(VRAMFlat_ABG, VRAMMap_ABG, dirty, &GPU::ReadVRAM_ABG<u64>);
}
bool GPU::MakeVRAMFlat_BBGCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty) noexcept
{
    return CopyLinearVRAM<16*1024>(VRAMFlat_BBG, VRAMMap_BBG, dirty, &GPU::ReadVRAM_BBG<u64>);
}

bool GPU::MakeVRAMFlat_AOBJCoherent(NonStupidBitField<256*1024/VRAMDirtyGranularity>& dirty) noexcept
{
    return CopyLinearVRAM<16*1024>(VRAMFlat_AOBJ, VRAMMap_AOBJ, dirty, &GPU::ReadVRAM_AOBJ<u64>);
}
bool GPU::MakeVRAMFlat_BOBJCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty) noexcept
{
    return CopyLinearVRAM<16*1024>(VRAMFlat_BOBJ, VRAMMap_BOBJ, dirty, &GPU::ReadVRAM_BOBJ<u64>);
}

bool GPU::MakeVRAMFlat_ABGExtPalCoherent(NonStupidBitField<32*1024/VRAMDirtyGranularity>& dirty) noexcept
{
    return CopyLinearVRAM<8*1024>(VRAMFlat_ABGExtPal, VRAMMap_ABGExtPal, dirty, &GPU::ReadVRAM_ABGExtPal<u64>);
}
bool GPU::MakeVRAMFlat_BBGExtPalCoherent(NonStupidBitField<32*1024/VRAMDirtyGranularity>& dirty) noexcept
{
    return CopyLinearVRAM<8*1024>(VRAMFlat_BBGExtPal, VRAMMap_BBGExtPal, dirty, &GPU::ReadVRAM_BBGExtPal<u64>);
}

bool GPU::MakeVRAMFlat_AOBJExtPalCoherent(NonStupidBitField<8*1024/VRAMDirtyGranularity>& dirty) noexcept
{
    return CopyLinearVRAM<8*1024>(VRAMFlat_AOBJExtPal, &VRAMMap_AOBJExtPal, dirty, &GPU::ReadVRAM_AOBJExtPal<u64>);
}
bool GPU::MakeVRAMFlat_BOBJExtPalCoherent(NonStupidBitField<8*1024/VRAMDirtyGranularity>& dirty) noexcept
{
    return CopyLinearVRAM<8*1024>(VRAMFlat_BOBJExtPal, &VRAMMap_BOBJExtPal, dirty, &GPU::ReadVRAM_BOBJExtPal<u64>);
}


void GPU::VRAMCBFlagsSet(u32 bank, u32 block, u16 val)
{
    u16* cbflags = &VRAMCaptureBlockFlags[bank << 2];
    u32 start = block;//val & 0x3;
    u32 len = (val >> 4) & 0x3;

    u32 b = start;
    for (u32 i = 0; i < len; i++)
    {
        cbflags[b] = val;
        b = (b + 1) & 0x3;
    }
}

void GPU::VRAMCBFlagsClear(u32 bank, u32 block)
{
    u16* cbflags = &VRAMCaptureBlockFlags[bank << 2];
    u16 flags = cbflags[block];
    u32 start = flags & 0x3;
    u32 len = (flags >> 4) & 0x3;

    u32 b = start;
    for (u32 i = 0; i < len; i++)
    {
        cbflags[b] = 0;
        b = (b + 1) & 0x3;
    }
}

void GPU::VRAMCBFlagsOr(u32 bank, u32 block, u16 val)
{
    u16* cbflags = &VRAMCaptureBlockFlags[bank << 2];
    u16 flags = cbflags[block];
    u32 start = flags & 0x3;
    u32 len = (flags >> 4) & 0x3;

    u32 b = start;
    for (u32 i = 0; i < len; i++)
    {
        cbflags[b] |= val;
        b = (b + 1) & 0x3;
    }
}

void GPU::CheckCaptureStart()
{
    u32 dstbank = (CaptureCnt >> 16) & 0x3;
    if (!(VRAMMap_LCDC & (1<<dstbank)))
        return;

    u32 dstoff = (CaptureCnt >> 18) & 0x3;
    u32 size = (CaptureCnt >> 20) & 0x3;
    u32 len = (size == 0) ? 1 : size;

    // if needed, invalidate old captures
    u16* cbflags = &VRAMCaptureBlockFlags[dstbank << 2];
    u32 b = dstoff;
    for (u32 i = 0; i < len; i++)
    {
        u16 oldflags = cbflags[b];
        b = (b + 1) & 0x3;

        if (!(oldflags & CBFlag_IsCapture))
            continue;

        u32 oldstart = oldflags & 0x3;
        u32 oldsize = (oldflags >> 6) & 0x3;
        if (oldstart == dstoff && oldsize == size)
            continue;

        // we have an old capture here, and it was at a different offset/size
        // sync it and invalidate it

        Rend->SyncVRAMCapture(dstbank, oldstart, oldsize, (oldflags & CBFlag_Complete));
        VRAMCBFlagsClear(dstbank, oldstart);
    }

    // mark involved VRAM blocks as being a new capture
    u16 newval = CBFlag_IsCapture | dstoff | (dstbank << 2) | (len << 4) | (size << 6);
    VRAMCBFlagsSet(dstbank, dstoff, newval);
    Rend->AllocCapture(dstbank, dstoff, size);
}

void GPU::CheckCaptureEnd()
{
    // mark this capture as complete
    // TODO this will break if they change CaptureCnt during a capture
    u32 dstbank = (CaptureCnt >> 16) & 0x3;
    u32 dstoff = (CaptureCnt >> 18) & 0x3;
    u32 size = (CaptureCnt >> 20) & 0x3;

    u16 flags = VRAMCaptureBlockFlags[(dstbank << 2) | dstoff];
    if (!(flags & CBFlag_IsCapture))
        return;

    u32 oldstart = flags & 0x3;
    u32 oldsize = (flags >> 6) & 0x3;
    if (dstoff != oldstart || size != oldsize)
        return;

    VRAMCBFlagsOr(dstbank, dstoff, CBFlag_Complete);
}

void GPU::SyncVRAMCaptureBlock(u32 block, bool write)
{
    u16 flags = VRAMCaptureBlockFlags[block];
    if (!(flags & CBFlag_IsCapture)) return;

    // sync the capture which contains this block
    u32 bank = block >> 2;
    u32 start = flags & 0x3;
    u32 len = (flags >> 6) & 0x3;

    if (flags & CBFlag_Synced)
    {
        if (write)
            VRAMCBFlagsClear(bank, start);
        return;
    }

    Rend->SyncVRAMCapture(bank, start, len, (flags & CBFlag_Complete));

    if (write)
    {
        // if this block was written to by the CPU, invalidate the entire capture
        // the renderer will need to use the emulated VRAM contents
        VRAMCBFlagsClear(bank, start);
    }
    else
    {
        // if this block was simply read by the CPU, we just need to mark it as synced
        VRAMCBFlagsOr(bank, start, CBFlag_Synced);
    }
}

void GPU::SyncAllVRAMCaptures()
{
    for (u32 b = 0; b < 16; b++)
    {
        u16 flags = VRAMCaptureBlockFlags[b];
        if (!(flags & CBFlag_IsCapture))
            continue;
        if (flags & CBFlag_Synced)
            continue;

        u32 bank = b >> 2;
        u32 start = flags & 0x3;
        u32 len = (flags >> 6) & 0x3;

        Rend->SyncVRAMCapture(bank, start, len, (flags & CBFlag_Complete));
        VRAMCBFlagsClear(bank, start);
    }
}

int GPU::GetCaptureBlock_LCDC(u32 offset)
{
    u16 flags = VRAMCaptureBlockFlags[offset >> 15];
    //return (flags & CBFlag_IsCapture);
    if (flags & CBFlag_IsCapture)
        return ((offset >> 15) & 0xC) | (flags & 0x3);
    return -1;
}

void GPU::GetCaptureInfo(int* info, u16** cbf, int len)
{
    for (int b = 0; b < len; b++)
    {
        u16* ptr = cbf[b];
        if (!ptr)
        {
            info[b] = -1;
            continue;
        }

        u16 flags = *ptr;
        if (flags & CBFlag_IsCapture)
            info[b] = flags & 0xF;
        else
            info[b] = -1;
    }
}

void GPU::GetCaptureInfo_ABG(int* info)
{
    return GetCaptureInfo(info, VRAMCBF_ABG, 32);
}

void GPU::GetCaptureInfo_AOBJ(int* info)
{
    return GetCaptureInfo(info, VRAMCBF_AOBJ, 16);
}

void GPU::GetCaptureInfo_BBG(int* info)
{
    return GetCaptureInfo(info, VRAMCBF_BBG, 8);
}

void GPU::GetCaptureInfo_BOBJ(int* info)
{
    return GetCaptureInfo(info, VRAMCBF_BOBJ, 8);
}

void GPU::GetCaptureInfo_Texture(int* info)
{
    for (int b = 0; b < 16; b++)
    {
        int bank = b >> 2;
        int subblock = b & 0x3;
        u32 mask = VRAMMap_Texture[bank];
        u16 cbf = 0;

        // check the bank mask
        // for now we don't bother with overlapping banks
        // this may change if a game happens to do this
        if (mask == (1<<0))
            cbf = VRAMCaptureBlockFlags[(0<<2) | subblock];
        else if (mask == (1<<1))
            cbf = VRAMCaptureBlockFlags[(1<<2) | subblock];
        else if (mask == (1<<2))
            cbf = VRAMCaptureBlockFlags[(2<<2) | subblock];
        else if (mask == (1<<3))
            cbf = VRAMCaptureBlockFlags[(3<<2) | subblock];

        if (cbf & CBFlag_IsCapture)
            info[b] = cbf & 0xF;
        else
            info[b] = -1;
    }
}

}
