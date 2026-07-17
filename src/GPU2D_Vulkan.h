/*
    Copyright 2016-2026 melonDS team

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

#pragma once

#include <array>
#include <map>
#include "VulkanSupport.h"
#include "GPU2D.h"

namespace melonDS
{
class VulkanRenderer;

// Vulkan port of GLRenderer2D (src/GPU2D_OpenGL.{h,cpp}).
//
// The CPU-side logic (register diffing, dirty VRAM tracking, banded
// deferred rendering keyed on LastLine/LastSpriteLine) is copied
// near-verbatim from the GL renderer; only the GL calls are replaced.
//
// Recording model: the parent provides a command buffer per band via
// SetCommandBuffer(); everything DrawScanline/DrawSprites/VBlank does is
// recorded into it. Mid-frame texture uploads become staging-buffer copies
// recorded before the draws that sample them, with TRANSFER->FRAGMENT
// barriers; passes that consume prior outputs are separated by
// COLOR_ATTACHMENT_OUTPUT->FRAGMENT_SHADER image transitions.
//
// Frame contract (mirrors the synchronous model of the 3D Vulkan
// renderer): the parent must have waited for the previous frame's command
// buffers before recording of a new frame begins; per-frame transient
// allocations (vertex/staging rings) are reset in VBlank().
class VulkanRenderer2D : public Renderer2D
{
public:
    VulkanRenderer2D(melonDS::GPU2D& gpu2D, VK::Context& ctx);
    ~VulkanRenderer2D() override;

    // InitShaders() (or InitShaders(other)) must be called before Init():
    // the framebuffers created in Init() need the shared render passes
    bool Init() override;
    void Reset() override;

    bool InitShaders();
    bool InitShaders(VulkanRenderer2D& other);
    // destroys the resources shared between the two 2D units
    // (pipelines, layouts, render passes, samplers, mosaic LUT);
    // call it once, on the instance that owns them (InitShaders())
    void DeleteShaders();

    void PostSavestate();

    // call only while this renderer's command buffers are not executing
    // (device idle); recreates the scale-dependent images
    void SetScaleFactor(int scale);

    // resources owned by the (future) parent VulkanRenderer.
    // all views must be in SHADER_READ_ONLY_OPTIMAL whenever this
    // renderer's command buffers execute. unset views fall back to a
    // dummy texture so the descriptor sets stay valid.
    struct SharedResources
    {
        // 3D renderer color output (2D view, RGBA8, ScreenW x ScreenH);
        // sampled in place of BG0 when DISPCNT bit3 is set
        VkImageView OutputTex3D = VK_NULL_HANDLE;
        // display capture blocks (2D array views, RGBA8):
        // 128x128*scale x 16 layers / 256x256*scale x 4 layers
        VkImageView Capture128 = VK_NULL_HANDLE;
        VkImageView Capture256 = VK_NULL_HANDLE;
    };
    // call only while the device is idle for this renderer
    // (rebuilds the descriptor sets)
    void SetSharedResources(const SharedResources& shared);

    // command buffer that subsequent DrawScanline/DrawSprites/VBlank
    // work is recorded into; provided per band by the parent
    void SetCommandBuffer(VkCommandBuffer cmd) { CurCmd = cmd; }

    // mirror of GLRenderer::NeedPartialRender (forces compositing of the
    // current band even when no 2D state changed)
    void SetNeedPartialRender(bool need) { NeedPartialRender = need; }

    // composited output (RGBA8, ScreenW x ScreenH), left in
    // SHADER_READ_ONLY_OPTIMAL after each band; this is what the parent's
    // final pass and capture read (GL: Parent.OutputTex2D[GPU2D.Num])
    const VK::Context::Image& GetOutput() const { return OutputImg; }

    void DrawScanline(u32 line) override;
    void DrawSprites(u32 line) override;
    void VBlank() override;
    void VBlankEnd() override;

private:
    friend class VulkanRenderer;
    VK::Context& Ctx;

    int ScaleFactor;
    int ScreenW, ScreenH;

    VkCommandBuffer CurCmd = VK_NULL_HANDLE;
    SharedResources Shared;
    bool NeedPartialRender = false;

    // push constant block shared by all 2D pipelines
    // (see the binding map in GPU2D_Vulkan_shaders.h)
    struct sPush2D
    {
        s32 uCurBG;
        s32 uRenderTransparent;
        s32 uScaleFactor;
    };

    // linear per-frame allocator (vertex data, staging uploads)
    struct sRingBuffer
    {
        VK::Context::Buffer Buf;
        u32 Offset = 0;
    };

    // ring of config versions, bound through a dynamic UBO offset so each
    // recorded draw keeps the config as it was at record time (the GL
    // renderer gets this for free from glBufferSubData renaming)
    struct sConfigRing
    {
        VK::Context::Buffer Buf;
        u32 Stride = 0;
        u32 Slots = 0;
        u32 Next = 0;
        u32 CurOffset = 0;
    };

    // resources shared between the two 2D units, created by InitShaders()
    // and borrowed by InitShaders(other) (mirrors the shared GL programs)
    bool OwnsShared = false;
    VkDescriptorSetLayout SetLayout = VK_NULL_HANDLE;
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    VkRenderPass RPColorClear = VK_NULL_HANDLE;     // 1x RGBA8, cleared (BG layer prerender)
    VkRenderPass RPColorLoad = VK_NULL_HANDLE;      // 1x RGBA8, loaded (sprite atlas, compositor)
    VkRenderPass RPObjLoad = VK_NULL_HANDLE;        // 2x RGBA8 + D16, loaded (OBJ layer)
    VkPipeline LayerPrePipeline = VK_NULL_HANDLE;
    VkPipeline SpritePrePipeline = VK_NULL_HANDLE;
    // the per-pass glColorMaski/glDepthFunc juggling of DoRenderSprites
    // becomes three prebuilt pipeline permutations
    VkPipeline SpriteTransPipeline = VK_NULL_HANDLE;    // transparent-flag pass: flags G+A
    VkPipeline SpriteWindowPipeline = VK_NULL_HANDLE;   // OBJ window pass: flags B
    VkPipeline SpriteOpaquePipeline = VK_NULL_HANDLE;   // opaque pass: color RGBA, flags R+G+A, depth LESS
    VkPipeline CompositorPipeline = VK_NULL_HANDLE;
    VkSampler SamplerNearestClamp = VK_NULL_HANDLE;
    VkSampler SamplerNearestRepeat = VK_NULL_HANDLE;
    VkSampler SamplerNearestBorder = VK_NULL_HANDLE;
    VK::Context::Image MosaicImg;                   // R8_SINT 256x16, shared A/B

    // per-instance resources

    u16* SpritePreVtxData = nullptr;
    u16* SpriteVtxData = nullptr;

    sRingBuffer VertexRing;                         // sprite vertex data, one slice per draw
    sRingBuffer StagingRing;                        // VRAM/palette upload staging
    VK::Context::Buffer RectVtxBuffer;              // fullscreen unit rect, 6x vec2

    sConfigRing LayerConfigRing;                    // binding 0, dynamic offset
    sConfigRing CompositorConfigRing;               // binding 2, dynamic offset
    sConfigRing SpriteConfigRing;                   // binding 3, dynamic offset
    VK::Context::Buffer ScanlineConfigUBO;          // binding 1, band-disjoint direct writes
    VK::Context::Buffer SpriteScanlineConfigUBO;    // binding 4, band-disjoint direct writes

    VK::Context::Image VRAMTexBG;                   // R8_UINT 1024 x bgheight
    VK::Context::Image VRAMTexOBJ;                  // R8_UINT 1024 x objheight
    VK::Context::Image PalTexBG;                    // A1R5G5B5 256 x 65 (see CreatePalView)
    VK::Context::Image PalTexOBJ;                   // A1R5G5B5 256 x 17
    VkImageView PalViewBG = VK_NULL_HANDLE;         // r<->b swizzled views of the above
    VkImageView PalViewOBJ = VK_NULL_HANDLE;

    // base index for a BG layer within the BG texture pool
    // based on BG type and size
    const u8 BGBaseIndex[4][4] = {
        {2, 10, 6, 14},     // text mode
        {0, 4, 16, 20},     // rotscale
        {0, 4, 12, 16},     // bitmap
        {18, 19, 12, 16},   // large bitmap
    };

    VK::Context::Image AllBGLayerImg[22];           // RGBA8, one per possible BG size
    VkFramebuffer AllBGLayerFB[22] = {};

    VK::Context::Image* BGLayerImg[4] = {};         // aliases into the pool above
    VkFramebuffer BGLayerFB[4] = {};

    VK::Context::Image SpriteImg;                   // RGBA8 1024x512 atlas (16x8 cells of 64x64)
    VkFramebuffer SpriteFB = VK_NULL_HANDLE;

    VK::Context::Image OBJLayerImg;                 // RGBA8 array, 2 layers (color + flags)
    VkImageView OBJLayerView[2] = {};               // per-layer attachment views
    VK::Context::Image OBJDepthImg;                 // D16
    VkFramebuffer OBJLayerFB = VK_NULL_HANDLE;

    VK::Context::Image OutputImg;                   // RGBA8 ScreenW x ScreenH
    VkFramebuffer OutputFB = VK_NULL_HANDLE;

    VK::Context::Image DummyTex;                    // 1x1 RGBA8 stand-ins for unset
    VK::Context::Image DummyTexArray;               // shared resources / unassigned layers

    // descriptor sets, one per combination of variable bindings
    // (VRAM/palette pair, BG layer aliases + wrap modes, 3D layer swap),
    // cached for the lifetime of the current scale factor / shared views
    VkDescriptorPool DescPool = VK_NULL_HANDLE;
    std::map<std::array<uintptr_t, 10>, VkDescriptorSet> DescCache;

    // std140 compliant config struct for the layer shader
    struct sLayerConfig
    {
        u32 uVRAMMask;
        u32 __pad0[3];
        struct sBGConfig
        {
            u32 Size[2];
            u32 Type;
            u32 PalOffset;
            u32 TileOffset;
            u32 MapOffset;
            u32 Clamp;
            u32 __pad0[1];
        } uBGConfig[4];
    } LayerConfig;

    struct sSpriteConfig
    {
        u32 uVRAMMask;
        u32 __pad0[3];
        s32 uRotscale[32][4];
        struct sOAM
        {
            s32 Position[2];
            s32 Flip[2];
            s32 Size[2];
            s32 BoundSize[2];
            u32 OBJMode;
            u32 Type;
            u32 PalOffset;
            u32 TileOffset;
            u32 TileStride;
            u32 Rotscale;
            u32 BGPrio;
            u32 Mosaic;
        } uOAM[128];
    } SpriteConfig;
    int NumSprites;
    bool SpriteUseMosaic;

    struct sScanlineConfig
    {
        struct sScanline
        {
            s32 BGOffset[4][4];     // really [4][2]
            s32 BGRotscale[2][4];
            u32 BackColor;          // 96
            u32 WinRegs;            // 100
            u32 WinMask;            // 104
            u32 __pad0[1];
            s32 WinPos[4];
            u32 BGMosaicEnable[4];
            s32 MosaicSize[4];
        } uScanline[192];
    } ScanlineConfig;

    struct sSpriteScanlineConfig
    {
        s32 uMosaicLine[192];
    } SpriteScanlineConfig;

    struct sCompositorConfig
    {
        u32 uBGPrio[4];
        u32 uEnableOBJ;
        u32 uEnable3D;
        u32 uBlendCnt;
        u32 uBlendEffect;
        u32 uBlendCoef[4];
    } CompositorConfig;

    int LastLine;

    bool UnitEnabled;

    u32 DispCnt;
    u8 LayerEnable;
    u8 OBJEnable;
    u8 ForcedBlank;
    u16 BGCnt[4];
    u16 BlendCnt;
    u8 EVA, EVB, EVY;

    u32 BGVRAMRange[4][4];

    bool LayerConfigDirty;

    int LastSpriteLine;
    u16 OAM[512];

    u32 SpriteDispCnt;
    bool SpriteConfigDirty;
    bool SpriteDirty;

    u16 TempPalBuffer[256 * (1 + (4*16))];

    // Vulkan plumbing helpers

    bool CompilePipelineShader(VkShaderModule& out, VK::Context::ShaderStage stage,
                               const std::string& source, const char* name);
    bool CreatePalView(const VK::Context::Image& img, VkImageView& out);
    void DestroyScaleDependentResources();
    void InvalidateDescriptorCache();

    u32 RingAlloc(sRingBuffer& ring, u32 size);
    void PushConfig(sConfigRing& ring, const void* data, u32 size);

    VkDescriptorSet GetDescriptorSet(VkImageView vram, VkImageView pal,
                                     const VkImageView* bgViews, const VkSampler* bgSamplers);
    VkDescriptorSet GetBGPassDescriptorSet();
    VkDescriptorSet GetOBJPassDescriptorSet();
    void BindDescriptorSet(VkDescriptorSet set);
    void PushConstants(int curBG, int renderTransparent);
    void SetViewportScissor(u32 w, u32 h, u32 scissorY, u32 scissorH);
    void BeginPass(VkRenderPass pass, VkFramebuffer fb, u32 w, u32 h);

    void BeginColorTarget(VK::Context::Image& img);
    void EndColorTarget(VK::Context::Image& img);
    void BeginTexUpload(VK::Context::Image& img);
    void EndTexUpload(VK::Context::Image& img);
    void DepthTargetBarrier();
    void UploadTexRows(VK::Context::Image& img, const void* data,
                       u32 rowStart, u32 rowCount, u32 bytesPerRow);

    // mirrors of the GLRenderer2D methods

    bool IsScreenOn();

    void UpdateAndRender(int line);

    void UpdateScanlineConfig(int line);
    void UpdateLayerConfig();
    void UpdateOAM(int ystart, int yend);
    void UpdateCompositorConfig();

    void PrerenderSprites();
    void PrerenderLayer(int layer);

    void DoRenderSprites(int line);
    void RenderSprites(bool window, int ystart, int yend);

    void RenderScreen(int ystart, int yend);
};

}
