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

#ifndef GPU3D_COMPUTEVULKAN
#define GPU3D_COMPUTEVULKAN

#include <memory>
#include <unordered_map>
#include <vector>

#include "types.h"

#include "GPU3D.h"

#include "OpenGLSupport.h"
#include "VulkanSupport.h"

#include "GPU3D_TexcacheVulkan.h"

namespace melonDS
{
class GLRenderer;

// Vulkan port of ComputeRenderer3D (the OpenGL 4.3 compute-shader
// rasteriser). Renders through a self-contained Vulkan device, then hands
// the finished frame to the OpenGL compositor by uploading it into a GL
// texture. This is what makes the "modern" renderer available on macOS,
// where OpenGL is capped below 4.3 but Vulkan works through MoltenVK.
class ComputeRenderer3D_Vulkan : public Renderer3D
{
public:
    // parent is only used for the GL-interop path (hybrid GLRenderer +
    // ComputeVulkan 3D); pass nullptr when SetVulkanNativeOutput(true) is
    // used (the all-Vulkan parent never dereferences it)
    ComputeRenderer3D_Vulkan(melonDS::GPU3D& gpu3D, GLRenderer* parent);
    ~ComputeRenderer3D_Vulkan() override;
    bool Init() override;
    void Reset() override;

    void SetRenderSettings(int scale, bool highResolutionCoordinates);

    // when the parent is the all-Vulkan renderer, the 3D output stays a
    // Vulkan image (no GL interop / readback). RenderFrame() then leaves
    // FramebufferImg in SHADER_READ_ONLY_OPTIMAL, fence-waited, ready for
    // the 2D compositor to sample. Must be set before SetRenderSettings().
    void SetVulkanNativeOutput(bool native) { VulkanNativeOutput = native; }
    const VK::Context::Image& GetOutputImage() const { return FramebufferImg; }
    VK::Context& GetContext() { return Ctx; }

    // hand the parent renderer's display-capture output images (128- and
    // 256-wide array textures) to the rasteriser so polygons that use a
    // display capture as their texture sample the real capture instead of a
    // transparent dummy. Pass VK_NULL_HANDLE to fall back to the dummy.
    void SetCaptureImages(VkImageView cap128, VkImageView cap256);

    // optional bilinear texture filtering (manual, in the rasterise shader,
    // since DS textures are integer-format). Off => nearest / accuracy-exact.
    void SetTextureFilter(bool enable) { TexFilter = enable; }

    void RenderFrame() override;
    void RestartFrame() override;
    u32* GetLine(int line) override;

    bool NeedsShaderCompile() override { return ShaderStepIdx != 33; }
    void ShaderCompileStep(int& current, int& count) override;

private:
    GLRenderer* Parent;

    VK::Context Ctx;

    // pipelines (same variant set as the GL compute renderer)
    VkPipeline ShaderInterpXSpans[2] = {};
    VkPipeline ShaderBinCombined = VK_NULL_HANDLE;
    VkPipeline ShaderDepthBlend[2] = {};
    VkPipeline ShaderRasteriseNoTexture[2] = {};
    VkPipeline ShaderRasteriseNoTextureToon[2] = {};
    VkPipeline ShaderRasteriseNoTextureHighlight[2] = {};
    VkPipeline ShaderRasteriseUseTextureDecal[2] = {};
    VkPipeline ShaderRasteriseUseTextureModulate[2] = {};
    VkPipeline ShaderRasteriseUseTextureToon[2] = {};
    VkPipeline ShaderRasteriseUseTextureHighlight[2] = {};
    VkPipeline ShaderRasteriseShadowMask[2] = {};
    VkPipeline ShaderClearCoarseBinMask = VK_NULL_HANDLE;
    VkPipeline ShaderClearIndirectWorkCount = VK_NULL_HANDLE;
    VkPipeline ShaderCalculateWorkListOffset = VK_NULL_HANDLE;
    VkPipeline ShaderSortWork = VK_NULL_HANDLE;
    VkPipeline ShaderFinalPass[8] = {};

    VkDescriptorSetLayout SetLayoutStatic = VK_NULL_HANDLE;   // set 0
    VkDescriptorSetLayout SetLayoutTexture = VK_NULL_HANDLE;  // set 1
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool DescPoolStatic = VK_NULL_HANDLE;
    VkDescriptorPool DescPoolFrame = VK_NULL_HANDLE;
    VkDescriptorSet SetStatic = VK_NULL_HANDLE;

    // buffers
    VK::Context::Buffer YSpanSetupMemory;      // host visible
    VK::Context::Buffer YSpanIndicesMemory;    // host visible
    VK::Context::Buffer RenderPolygonMemory;   // host visible
    VK::Context::Buffer MetaUniformMemory;     // host visible
    VK::Context::Buffer XSpanSetupMemory;
    VK::Context::Buffer BinResultMemory;       // also indirect dispatch source
    VK::Context::Buffer WorkDescMemory;
    VK::Context::Buffer FinalTileMemory;       // "Result" buffer of the depth/blend pass

    enum
    {
        tilememoryLayer_Color,
        tilememoryLayer_Depth,
        tilememoryLayer_Attr,
        tilememoryLayer_Num,
    };
    VK::Context::Buffer TileMemory[tilememoryLayer_Num];

    VK::Context::Buffer ReadbackBuffer;        // host visible, framebuffer readback
    alignas(8) u32 ReadbackLine[256] {};
    bool ReadbackValid = false;

