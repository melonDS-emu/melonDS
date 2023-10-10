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

#include <string.h>
#include "NDS.h"
#include "GPU.h"

#ifdef JIT_ENABLED
#include "ARMJIT.h"
#endif

#include "GPU2D_Soft.h"

using Platform::Log;
using Platform::LogLevel;

namespace GPU
{

#define LINE_CYCLES  (355*6)
#define HBLANK_CYCLES (48+(256*6))
#define FRAME_CYCLES  (LINE_CYCLES * 263)

u16 VCount;
u32 NextVCount;
u16 TotalScanlines;

bool RunFIFO;

u16 DispStat[2], VMatch[2];

u8 Palette[2*1024];
u8 OAM[2*1024];

u8 VRAM_A[128*1024];
u8 VRAM_B[128*1024];
u8 VRAM_C[128*1024];
u8 VRAM_D[128*1024];
u8 VRAM_E[ 64*1024];
u8 VRAM_F[ 16*1024];
u8 VRAM_G[ 16*1024];
u8 VRAM_H[ 32*1024];
u8 VRAM_I[ 16*1024];
u8* const VRAM[9]     = {VRAM_A,  VRAM_B,  VRAM_C,  VRAM_D,  VRAM_E, VRAM_F, VRAM_G, VRAM_H, VRAM_I};
u32 const VRAMMask[9] = {0x1FFFF, 0x1FFFF, 0x1FFFF, 0x1FFFF, 0xFFFF, 0x3FFF, 0x3FFF, 0x7FFF, 0x3FFF};

u8 VRAMCNT[9];
u8 VRAMSTAT;

u32 VRAMMap_LCDC;

u32 VRAMMap_ABG[0x20];
u32 VRAMMap_AOBJ[0x10];
u32 VRAMMap_BBG[0x8];
u32 VRAMMap_BOBJ[0x8];

u32 VRAMMap_ABGExtPal[4];
u32 VRAMMap_AOBJExtPal;
u32 VRAMMap_BBGExtPal[4];
u32 VRAMMap_BOBJExtPal;

u32 VRAMMap_Texture[4];
u32 VRAMMap_TexPal[8];

u32 VRAMMap_ARM7[2];

u8* VRAMPtr_ABG[0x20];
u8* VRAMPtr_AOBJ[0x10];
u8* VRAMPtr_BBG[0x8];
u8* VRAMPtr_BOBJ[0x8];

int FrontBuffer;
u32* Framebuffer[2][2];
int Renderer = 0;

GPU2D::Unit GPU2D_A(0);
GPU2D::Unit GPU2D_B(1);

std::unique_ptr<GPU2D::Renderer2D> GPU2D_Renderer = {};

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

VRAMTrackingSet<512*1024, 16*1024> VRAMDirty_ABG;
VRAMTrackingSet<256*1024, 16*1024> VRAMDirty_AOBJ;
VRAMTrackingSet<128*1024, 16*1024> VRAMDirty_BBG;
VRAMTrackingSet<128*1024, 16*1024> VRAMDirty_BOBJ;

VRAMTrackingSet<32*1024, 8*1024> VRAMDirty_ABGExtPal;
VRAMTrackingSet<32*1024, 8*1024> VRAMDirty_BBGExtPal;
VRAMTrackingSet<8*1024, 8*1024> VRAMDirty_AOBJExtPal;
VRAMTrackingSet<8*1024, 8*1024> VRAMDirty_BOBJExtPal;

VRAMTrackingSet<512*1024, 128*1024> VRAMDirty_Texture;
VRAMTrackingSet<128*1024, 16*1024> VRAMDirty_TexPal;

NonStupidBitField<128*1024/VRAMDirtyGranularity> VRAMDirty[9];

u8 VRAMFlat_ABG[512*1024];
u8 VRAMFlat_BBG[128*1024];
u8 VRAMFlat_AOBJ[256*1024];
u8 VRAMFlat_BOBJ[128*1024];

u8 VRAMFlat_ABGExtPal[32*1024];
u8 VRAMFlat_BBGExtPal[32*1024];
u8 VRAMFlat_AOBJExtPal[8*1024];
u8 VRAMFlat_BOBJExtPal[8*1024];

u8 VRAMFlat_Texture[512*1024];
u8 VRAMFlat_TexPal[128*1024];

u32 OAMDirty;
u32 PaletteDirty;

#ifdef OGLRENDERER_ENABLED
std::unique_ptr<GLCompositor> CurGLCompositor = {};
#endif

bool Init()
{
    GPU2D_Renderer = std::make_unique<GPU2D::SoftRenderer>();
    if (!GPU3D::Init()) return false;

    FrontBuffer = 0;
    Framebuffer[0][0] = NULL; Framebuffer[0][1] = NULL;
    Framebuffer[1][0] = NULL; Framebuffer[1][1] = NULL;
    Renderer = 0;

    return true;
}

void DeInit()
{
    GPU2D_Renderer.reset();
    GPU3D::DeInit();

    if (Framebuffer[0][0]) delete[] Framebuffer[0][0];
    if (Framebuffer[0][1]) delete[] Framebuffer[0][1];
    if (Framebuffer[1][0]) delete[] Framebuffer[1][0];
    if (Framebuffer[1][1]) delete[] Framebuffer[1][1];

    Framebuffer[0][0] = nullptr;
    Framebuffer[0][1] = nullptr;
    Framebuffer[1][0] = nullptr;
    Framebuffer[1][1] = nullptr;

#ifdef OGLRENDERER_ENABLED
    CurGLCompositor = nullptr;
#endif
}

void ResetVRAMCache()
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

void Reset()
{
    VCount = 0;
    NextVCount = -1;
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

    size_t fbsize;
    if (GPU3D::CurrentRenderer->Accelerated)
        fbsize = (256*3 + 1) * 192;
    else
        fbsize = 256 * 192;

    for (size_t i = 0; i < fbsize; i++)
    {
        Framebuffer[0][0][i] = 0xFFFFFFFF;
        Framebuffer[1][0][i] = 0xFFFFFFFF;
    }
    for (size_t i = 0; i < fbsize; i++)
    {
        Framebuffer[0][1][i] = 0xFFFFFFFF;
        Framebuffer[1][1][i] = 0xFFFFFFFF;
    }

    GPU2D_A.Reset();
    GPU2D_B.Reset();
    GPU3D::Reset();

    int backbuf = FrontBuffer ? 0 : 1;
    GPU2D_Renderer->SetFramebuffer(Framebuffer[backbuf][1], Framebuffer[backbuf][0]);

    ResetRenderer();

    ResetVRAMCache();

    OAMDirty = 0x3;
    PaletteDirty = 0xF;
}

void Stop()
{
    int fbsize;
    if (GPU3D::CurrentRenderer->Accelerated)
        fbsize = (256*3 + 1) * 192;
    else
        fbsize = 256 * 192;

    memset(Framebuffer[0][0], 0, fbsize*4);
    memset(Framebuffer[0][1], 0, fbsize*4);
    memset(Framebuffer[1][0], 0, fbsize*4);
    memset(Framebuffer[1][1], 0, fbsize*4);

#ifdef OGLRENDERER_ENABLED
    // This needs a better way to know that we're
    // using the OpenGL renderer specifically
    if (GPU3D::CurrentRenderer->Accelerated)
        CurGLCompositor->Stop();
#endif
}

void DoSavestate(Savestate* file)
{
    file->Section("GPUG");

    file->Var16(&VCount);
    file->Var32(&NextVCount);
    file->Var16(&TotalScanlines);

    file->Var16(&DispStat[0]);
    file->Var16(&DispStat[1]);
    file->Var16(&VMatch[0]);
    file->Var16(&VMatch[1]);

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
            VRAMPtr_ABG[i] = GetUniqueBankPtr(VRAMMap_ABG[i], i << 14);
        for (int i = 0; i < 0x10; i++)
            VRAMPtr_AOBJ[i] = GetUniqueBankPtr(VRAMMap_AOBJ[i], i << 14);
        for (int i = 0; i < 0x8; i++)
            VRAMPtr_BBG[i] = GetUniqueBankPtr(VRAMMap_BBG[i], i << 14);
        for (int i = 0; i < 0x8; i++)
            VRAMPtr_BOBJ[i] = GetUniqueBankPtr(VRAMMap_BOBJ[i], i << 14);
    }

