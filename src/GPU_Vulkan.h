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

#ifndef GPU_VULKAN_H
#define GPU_VULKAN_H

#include <array>
#include <map>

#include "OpenGLSupport.h"
#include "VulkanSupport.h"
#include "GPU.h"
#include "GPU2D_Vulkan.h"
#include "GPU3D_ComputeVulkan.h"

namespace melonDS
{

// Vulkan port of GLRenderer (src/GPU_OpenGL.{h,cpp}): the full-Vulkan
// renderer parent. Owns two VulkanRenderer2D units (engine A/B) and a
// ComputeRenderer3D_Vulkan running in native-output mode (its framebuffer
// stays a Vulkan image, never round-tripping through GL), all sharing one
// VK::Context and one queue.
//
// Frame model: a synchronous, once-per-frame command buffer. The parent
// opens its own command buffer (separate from the 3D renderer's) at the
// first DrawScanline() of a frame, hands it to both 2D units per band via
// SetCommandBuffer(), records its own FinalPass/Capture into the same
// buffer right after each band's compositor output is ready, and at
// VBlank() ends + submits + fence-waits it. The 3D renderer runs one frame
// ahead (RenderFrame() at VCount 215, already fence-waited/synchronous by
// the time it returns) so its output image has no read/write hazard
// against the 2D units sampling it during the following frame; it is not
// double-buffered.
//
// Presentation stays GL-interop, mirroring ComputeRenderer3D_Vulkan: the
// FinalPass output (a 2-layer RGBA8 image, layer0=top/layer1=bottom) is
// read back to a host buffer at VBlank and uploaded into a
// GL_TEXTURE_2D_ARRAY (FPOutputTex[2], double-buffered exactly like
// GLRenderer's) via glTexSubImage3D, since the frontend still expects GL
// texture handles from GetFramebuffers().
class VulkanRenderer : public Renderer
{
public:
    explicit VulkanRenderer(melonDS::NDS& nds);
    ~VulkanRenderer() override;
    bool Init() override;
    void Reset() override;
    void Stop() override;

    void PostSavestate() override;

    void SetRenderSettings(RendererSettings& settings) override;

    void DrawScanline(u32 line) override;
    void DrawSprites(u32 line) override;

    void VBlank() override;
    void VBlankEnd() override;

    void AllocCapture(u32 bank, u32 start, u32 len) override;
    void SyncVRAMCapture(u32 bank, u32 start, u32 len, bool complete) override;

    bool GetFramebuffers(void** top, void** bottom) override;

    bool NeedsShaderCompile() override;
    void ShaderCompileStep(int& current, int& count) override;

private:
    // borrowed from Rend3D (which owns the VK::Context); shared with
    // Rend2D_A/B, set once construction of Rend3D has happened
    VK::Context* Ctx = nullptr;

    int ScaleFactor;
    int ScreenW, ScreenH;

    // optional additive enhancements (off => accuracy-exact)
    bool EnableDither = false;
    bool EnableTexFilter = false;

    // ---- per-frame command buffer plumbing ----
    // own submission, entirely separate from Rend3D's FrameCmd/FrameFence
    VkCommandBuffer FrameCmd = VK_NULL_HANDLE;
    VkFence FrameFence = VK_NULL_HANDLE;
    bool FrameStarted = false;
    // == FrameCmd while recording a frame's bands; temporarily repointed at
    // a one-shot command buffer while SyncVRAMCapture forces an out-of-band
    // flush (see .cpp for why)
    VkCommandBuffer CurCmd = VK_NULL_HANDLE;

    void EnsureFrameStarted();
    void SubmitAndWaitFrame(); // ends + submits + fence-waits FrameCmd; clears FrameStarted

    // generic per-frame linear allocator (vertex data, staging uploads),
    // mirrors VulkanRenderer2D::sRingBuffer
    struct sRingBuffer
    {
        VK::Context::Buffer Buf;
        u32 Offset = 0;
    };
    u32 RingAlloc(sRingBuffer& ring, u32 size);

