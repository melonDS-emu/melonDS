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

#ifndef GPU3D_VULKAN_H
#define GPU3D_VULKAN_H

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "GPU3D.h"
#include "VulkanSupport.h"

namespace melonDS
{

class VulkanRenderer3D : public Renderer3D
{
public:
    VulkanRenderer3D(melonDS::GPU3D& gpu3D, Vulkan::Context& context) noexcept;
    ~VulkanRenderer3D() override;

    bool Init() override;
    void Reset() override;
    void SetRenderSettings(int scale) noexcept;

    void RenderFrame() override;
    u32* GetLine(int line) override;

private:
    static constexpr u32 ScreenWidth = 256;
    static constexpr u32 ScreenHeight = 192;
    static constexpr VkDeviceSize FramebufferSize = ScreenWidth * ScreenHeight * sizeof(u32);
    static constexpr u32 TileSize = 8;
    static constexpr u32 TilesPerLine = ScreenWidth / TileSize;
    static constexpr u32 TileLines = ScreenHeight / TileSize;
    static constexpr u32 MaxWorkTiles = TilesPerLine * TileLines * 16;
    static constexpr u32 MaxYSpanIndices = 64 * 2048;
    static constexpr u32 MaxYSpanSetups = 6144 * 2;
    static constexpr u32 BinStride = 2048 / 32;
    static constexpr u32 CoarseBinStride = BinStride / 32;
    static constexpr u32 MaxVariants = 256;

    struct Buffer
    {
        VkBuffer Handle = VK_NULL_HANDLE;
        VkDeviceMemory Memory = VK_NULL_HANDLE;
        VkDeviceSize Size = 0;
        void* Mapped = nullptr;
    };

    struct SpanSetupY
    {
        s32 Z0, Z1, W0, W1;
        s32 ColorR0, ColorG0, ColorB0;
        s32 ColorR1, ColorG1, ColorB1;
        s32 TexcoordU0, TexcoordV0;
        s32 TexcoordU1, TexcoordV1;
        s32 I0, I1;
        s32 Linear;
        s32 IRecip;
        s32 W0n, W0d, W1d;
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

    struct TextureResource
    {
        VkImage Image = VK_NULL_HANDLE;
        VkDeviceMemory Memory = VK_NULL_HANDLE;
        VkImageView View = VK_NULL_HANDLE;
    };

    struct Variant
    {
        u32 TexParam;
        u32 TexPalette;
        u16 Width;
        u16 Height;
        u8 BlendMode;
        bool UsesTexture;
        TextureResource* Texture = nullptr;
        VkSampler Sampler = VK_NULL_HANDLE;

        bool operator==(const Variant& other) const noexcept;
    };

    Vulkan::Context& Context;

    VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
    VkFence Fence = VK_NULL_HANDLE;
    VkImage ColorImage = VK_NULL_HANDLE;
    VkDeviceMemory ColorMemory = VK_NULL_HANDLE;
    VkImageView ColorView = VK_NULL_HANDLE;
    VkBuffer ReadbackBuffer = VK_NULL_HANDLE;
    VkDeviceMemory ReadbackMemory = VK_NULL_HANDLE;
    u32* Framebuffer = nullptr;

    VkImageLayout ColorLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::array<u32, ScreenWidth> ScrolledLine {};
    std::vector<VkShaderModule> ShaderModules;
    std::vector<VkPipeline> ComputePipelines;

    VkDescriptorSetLayout BufferSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout UniformSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout SpanImageSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout TextureSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout OutputImageSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout BasePipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout SpanPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout RasterPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout TexturePipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout OutputPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet SpanBufferSet = VK_NULL_HANDLE;
    VkDescriptorSet RasterBufferSet = VK_NULL_HANDLE;
    VkDescriptorSet UniformSet = VK_NULL_HANDLE;
    VkDescriptorSet SpanImageSet = VK_NULL_HANDLE;
    VkDescriptorSet ClearTextureSet = VK_NULL_HANDLE;
    VkDescriptorSet OutputImageSet = VK_NULL_HANDLE;