    GPU2D_A.DoSavestate(file);
    GPU2D_B.DoSavestate(file);
    GPU3D::DoSavestate(file);

    ResetVRAMCache();
}

void AssignFramebuffers()
{
    int backbuf = FrontBuffer ? 0 : 1;
    if (NDS::PowerControl9 & (1<<15))
    {
        GPU2D_Renderer->SetFramebuffer(Framebuffer[backbuf][0], Framebuffer[backbuf][1]);
    }
    else
    {
        GPU2D_Renderer->SetFramebuffer(Framebuffer[backbuf][1], Framebuffer[backbuf][0]);
    }
}

void InitRenderer(int renderer)
{
#ifdef OGLRENDERER_ENABLED
    if (renderer == 1)
    {
        CurGLCompositor = GLCompositor::New();
        // Create opengl renderer
        if (!CurGLCompositor)
        {
            // Fallback on software renderer
            renderer = 0;
            GPU3D::CurrentRenderer = std::make_unique<GPU3D::SoftRenderer>();
        }
        GPU3D::CurrentRenderer = GPU3D::GLRenderer::New();
        if (!GPU3D::CurrentRenderer)
        {
            // Fallback on software renderer
            CurGLCompositor.reset();
            renderer = 0;
            GPU3D::CurrentRenderer = std::make_unique<GPU3D::SoftRenderer>();
        }
    }
    else
#endif
    {
        GPU3D::CurrentRenderer = std::make_unique<GPU3D::SoftRenderer>();
    }

    Renderer = renderer;
}