    // ring of config versions bound through a dynamic UBO offset, so each
    // recorded band's draw keeps the config as it was when recorded even
    // though later bands overwrite the same struct in RAM before the whole
    // command buffer is submitted; mirrors VulkanRenderer2D::sConfigRing
    struct sConfigRing
    {
        VK::Context::Buffer Buf;
        u32 Stride = 0;
        u32 Slots = 0;
        u32 Next = 0;
        u32 CurOffset = 0;
    };
    bool InitConfigRing(sConfigRing& ring, u32 size, u32 slots);
    void PushConfig(sConfigRing& ring, const void* data, u32 size);

    void BeginColorTarget(VK::Context::Image& img);
    void EndColorTarget(VK::Context::Image& img);
    void BeginTexUpload(VK::Context::Image& img);
    void EndTexUpload(VK::Context::Image& img);
    void UploadTexRows(VK::Context::Image& img, const void* data,
                       u32 rowStart, u32 rowCount, u32 bytesPerRow, u32 layer);

    VkSampler SamplerNearestClamp = VK_NULL_HANDLE;
    VkSampler SamplerNearestRepeat = VK_NULL_HANDLE;
    VkSampler SamplerNearestBorder = VK_NULL_HANDLE;

    // ---- FinalPass (GL: FPShader / FinalPassVS/FS.glsl) ----

    struct sFinalPassConfig
    {
        u32 uScreenSwap[192];
        u32 uScaleFactor;
        u32 uAuxLayer;
        u32 uDispModeA;
        u32 uDispModeB;
        u32 uBrightModeA;
        u32 uBrightModeB;
        u32 uBrightFactorA;
        u32 uBrightFactorB;
        float uAuxColorFactor;
        u32 uDither;
        u32 __pad0[2];
    } FinalPassConfig;

    VkDescriptorSetLayout FPSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout FPPipelineLayout = VK_NULL_HANDLE;
    VkRenderPass FPRenderPass = VK_NULL_HANDLE;   // 2 color attachments, RGBA8, loaded (never cleared)
    VkPipeline FPPipeline = VK_NULL_HANDLE;

    VK::Context::Buffer FPVertexBuffer;           // fullscreen NDS quad, vPosition only (GL: FPVertexArrayID)
    sConfigRing FPConfigRing;

    // MRT target: layer0 = top screen output, layer1 = bottom screen output
    // (GL: FPOutputTex[2] array layers via glFramebufferTextureLayer)
    VK::Context::Image FPOutputImg;
    VkImageView FPOutputView[2] = {};
    VkFramebuffer FPFramebuffer = VK_NULL_HANDLE;

    VkDescriptorPool FPDescPool = VK_NULL_HANDLE;
    VkDescriptorSet FPDescSet = VK_NULL_HANDLE;

    VK::Context::Buffer FPReadbackBuffer;         // host visible, ScreenW*ScreenH*2 layers*4 bytes

    // GL interop: presentation textures, mirrors GLRenderer::FPOutputTex
    // EXACTLY (GL_TEXTURE_2D_ARRAY, RGBA8, ScreenW x ScreenH x 2, layer0=top
    // /layer1=bottom, double-buffered and flipped by BackBuffer)
    GLuint FPOutputTex[2] = {};

    // ---- AuxInput (GL: AuxInputTex, VRAM-display / mainmem DISP FIFO) ----

    // raw DS RGBA5551 data, same packed format as the 2D units' palette
    // textures; A1R5G5B5_UNORM_PACK16 + a r<->b swizzled view recovers the
    // channel order the shader expects (see VulkanRenderer2D::CreatePalView)
    VK::Context::Image AuxInputImg;               // 256 x 256 x 2 layers
    VkImageView AuxInputView = VK_NULL_HANDLE;
    sRingBuffer AuxStagingRing;

    u16* AuxInputBuffer[2];
    u8 AuxUsageMask;