    // images
    VK::Context::Image ClearBitmapImg[2];
    VK::Context::Image FramebufferImg;
    VK::Context::Image DummyTexture;           // 1x1 RGBA8UI array
    VK::Context::Image DummyCapture;           // 1x1 RGBA8 array

    // parent renderer's capture output views (not owned); null => use dummy
    VkImageView ExtCapture128 = VK_NULL_HANDLE;
    VkImageView ExtCapture256 = VK_NULL_HANDLE;

    bool TexFilter = false;

    VkSampler Samplers[9] = {};
    VkSampler ClearBitmapSampler = VK_NULL_HANDLE;
    VkSampler CaptureSampler = VK_NULL_HANDLE;

    VkCommandBuffer FrameCmd = VK_NULL_HANDLE;
    VkFence FrameFence = VK_NULL_HANDLE;
    // native-output path pipelining: submit the compute frame without blocking
    // and reclaim the fence at the start of the next RenderFrame, so the 2D
    // compositor's next-frame wait no longer serialises behind this one
    bool SubmitPending = false;

    struct SpanSetupY
    {
        // Attributes
        s32 Z0, Z1, W0, W1;
        s32 ColorR0, ColorG0, ColorB0;
        s32 ColorR1, ColorG1, ColorB1;
        s32 TexcoordU0, TexcoordV0;
        s32 TexcoordU1, TexcoordV1;

        // Interpolator
        s32 I0, I1;
        s32 Linear;
        s32 IRecip;
        s32 W0n, W0d, W1d;

        // Slope
        s32 Increment;

        s32 X0, X1, Y0, Y1;
        s32 XMin, XMax;
        s32 DxInitial;

        s32 XCovIncr;
        u32 IsDummy;
    };
    struct SpanSetupX
    {
        s32 X0, X1;

        s32 EdgeLenL, EdgeLenR, EdgeCovL, EdgeCovR;

        s32 XRecip;

        u32 Flags;

        s32 Z0, Z1, W0, W1;
        s32 ColorR0, ColorG0, ColorB0;
        s32 ColorR1, ColorG1, ColorB1;
        s32 TexcoordU0, TexcoordV0;
        s32 TexcoordU1, TexcoordV1;

        s32 CovLInitial, CovRInitial;
    };
    struct SetupIndices
    {
        u16 PolyIdx, SpanIdxL, SpanIdxR, Y;
    };
    struct RenderPolygon
    {
        u32 FirstXSpan;
        s32 YTop, YBot;

        s32 XMin, XMax;
        s32 XMinY, XMaxY;

        u32 Variant;
        u32 Attr;

        float TextureLayer;
    };

    struct RasterPushConstants
    {
        u32 CurVariant;
        s32 TexIsCapture;
        float InvTextureSize[2];
        float CaptureYOffset;
        s32 FilterTex;
    };

    int TileSize;
    static constexpr int CoarseTileCountX = 8;
    int CoarseTileCountY;
    int CoarseTileArea;
    int CoarseTileW;
    int CoarseTileH;
    int ClearCoarseBinMaskLocalSize;

    static constexpr int BinStride = 2048/32;
    static constexpr int CoarseBinStride = BinStride/32;

    static constexpr int MaxVariants = 256;

    static constexpr int MaxFullscreenLayers = 16;

    struct BinResultHeader
    {
        u32 VariantWorkCount[MaxVariants*4];
        u32 SortedWorkOffset[MaxVariants];

        u32 SortWorkWorkCount[4];
    };

    static const int MaxYSpanSetups = 6144*2;
    std::vector<SetupIndices> YSpanIndices;
    SpanSetupY YSpanSetups[MaxYSpanSetups];
    RenderPolygon RenderPolygons[2048];

    TexcacheVulkan Texcache;

    struct MetaUniform
    {
        u32 NumPolygons;
        u32 NumVariants;

        u32 AlphaRef;
        u32 DispCnt;

        u32 ToonTable[4*34];

        u32 ClearColor, ClearDepth, ClearAttr;

        u32 FogOffset, FogShift, FogColor;

        float ClearBitmapOffset[2];
    };

    u32* ClearBitmap[2] = {};
    u8 ClearBitmapDirty = 0;

    GLuint OutputGLTex = 0;
    bool VulkanNativeOutput = false;

    int ScreenWidth = 0, ScreenHeight = 0;
    int TilesPerLine = 0, TileLines = 0;
    int ScaleFactor = -1;
    int MaxWorkTiles = 0;
    bool HiresCoordinates = false;

    int ShaderStepIdx = 0;

    void DeleteShaders();
    void DestroyScaleDependentResources();

    void SetupAttrs(SpanSetupY* span, Polygon* poly, int from, int to);
    void SetupYSpan(RenderPolygon* rp, SpanSetupY* span, Polygon* poly, int from, int to, int side, s32 positions[10][2]);
    void SetupYSpanDummy(RenderPolygon* rp, SpanSetupY* span, Polygon* poly, int vertex, int side, s32 positions[10][2]);

    bool CompileShader(VkPipeline& pipeline, const std::string& source, const std::initializer_list<const char*>& defines);

    void UpdateStaticDescriptorSet();
    VkDescriptorSet GetTextureDescriptorSet(VkImageView view, VkSampler sampler);
    void ComputeToComputeBarrier(bool indirect);

    // per-frame descriptor set cache, cleared each frame
    std::unordered_map<u64, VkDescriptorSet> FrameTextureSets;
};

}

#endif