void DeInitRenderer()
{
    // Delete the 3D renderer, if it exists
    GPU3D::CurrentRenderer.reset();

#ifdef OGLRENDERER_ENABLED
    // Delete the compositor, if one exists
    CurGLCompositor.reset();
#endif
}

void ResetRenderer()
{
    if (Renderer == 0)
    {
        GPU3D::CurrentRenderer->Reset();
    }
#ifdef OGLRENDERER_ENABLED
    else
    {
        CurGLCompositor->Reset();
        GPU3D::CurrentRenderer->Reset();
    }
#endif
}

void SetRenderSettings(int renderer, RenderSettings& settings)
{
    if (renderer != Renderer)
    {
        DeInitRenderer();
        InitRenderer(renderer);
    }

    int fbsize;
    if (GPU3D::CurrentRenderer->Accelerated)
        fbsize = (256*3 + 1) * 192;
    else
        fbsize = 256 * 192;

    if (Framebuffer[0][0]) { delete[] Framebuffer[0][0]; Framebuffer[0][0] = nullptr; }
    if (Framebuffer[1][0]) { delete[] Framebuffer[1][0]; Framebuffer[1][0] = nullptr; }
    if (Framebuffer[0][1]) { delete[] Framebuffer[0][1]; Framebuffer[0][1] = nullptr; }
    if (Framebuffer[1][1]) { delete[] Framebuffer[1][1]; Framebuffer[1][1] = nullptr; }

    Framebuffer[0][0] = new u32[fbsize];
    Framebuffer[1][0] = new u32[fbsize];
    Framebuffer[0][1] = new u32[fbsize];
    Framebuffer[1][1] = new u32[fbsize];

    memset(Framebuffer[0][0], 0, fbsize*4);
    memset(Framebuffer[1][0], 0, fbsize*4);
    memset(Framebuffer[0][1], 0, fbsize*4);
    memset(Framebuffer[1][1], 0, fbsize*4);

    AssignFramebuffers();

    if (Renderer == 0)
    {
        GPU3D::CurrentRenderer->SetRenderSettings(settings);
    }
#ifdef OGLRENDERER_ENABLED
    else
    {
        CurGLCompositor->SetRenderSettings(settings);
        GPU3D::CurrentRenderer->SetRenderSettings(settings);
    }
#endif
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

u8* GetUniqueBankPtr(u32 mask, u32 offset)
{
    if (!mask || (mask & (mask - 1)) != 0) return NULL;
    int num = __builtin_ctz(mask);
    return &VRAM[num][offset & VRAMMask[num]];
}

#define MAP_RANGE(map, base, n)    for (int i = 0; i < n; i++) VRAMMap_##map[(base)+i] |= bankmask;
#define UNMAP_RANGE(map, base, n)  for (int i = 0; i < n; i++) VRAMMap_##map[(base)+i] &= ~bankmask;

#define MAP_RANGE_PTR(map, base, n) \
    for (int i = 0; i < n; i++) { VRAMMap_##map[(base)+i] |= bankmask; VRAMPtr_##map[(base)+i] = GetUniqueBankPtr(VRAMMap_##map[(base)+i], ((base)+i)<<14); }
#define UNMAP_RANGE_PTR(map, base, n) \
    for (int i = 0; i < n; i++) { VRAMMap_##map[(base)+i] &= ~bankmask; VRAMPtr_##map[(base)+i] = GetUniqueBankPtr(VRAMMap_##map[(base)+i], ((base)+i)<<14); }

void MapVRAM_AB(u32 bank, u8 cnt)
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
            break;

        case 2: // AOBJ
            oldofs &= 0x1;
            UNMAP_RANGE_PTR(AOBJ, oldofs<<3, 8);
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
            break;

        case 2: // AOBJ
            ofs &= 0x1;
            MAP_RANGE_PTR(AOBJ, ofs<<3, 8);
            break;

        case 3: // texture
            VRAMMap_Texture[ofs] |= bankmask;
            break;
        }
    }
}