    Buffer PolygonBuffer;
    Buffer XSpanBuffer;
    Buffer YSpanBuffer;
    Buffer SetupIndicesBuffer;
    Buffer ColorTileBuffer;
    Buffer DepthTileBuffer;
    Buffer AttrTileBuffer;
    Buffer ResultBuffer;
    Buffer BinResultBuffer;
    Buffer WorkBuffer;
    Buffer MetaBuffer;
    Buffer ClearUploadBuffer;
    Buffer TextureUploadBuffer;
    VkBufferView SetupIndicesView = VK_NULL_HANDLE;
    VkImage ClearImages[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkDeviceMemory ClearImageMemory[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkImageView ClearImageViews[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkImageLayout ClearImageLayouts[2] = {VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_UNDEFINED};
    VkSampler ClearSampler = VK_NULL_HANDLE;
    VkSampler TextureSamplers[9] = {};
    VkImage DummyIntegerImage = VK_NULL_HANDLE;
    VkDeviceMemory DummyIntegerMemory = VK_NULL_HANDLE;
    VkImageView DummyIntegerView = VK_NULL_HANDLE;
    VkImage DummyCaptureImage = VK_NULL_HANDLE;
    VkDeviceMemory DummyCaptureMemory = VK_NULL_HANDLE;
    VkImageView DummyCaptureView = VK_NULL_HANDLE;
    VkImageLayout DummyImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::array<VkDescriptorSet, MaxVariants> RasterTextureSets {};
    std::unordered_map<u64, std::unique_ptr<TextureResource>> TextureCache;
    std::vector<SetupIndices> YSpanIndices;
    std::array<SpanSetupY, MaxYSpanSetups> YSpanSetups {};
    std::array<RenderPolygon, 2048> RenderPolygons {};
    std::vector<Variant> FrameVariants;
    u32 FrameYSpanCount = 0;
    u32 FrameSetupIndexCount = 0;
    bool FrameWBuffer = false;

    bool CreateColorImage();
    bool CreateReadbackBuffer();
    bool CompileShaders();
    bool CompileShader(const std::string& source, const std::vector<const char*>& defines,
                       const char* name);
    bool CreateComputePipelines();
    bool CreateComputeResources();
    bool CreateBuffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage);
    bool CreateImage(u32 width, u32 height, VkFormat format, VkImageUsageFlags usage,
                     VkImage& image, VkDeviceMemory& memory, VkImageView& view,
                     VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
                     u32 layers = 1);
    void DestroyBuffer(Buffer& buffer);
    bool CreateDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings,
                                   VkDescriptorSetLayout& layout);
    bool CreatePipelineLayout(const std::vector<VkDescriptorSetLayout>& setLayouts,
                              bool pushConstants, VkPipelineLayout& layout);
    bool SubmitFrame(bool bitmapClear);
    void PrepareBitmapClear();
    bool PreparePolygons(u32& numYSpans, u32& numSetupIndices,
                         std::vector<Variant>& variants, bool& wBuffer);
    bool PrepareTextures(std::vector<Variant>& variants);
    TextureResource* GetTexture(u32 texParam, u32 texPalette);
    bool UploadTexture(TextureResource& texture, u32 width, u32 height,
                       const u32* pixels);
    void ResetTextureCache();
    void SetupAttrs(SpanSetupY* span, Polygon* polygon, int from, int to);
    void SetupYSpan(RenderPolygon* renderPolygon, SpanSetupY* span, Polygon* polygon,
                    int from, int to, int side, s32 positions[10][2]);
    void SetupYSpanDummy(RenderPolygon* renderPolygon, SpanSetupY* span, Polygon* polygon,
                         int vertex, int side, s32 positions[10][2]);
    void DestroyResources();
};

}

#endif // GPU3D_VULKAN_H