    // ---- Capture (GL: CaptureShader, CaptureOutput128/256, CaptureVRAM) ----

    struct sCaptureConfig
    {
        float uInvCaptureSize[2];
        u32 uSrcALayer;
        u32 uSrcBLayer;
        u32 uSrcBOffset;
        u32 uDstMode;
        u32 uBlendFactors[2];
        float uSrcAOffset[192];
        float uSrcBColorFactor;
        u32 __pad0[3];
    } CaptureConfig;

    VkDescriptorSetLayout CaptureSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout CapturePipelineLayout = VK_NULL_HANDLE;
    VkRenderPass CaptureRenderPass = VK_NULL_HANDLE; // 1 color attachment, RGBA8, loaded
    VkPipeline CapturePipeline = VK_NULL_HANDLE;

    sConfigRing CaptureConfigRing;
    sRingBuffer CaptureVertexRing;                // per-band vertex geometry (GL: CaptureVtxBuffer)

    VkDescriptorPool CaptureDescPool = VK_NULL_HANDLE;
    // keyed by {InputTexA view, InputTexB view}; the UBO binding is constant
    // (bound through a dynamic offset instead), mirrors
    // VulkanRenderer2D::DescCache
    std::map<std::array<uintptr_t, 2>, VkDescriptorSet> CaptureDescCache;

    VK::Context::Image CaptureOutput256Img;       // RGBA8 array, 4 layers (one per VRAM bank)
    VkFramebuffer CaptureOutput256FB[4] = {};
    VK::Context::Image CaptureOutput128Img;       // RGBA8 array, 16 layers (4 banks x 4 offsets)
    VkFramebuffer CaptureOutput128FB[16] = {};
    VK::Context::Image CaptureVRAMImg;            // RGBA8 array, 1 layer; same-bank hazard temp

    // ---- CaptureDownscale (GL: CapDownShader, CaptureSync) ----

    VkDescriptorSetLayout CapDownSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout CapDownPipelineLayout = VK_NULL_HANDLE;
    VkRenderPass CapDownRenderPass = VK_NULL_HANDLE; // 1 color attachment, RGBA8, cleared (self-contained one-shot)
    VkPipeline CapDownPipeline = VK_NULL_HANDLE;

    VkDescriptorPool CapDownDescPool = VK_NULL_HANDLE;
    VkDescriptorSet CapDown128Set = VK_NULL_HANDLE;
    VkDescriptorSet CapDown256Set = VK_NULL_HANDLE;

    VK::Context::Buffer RectVtxBuffer;            // fullscreen unit rect [0,1], shared helper geometry

    // downscaled (1x IR) capture, pre-pack; kept as plain RGBA8 rather than
    // a 16-bit packed format for portability (see .cpp for rationale) -
    // SyncVRAMCapture packs it down to RGBA5551 on the CPU side, matching
    // what glReadPixels(..., GL_UNSIGNED_SHORT_1_5_5_5_REV, ...) did for GL
    VK::Context::Image CaptureSyncImg;             // RGBA8, 256x256
    VkFramebuffer CaptureSyncFB = VK_NULL_HANDLE;
    VK::Context::Buffer CaptureSyncReadback;       // host visible, 256*256*4 bytes

    // ---- CPU-side state (near-verbatim mirror of GLRenderer) ----

    u32 DispCntA, DispCntB;
    u16 MasterBrightnessA, MasterBrightnessB;
    u32 CaptureCnt;

    bool NeedPartialRender;
    int LastLine;
    int LastCapLine;
    int Aux0VRAMCap;

    void SetScaleFactor(int scale);
    void DestroyScaleDependentResources();
    void InvalidateCaptureDescCache();

    void RenderScreen(int ystart, int yend);
    void DoCapture(int ystart, int yend);
    void DownscaleCapture(VkCommandBuffer cmd, int width, int height, int layer);

    VkDescriptorSet GetCaptureDescriptorSet(VkImageView viewA, VkImageView viewB);
};

}

#endif // GPU_VULKAN_H