void MapVRAM_CD(u32 bank, u8 cnt)
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
            }
            else
            {
                UNMAP_RANGE_PTR(BOBJ, 0, 8);
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
            break;

        case 2: // ARM7 VRAM
            ofs &= 0x1;
            VRAMMap_ARM7[ofs] |= bankmask;
            memset(VRAMDirty[bank].Data, 0xFF, sizeof(VRAMDirty[bank].Data));
            VRAMSTAT |= (1 << (bank-2));
#ifdef JIT_ENABLED
            ARMJIT::CheckAndInvalidateWVRAM(ofs);
#endif
            break;

        case 3: // texture
            VRAMMap_Texture[ofs] |= bankmask;
            break;

        case 4: // BBG/BOBJ
            if (bank == 2)
            {
                MAP_RANGE_PTR(BBG, 0, 8);
            }
            else
            {
                MAP_RANGE_PTR(BOBJ, 0, 8);
            }
            break;
        }
    }
}

void MapVRAM_E(u32 bank, u8 cnt)
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
            break;

        case 2: // AOBJ
            UNMAP_RANGE_PTR(AOBJ, 0, 4);
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
            break;

        case 2: // AOBJ
            MAP_RANGE_PTR(AOBJ, 0, 4);
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

void MapVRAM_FG(u32 bank, u8 cnt)
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
            }
            break;

        case 2: // AOBJ
            {
                u32 base = (oldofs & 0x1) + ((oldofs & 0x2) << 1);
                VRAMMap_AOBJ[base] &= ~bankmask;
                VRAMMap_AOBJ[base + 2] &= ~bankmask;
                VRAMPtr_AOBJ[base] = GetUniqueBankPtr(VRAMMap_AOBJ[base], base << 14);
                VRAMPtr_AOBJ[base + 2] = GetUniqueBankPtr(VRAMMap_AOBJ[base + 2], (base + 2) << 14);
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
            }
            break;

        case 2: // AOBJ
            {
                u32 base = (ofs & 0x1) + ((ofs & 0x2) << 1);
                VRAMMap_AOBJ[base] |= bankmask;
                VRAMMap_AOBJ[base + 2] |= bankmask;
                VRAMPtr_AOBJ[base] = GetUniqueBankPtr(VRAMMap_AOBJ[base], base << 14);
                VRAMPtr_AOBJ[base + 2] = GetUniqueBankPtr(VRAMMap_AOBJ[base + 2], (base + 2) << 14);
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

void MapVRAM_H(u32 bank, u8 cnt)
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
            VRAMMap_BBG[0] &= ~bankmask;
            VRAMMap_BBG[1] &= ~bankmask;
            VRAMMap_BBG[4] &= ~bankmask;
            VRAMMap_BBG[5] &= ~bankmask;
            VRAMPtr_BBG[0] = GetUniqueBankPtr(VRAMMap_BBG[0], 0 << 14);
            VRAMPtr_BBG[1] = GetUniqueBankPtr(VRAMMap_BBG[1], 1 << 14);
            VRAMPtr_BBG[4] = GetUniqueBankPtr(VRAMMap_BBG[4], 4 << 14);
            VRAMPtr_BBG[5] = GetUniqueBankPtr(VRAMMap_BBG[5], 5 << 14);
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
            VRAMMap_BBG[0] |= bankmask;
            VRAMMap_BBG[1] |= bankmask;
            VRAMMap_BBG[4] |= bankmask;
            VRAMMap_BBG[5] |= bankmask;
            VRAMPtr_BBG[0] = GetUniqueBankPtr(VRAMMap_BBG[0], 0 << 14);
            VRAMPtr_BBG[1] = GetUniqueBankPtr(VRAMMap_BBG[1], 1 << 14);
            VRAMPtr_BBG[4] = GetUniqueBankPtr(VRAMMap_BBG[4], 4 << 14);
            VRAMPtr_BBG[5] = GetUniqueBankPtr(VRAMMap_BBG[5], 5 << 14);
            break;

        case 2: // BBG ext palette
            MAP_RANGE(BBGExtPal, 0, 4);
            break;
        }
    }
}

void MapVRAM_I(u32 bank, u8 cnt)
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
            VRAMMap_BBG[2] &= ~bankmask;
            VRAMMap_BBG[3] &= ~bankmask;
            VRAMMap_BBG[6] &= ~bankmask;
            VRAMMap_BBG[7] &= ~bankmask;
            VRAMPtr_BBG[2] = GetUniqueBankPtr(VRAMMap_BBG[2], 2 << 14);
            VRAMPtr_BBG[3] = GetUniqueBankPtr(VRAMMap_BBG[3], 3 << 14);
            VRAMPtr_BBG[6] = GetUniqueBankPtr(VRAMMap_BBG[6], 6 << 14);
            VRAMPtr_BBG[7] = GetUniqueBankPtr(VRAMMap_BBG[7], 7 << 14);
            break;

        case 2: // BOBJ
            UNMAP_RANGE_PTR(BOBJ, 0, 8);
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
            VRAMMap_BBG[2] |= bankmask;
            VRAMMap_BBG[3] |= bankmask;
            VRAMMap_BBG[6] |= bankmask;
            VRAMMap_BBG[7] |= bankmask;
            VRAMPtr_BBG[2] = GetUniqueBankPtr(VRAMMap_BBG[2], 2 << 14);
            VRAMPtr_BBG[3] = GetUniqueBankPtr(VRAMMap_BBG[3], 3 << 14);
            VRAMPtr_BBG[6] = GetUniqueBankPtr(VRAMMap_BBG[6], 6 << 14);
            VRAMPtr_BBG[7] = GetUniqueBankPtr(VRAMMap_BBG[7], 7 << 14);
            break;

        case 2: // BOBJ
            MAP_RANGE_PTR(BOBJ, 0, 8);
            break;

        case 3: // BOBJ ext palette
            VRAMMap_BOBJExtPal |= bankmask;
            break;
        }
    }
}


void SetPowerCnt(u32 val)
{
    // POWCNT1 effects:
    // * bit0: asplodes hardware??? not tested.
    // * bit1: disables engine A palette and OAM (zero-filled) (TODO: affects mem timings???)
    // * bit2: disables rendering engine, resets its internal state (polygons and registers)
    // * bit3: disables geometry engine
    // * bit9: disables engine B palette, OAM and rendering (screen turns white)
    // * bit15: screen swap

    if (!(val & (1<<0))) Log(LogLevel::Warn, "!!! CLEARING POWCNT BIT0. DANGER\n");

    GPU2D_A.SetEnabled(val & (1<<1));
    GPU2D_B.SetEnabled(val & (1<<9));
    GPU3D::SetEnabled(val & (1<<3), val & (1<<2));

    AssignFramebuffers();
}


void DisplayFIFO(u32 x)
{
    // sample the FIFO
    // as this starts 16 cycles (~3 pixels) before display start,
    // we aren't aligned to the 8-pixel grid
    if (x > 0)
    {
        if (x == 8)
            GPU2D_A.SampleFIFO(0, 5);
        else
            GPU2D_A.SampleFIFO(x-11, 8);
    }

    if (x < 256)
    {
        // transfer the next 8 pixels
        NDS::CheckDMAs(0, 0x04);
        NDS::ScheduleEvent(NDS::Event_DisplayFIFO, true, 6*8, DisplayFIFO, x+8);
    }
    else
        GPU2D_A.SampleFIFO(253, 3); // sample the remaining pixels
}

void StartFrame()
{
    // only run the display FIFO if needed:
    // * if it is used for display or capture
    // * if we have display FIFO DMA
    RunFIFO = GPU2D_A.UsesFIFO() || NDS::DMAsInMode(0, 0x04);

    TotalScanlines = 0;
    StartScanline(0);
}

void StartHBlank(u32 line)
{
    DispStat[0] |= (1<<1);
    DispStat[1] |= (1<<1);

    if (VCount < 192)
    {
        // draw
        // note: this should start 48 cycles after the scanline start
        if (line < 192)
        {
            GPU2D_Renderer->DrawScanline(line, &GPU2D_A);
            GPU2D_Renderer->DrawScanline(line, &GPU2D_B);
        }

        // sprites are pre-rendered one scanline in advance
        if (line < 191)
        {
            GPU2D_Renderer->DrawSprites(line+1, &GPU2D_A);
            GPU2D_Renderer->DrawSprites(line+1, &GPU2D_B);
        }

        NDS::CheckDMAs(0, 0x02);
    }
    else if (VCount == 215)
    {
        GPU3D::VCount215();
    }
    else if (VCount == 262)
    {
        GPU2D_Renderer->DrawSprites(0, &GPU2D_A);
        GPU2D_Renderer->DrawSprites(0, &GPU2D_B);
    }

    if (DispStat[0] & (1<<4)) NDS::SetIRQ(0, NDS::IRQ_HBlank);
    if (DispStat[1] & (1<<4)) NDS::SetIRQ(1, NDS::IRQ_HBlank);

    if (VCount < 262)
        NDS::ScheduleEvent(NDS::Event_LCD, true, (LINE_CYCLES - HBLANK_CYCLES), StartScanline, line+1);
    else
        NDS::ScheduleEvent(NDS::Event_LCD, true, (LINE_CYCLES - HBLANK_CYCLES), FinishFrame, line+1);
}

void FinishFrame(u32 lines)
{
    FrontBuffer = FrontBuffer ? 0 : 1;
    AssignFramebuffers();

    TotalScanlines = lines;

    if (GPU3D::AbortFrame)
    {
        GPU3D::RestartFrame();
        GPU3D::AbortFrame = false;
    }
}

void StartScanline(u32 line)
{
    if (line == 0)
        VCount = 0;
    else if (NextVCount != 0xFFFFFFFF)
        VCount = NextVCount;
    else
        VCount++;

    NextVCount = -1;

    DispStat[0] &= ~(1<<1);
    DispStat[1] &= ~(1<<1);

    if (VCount == VMatch[0])
    {
        DispStat[0] |= (1<<2);

        if (DispStat[0] & (1<<5)) NDS::SetIRQ(0, NDS::IRQ_VCount);
    }
    else
        DispStat[0] &= ~(1<<2);

    if (VCount == VMatch[1])
    {
        DispStat[1] |= (1<<2);

        if (DispStat[1] & (1<<5)) NDS::SetIRQ(1, NDS::IRQ_VCount);
    }
    else
        DispStat[1] &= ~(1<<2);

    GPU2D_A.CheckWindows(VCount);
    GPU2D_B.CheckWindows(VCount);

    if (VCount >= 2 && VCount < 194)
        NDS::CheckDMAs(0, 0x03);
    else if (VCount == 194)
        NDS::StopDMAs(0, 0x03);

    if (line < 192)
    {
        if (line == 0)
        {
            GPU2D_Renderer->VBlankEnd(&GPU2D_A, &GPU2D_B);
            GPU2D_A.VBlankEnd();
            GPU2D_B.VBlankEnd();
        }

        if (RunFIFO)
            NDS::ScheduleEvent(NDS::Event_DisplayFIFO, false, 32, DisplayFIFO, 0);
    }

    if (VCount == 262)
    {
        // frame end

        DispStat[0] &= ~(1<<0);
        DispStat[1] &= ~(1<<0);
    }
    else
    {
        if (VCount == 192)
        {
            // in reality rendering already finishes at line 144
            // and games might already start to modify texture memory.
            // That doesn't matter for us because we cache the entire
            // texture memory anyway and only update it before the start
            //of the next frame.
            // So we can give the rasteriser a bit more headroom
            GPU3D::VCount144();

            // VBlank
            DispStat[0] |= (1<<0);
            DispStat[1] |= (1<<0);

            NDS::StopDMAs(0, 0x04);

            NDS::CheckDMAs(0, 0x01);
            NDS::CheckDMAs(1, 0x11);

            if (DispStat[0] & (1<<3)) NDS::SetIRQ(0, NDS::IRQ_VBlank);
            if (DispStat[1] & (1<<3)) NDS::SetIRQ(1, NDS::IRQ_VBlank);

            GPU2D_A.VBlank();
            GPU2D_B.VBlank();
            GPU3D::VBlank();

#ifdef OGLRENDERER_ENABLED
            // Need a better way to identify the openGL renderer in particular
            if (GPU3D::CurrentRenderer->Accelerated)
                CurGLCompositor->RenderFrame();
#endif
        }
    }

    NDS::ScheduleEvent(NDS::Event_LCD, true, HBLANK_CYCLES, StartHBlank, line);
}


void SetDispStat(u32 cpu, u16 val)
{
    val &= 0xFFB8;
    DispStat[cpu] &= 0x0047;
    DispStat[cpu] |= val;

    VMatch[cpu] = (val >> 8) | ((val & 0x80) << 1);
}

void SetVCount(u16 val)
{
    // VCount write is delayed until the next scanline

    // TODO: how does the 3D engine react to VCount writes while it's rendering?
    // 3D engine seems to give up on the current frame in that situation, repeating the last two scanlines
    // TODO: also check the various DMA types that can be involved

    GPU3D::AbortFrame |= NextVCount != val;
    NextVCount = val;
}

template <u32 Size, u32 MappingGranularity>
NonStupidBitField<Size/VRAMDirtyGranularity> VRAMTrackingSet<Size, MappingGranularity>::DeriveState(u32* currentMappings)
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
                    u32 dirty = ((u32*)VRAMDirty[num].Data)[i & (VRAMMask[num] >> 14)];
                    result.Data[i / 2] |= (u64)dirty << ((i&1)*32);
                }
                else if (MappingGranularity == 8*1024)
                {
                    u16 dirty = ((u16*)VRAMDirty[num].Data)[i & (VRAMMask[num] >> 13)];
                    result.Data[i / 4] |= (u64)dirty << ((i&3)*16);
                }
                else if (MappingGranularity == 128*1024)
                {
                    result.Data[i * 4 + 0] |= VRAMDirty[num].Data[0];
                    result.Data[i * 4 + 1] |= VRAMDirty[num].Data[1];
                    result.Data[i * 4 + 2] |= VRAMDirty[num].Data[2];
                    result.Data[i * 4 + 3] |= VRAMDirty[num].Data[3];
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
        VRAMDirty[num].Clear();
    }

    return result;
}

template NonStupidBitField<32*1024/VRAMDirtyGranularity> VRAMTrackingSet<32*1024, 8*1024>::DeriveState(u32*);
template NonStupidBitField<8*1024/VRAMDirtyGranularity> VRAMTrackingSet<8*1024, 8*1024>::DeriveState(u32*);
template NonStupidBitField<512*1024/VRAMDirtyGranularity> VRAMTrackingSet<512*1024, 128*1024>::DeriveState(u32*);
template NonStupidBitField<128*1024/VRAMDirtyGranularity> VRAMTrackingSet<128*1024, 16*1024>::DeriveState(u32*);
template NonStupidBitField<256*1024/VRAMDirtyGranularity> VRAMTrackingSet<256*1024, 16*1024>::DeriveState(u32*);
template NonStupidBitField<512*1024/VRAMDirtyGranularity> VRAMTrackingSet<512*1024, 16*1024>::DeriveState(u32*);

template <u32 MappingGranularity, u32 Size>
inline bool CopyLinearVRAM(u8* flat, u32* mappings, NonStupidBitField<Size>& dirty, u64 (*slowAccess)(u32 addr))
{
    const u32 VRAMBitsPerMapping = MappingGranularity / VRAMDirtyGranularity;

    bool change = false;

    typename NonStupidBitField<Size>::Iterator it = dirty.Begin();
    while (it != dirty.End())
    {
        u32 offset = *it * VRAMDirtyGranularity;
        u8* dst = flat + offset;
        u8* fastAccess = GetUniqueBankPtr(mappings[*it / VRAMBitsPerMapping], offset);
        if (fastAccess)
        {
            memcpy(dst, fastAccess, VRAMDirtyGranularity);
        }
        else
        {
            for (u32 i = 0; i < VRAMDirtyGranularity; i += 8)
                *(u64*)&dst[i] = slowAccess(offset + i);
        }
        change = true;
        it++;
    }
    return change;
}

bool MakeVRAMFlat_TextureCoherent(NonStupidBitField<512*1024/VRAMDirtyGranularity>& dirty)
{
    return CopyLinearVRAM<128*1024>(VRAMFlat_Texture, VRAMMap_Texture, dirty, ReadVRAM_Texture<u64>);
}
bool MakeVRAMFlat_TexPalCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty)
{
    return CopyLinearVRAM<16*1024>(VRAMFlat_TexPal, VRAMMap_TexPal, dirty, ReadVRAM_TexPal<u64>);
}

bool MakeVRAMFlat_ABGCoherent(NonStupidBitField<512*1024/VRAMDirtyGranularity>& dirty)
{
    return CopyLinearVRAM<16*1024>(VRAMFlat_ABG, VRAMMap_ABG, dirty, ReadVRAM_ABG<u64>);
}
bool MakeVRAMFlat_BBGCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty)
{
    return CopyLinearVRAM<16*1024>(VRAMFlat_BBG, VRAMMap_BBG, dirty, ReadVRAM_BBG<u64>);
}

bool MakeVRAMFlat_AOBJCoherent(NonStupidBitField<256*1024/VRAMDirtyGranularity>& dirty)
{
    return CopyLinearVRAM<16*1024>(VRAMFlat_AOBJ, VRAMMap_AOBJ, dirty, ReadVRAM_AOBJ<u64>);
}
bool MakeVRAMFlat_BOBJCoherent(NonStupidBitField<128*1024/VRAMDirtyGranularity>& dirty)
{
    return CopyLinearVRAM<16*1024>(VRAMFlat_BOBJ, VRAMMap_BOBJ, dirty, ReadVRAM_BOBJ<u64>);
}

template<typename T>
T ReadVRAM_ABGExtPal(u32 addr)
{
    u32 mask = VRAMMap_ABGExtPal[(addr >> 13) & 0x3];

    T ret = 0;
    if (mask & (1<<4)) ret |= *(T*)&VRAM_E[addr & 0x7FFF];
    if (mask & (1<<5)) ret |= *(T*)&VRAM_F[addr & 0x3FFF];
    if (mask & (1<<6)) ret |= *(T*)&VRAM_G[addr & 0x3FFF];

    return ret;
}

template<typename T>
T ReadVRAM_BBGExtPal(u32 addr)
{
    u32 mask = VRAMMap_BBGExtPal[(addr >> 13) & 0x3];

    T ret = 0;
    if (mask & (1<<7)) ret |= *(T*)&VRAM_H[addr & 0x7FFF];

    return ret;
}

template<typename T>
T ReadVRAM_AOBJExtPal(u32 addr)
{
    u32 mask = VRAMMap_AOBJExtPal;

    T ret = 0;
    if (mask & (1<<4)) ret |= *(T*)&VRAM_F[addr & 0x1FFF];
    if (mask & (1<<5)) ret |= *(T*)&VRAM_G[addr & 0x1FFF];

    return ret;
}

template<typename T>
T ReadVRAM_BOBJExtPal(u32 addr)
{
    u32 mask = VRAMMap_BOBJExtPal;

    T ret = 0;
    if (mask & (1<<8)) ret |= *(T*)&VRAM_I[addr & 0x1FFF];

    return ret;
}

bool MakeVRAMFlat_ABGExtPalCoherent(NonStupidBitField<32*1024/VRAMDirtyGranularity>& dirty)
{
    return CopyLinearVRAM<8*1024>(VRAMFlat_ABGExtPal, VRAMMap_ABGExtPal, dirty, ReadVRAM_ABGExtPal<u64>);
}
bool MakeVRAMFlat_BBGExtPalCoherent(NonStupidBitField<32*1024/VRAMDirtyGranularity>& dirty)
{
    return CopyLinearVRAM<8*1024>(VRAMFlat_BBGExtPal, VRAMMap_BBGExtPal, dirty, ReadVRAM_BBGExtPal<u64>);
}

bool MakeVRAMFlat_AOBJExtPalCoherent(NonStupidBitField<8*1024/VRAMDirtyGranularity>& dirty)
{
    return CopyLinearVRAM<8*1024>(VRAMFlat_AOBJExtPal, &VRAMMap_AOBJExtPal, dirty, ReadVRAM_AOBJExtPal<u64>);
}
bool MakeVRAMFlat_BOBJExtPalCoherent(NonStupidBitField<8*1024/VRAMDirtyGranularity>& dirty)
{
    return CopyLinearVRAM<8*1024>(VRAMFlat_BOBJExtPal, &VRAMMap_BOBJExtPal, dirty, ReadVRAM_BOBJExtPal<u64>);
}

}
