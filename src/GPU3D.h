/*
    Copyright 2016-2023 melonDS team

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

#ifndef GPU3D_H
#define GPU3D_H

#include <array>
#include <memory>

#include "Savestate.h"
#include "FIFO.h"


namespace melonDS
{
class GPU;


struct Vertex
{
    s32 Position[4];
    s32 Color[3];
    s16 TexCoords[2];

    bool Clipped;

    // final vertex attributes.
    // allows them to be reused in polygon strips.

    s32 FinalPosition[2];
    s32 FinalColor[3];

    // hi-res position (4-bit fractional part)
    // TODO maybe: hi-res color? (that survives clipping)
    s32 HiresPosition[2];

    void DoSavestate(Savestate* file) noexcept;
};

struct Polygon
{
    Vertex* Vertices[10];
    u32 NumVertices;

    s32 FinalZ[10];
    s32 FinalW[10];
    bool WBuffer;

    u32 Attr;
    u32 TexParam;
    u32 TexPalette;

    bool Degenerate;

    bool FacingView;
    bool Translucent;

    bool IsShadowMask;
    bool IsShadow;

    int Type; // 0=regular 1=line

    u32 VTop, VBottom; // vertex indices
    s32 YTop, YBottom; // Y coords
    s32 XTop, XBottom; // associated X coords

    u32 SortKey;

    void DoSavestate(Savestate* file) noexcept;
};

class Renderer3D;
class NDS;

class GPU3D
{
public:
    GPU3D(melonDS::NDS& nds, std::unique_ptr<Renderer3D>&& renderer = nullptr) noexcept;
    ~GPU3D() noexcept = default;
    void Reset() noexcept;

    void DoSavestate(Savestate* file) noexcept;

    void SetEnabled(bool geometry, bool rendering) noexcept;

    void ExecuteCommand() noexcept;

    s32 CyclesToRunFor() const noexcept;
    void Run() noexcept;
    void CheckFIFOIRQ() noexcept;
    void CheckFIFODMA() noexcept;

    void VCount144(GPU& gpu) noexcept;
    void VBlank() noexcept;
    void VCount215(GPU& gpu) noexcept;

    void RestartFrame(GPU& gpu) noexcept;
    void Stop(const GPU& gpu) noexcept;

    void SetRenderXPos(u16 xpos) noexcept;
    [[nodiscard]] u16 GetRenderXPos() const noexcept { return RenderXPos; }
    void ScanlineSync(int line) noexcept;
    u32* GetLine(int line) noexcept;

    void WriteToGXFIFO(u32 val) noexcept;

    [[nodiscard]] bool IsRendererAccelerated() const noexcept;
    [[nodiscard]] Renderer3D& GetCurrentRenderer() noexcept { return *CurrentRenderer; }
    [[nodiscard]] const Renderer3D& GetCurrentRenderer() const noexcept { return *CurrentRenderer; }
    void SetCurrentRenderer(std::unique_ptr<Renderer3D>&& renderer) noexcept;

    u8 Read8(u32 addr) noexcept;
    u16 Read16(u32 addr) noexcept;
    u32 Read32(u32 addr) noexcept;
    void Write8(u32 addr, u8 val) noexcept;
    void Write16(u32 addr, u16 val) noexcept;
    void Write32(u32 addr, u32 val) noexcept;
    void Blit(const GPU& gpu) noexcept;
private:
    melonDS::NDS& NDS;
    typedef union
    {
        u64 _contents;
        struct
        {
            u32 Param;
            u8 Command;
        };

    } CmdFIFOEntry;

    void UpdateClipMatrix() noexcept;
    void ResetRenderingState() noexcept;
    void AddCycles(s32 num) noexcept;
    void NextVertexSlot() noexcept;
    void StallPolygonPipeline(s32 delay, s32 nonstalldelay) noexcept;
    void SubmitPolygon() noexcept;
    void SubmitVertex() noexcept;
    void CalculateLighting() noexcept;
    void BoxTest(const u32* params) noexcept;
    void PosTest() noexcept;
    void VecTest(u32 param) noexcept;
    void CmdFIFOWrite(const CmdFIFOEntry& entry) noexcept;
    CmdFIFOEntry CmdFIFORead() noexcept;
    void FinishWork(s32 cycles) noexcept;
    void VertexPipelineSubmitCmd() noexcept
    {
        // vertex commands 0x24, 0x25, 0x26, 0x27, 0x28
        if (!(VertexSlotsFree & 0x1)) NextVertexSlot();
        else                          AddCycles(1);
        NormalPipeline = 0;
    }

    void VertexPipelineCmdDelayed6() noexcept
    {
        // commands 0x20, 0x30, 0x31, 0x72 that can run 6 cycles after a vertex
        if (VertexPipeline > 2) AddCycles((VertexPipeline - 2) + 1);
        else                    AddCycles(NormalPipeline + 1);
        NormalPipeline = 0;
    }

    void VertexPipelineCmdDelayed8() noexcept
    {
        // commands 0x29, 0x2A, 0x2B, 0x33, 0x34, 0x41, 0x60, 0x71 that can run 8 cycles after a vertex
        if (VertexPipeline > 0) AddCycles(VertexPipeline + 1);
        else                    AddCycles(NormalPipeline + 1);
        NormalPipeline = 0;
    }

    void VertexPipelineCmdDelayed4() noexcept
    {
        // all other commands can run 4 cycles after a vertex
        // no need to do much here since that is the minimum
        AddCycles(NormalPipeline + 1);
        NormalPipeline = 0;
    }

    std::unique_ptr<Renderer3D> CurrentRenderer = nullptr;

    u16 RenderXPos = 0;

public:
    FIFO<CmdFIFOEntry, 256> CmdFIFO {};
    FIFO<CmdFIFOEntry, 4> CmdPIPE {};

    FIFO<CmdFIFOEntry, 64> CmdStallQueue {};

    u32 ZeroDotWLimit = 0;

    u32 GXStat = 0;

    u32 ExecParams[32] {};
    u32 ExecParamCount = 0;

    s32 CycleCount = 0;
    s32 VertexPipeline = 0;
    s32 NormalPipeline = 0;
    s32 PolygonPipeline = 0;
    s32 VertexSlotCounter = 0;
    u32 VertexSlotsFree = 0;

    u32 NumPushPopCommands = 0;
    u32 NumTestCommands = 0;


    u32 MatrixMode = 0;

    s32 ProjMatrix[16] {};
    s32 PosMatrix[16] {};
    s32 VecMatrix[16] {};
    s32 TexMatrix[16] {};

    s32 ClipMatrix[16] {};
    bool ClipMatrixDirty = false;

    u32 Viewport[6] {};

    s32 ProjMatrixStack[16] {};
    s32 PosMatrixStack[32][16] {};
    s32 VecMatrixStack[32][16] {};
    s32 TexMatrixStack[16] {};
    s32 ProjMatrixStackPointer = 0;
    s32 PosMatrixStackPointer = 0;
    s32 TexMatrixStackPointer = 0;

    u32 NumCommands = 0;
    u32 CurCommand = 0;
    u32 ParamCount = 0;
    u32 TotalParams = 0;

    bool GeometryEnabled = false;
    // 0 = powered off
    // 1 = powered on, inactive
    // 2 = one swap buffers, inactive
    // 3 = two swap buffers, active;
    u8 RenderingEnabled = 0;

    u32 DispCnt = 0;

    u16 UnderflowFlagVCount = 0;
    u8 RDLines = 0;
    u8 RDLinesTemp = 0;

    u8 AlphaRefVal = 0;
    u8 AlphaRef = 0;

    u16 ToonTable[32] {};
    u16 EdgeTable[8] {};

    u32 FogColor = 0;
    u32 FogOffset = 0;
    u8 FogDensityTable[32] {};

    u32 ClearAttr1 = 0;
    u32 ClearAttr2 = 0;

    u32 RenderDispCnt = 0;
    u8 RenderAlphaRef = 0;

    u16 RenderToonTable[32] {};
    u16 RenderEdgeTable[8] {};

    u32 RenderFogColor = 0;
    u32 RenderFogOffset = 0;
    u32 RenderFogShift = 0;
    u8 RenderFogDensityTable[34] {};

    u32 RenderClearAttr1 = 0;
    u32 RenderClearAttr2 = 0;

    bool RenderFrameIdentical = false; // not part of the hardware state, don't serialize

    bool AbortFrame = false;

    u64 Timestamp = 0;


    u32 PolygonMode = 0;
    s16 CurVertex[3] {};
    u8 VertexColor[3] {};
    s16 TexCoords[2] {};
    s16 RawTexCoords[2] {};
    s16 Normal[3] {};

    s16 LightDirection[4][3] {};
    u8 LightColor[4][3] {};
    u8 MatDiffuse[3] {};
    u8 MatAmbient[3] {};
    u8 MatSpecular[3] {};
    u8 MatEmission[3] {};

    bool UseShininessTable = false;
    u8 ShininessTable[128] {};

    u32 PolygonAttr = 0;
    u32 CurPolygonAttr = 0;

    u32 TexParam = 0;
    u32 TexPalette = 0;

    s32 PosTestResult[4] {};
    s16 VecTestResult[3] {};

    Vertex TempVertexBuffer[4] {};
    u32 VertexNum = 0;
    u32 VertexNumInPoly = 0;
    u32 NumConsecutivePolygons = 0;
    Polygon* LastStripPolygon = nullptr;
    u32 NumOpaquePolygons = 0;

    Vertex VertexRAM[6144 * 2] {};
    Polygon PolygonRAM[2048 * 2] {};

    Vertex* CurVertexRAM = nullptr;
    Polygon* CurPolygonRAM = nullptr;
    u32 NumVertices = 0;
    u32 NumPolygons = 0;
    u32 CurRAMBank = 0;

    std::array<Polygon*,2048> RenderPolygonRAM {};
    u32 RenderNumPolygons = 0;

    u32 FlushRequest = 0;
    u32 FlushAttributes = 0;
    u32 ScrolledLine[256]; // not part of the hardware state, don't serialize
};

    // Rasterization Timing Constants

    static constexpr int TimingFrac = 1; // add a fractional component if pixels is not enough precision

    // GPU 2D Read Timings: For Emulating Buffer Read/Write Race Conditions
    static constexpr int DelayBetweenReads = 809 * TimingFrac;
    static constexpr int ScanlineReadSpeed = 256 * TimingFrac;
    static constexpr int ScanlineReadInc = DelayBetweenReads + ScanlineReadSpeed;
    static constexpr int InitGPU2DTimeout = 51875 * TimingFrac; // 51618? 51874? 52128? | when it finishes reading the first scanline.
    static constexpr int FrameLength = ScanlineReadInc * 263; // how long the entire frame is. TODO: Verify if we actually need this?
        
    // compile-time list of scanline read times
    // these *should* always occur at the same point in each frame, so it shouldn't matter if we make them fixed
    static constexpr std::array<u32, 192> SLRead = []() constexpr {
    std::array<u32, 192> readtime {};

    for (int i = 0, time = InitGPU2DTimeout; i < 192; i++, time += ScanlineReadInc)
    {
        readtime[i] = time;
    }
    return readtime;
    }();

    static constexpr int Arbitrary = 565; // extra value after the scanline is read at which the cutoff of a scanline should be...?
                                          // idk why this is needed. im probably doing something wrong.

    // the point at which rdlines decrements. not sure why it's different...?
    static constexpr std::array<u32, 192> RDDecrement = []() constexpr {
    std::array<u32, 192> dec {};

    for (int i = 0; i < 192; i++)
    {
        dec[i] = SLRead[i] + Arbitrary - 39 - (!(i % 2));
    }
    return dec;
    }();

    // GPU 3D Rasterization Timings: For Emulating Scanline Timeout

    static constexpr int FinalPassLen = 500 * TimingFrac; // 496 (might technically be 500?) | the next scanline cannot begin while a scanline's final pass is in progress
                                                          // (can be interpreted as the minimum amount of cycles for the next scanline
                                                          // pair to start after the previous pair began) (related to final pass?)
    static constexpr int ScanlinePushDelay = 242 * TimingFrac;
    static constexpr int EMGlitchThreshhold = 502 * TimingFrac; // The threshold for the edge marking glitch behavior to change.
    static constexpr int EMFixNum = 571 * TimingFrac; // Arbitrary value added to fix edge marking glitch, not sure why it's needed?

    // GPU 3D Rasterization Timings II: For Tracking Timing Behaviors

    //static constexpr int FirstPolyScanline = 0 * TimingFrac; 
    static constexpr int PerPolyScanline = 12 * TimingFrac; // 12 | The basic timing cost for polygons. Applies per polygon per scanline.
    static constexpr int PerPixelTiming = 1 * TimingFrac; // 1 | 1 pixel = 1 pixel
    static constexpr int NumFreePixels = 4; // 4 | First 4 pixels in a polygon scanline are free (for some reason)
    static constexpr int MinToStartPoly = 2 * TimingFrac; // 1 | if there aren't 2 (why two?) cycles remaining after the polygon timing penalty,
                                                          // do not bother rendering the polygon (CHECKME: I dont think this should decrement timings by anything?)
    static constexpr int EmptyPolyScanline = 4 * TimingFrac; // 4 | the ignored "empty" bottom-most scanline of a polygon
                                                             // which shouldn't be rendered for some reason has timing characteristics.

    // GPU 3D Rasterization Timings III, For First Polygon "Pre-Calc" Timings
    // should be added before other timings, as these are "async" pre-calcs of polygon attributes

    static constexpr int FirstPerSlope = 1 * TimingFrac; // 1 | for each "slope" the first polygon has in this scanline increment it by 1.
                                                         // (see DoTimingsSlopes() in GPU3D_Soft.cpp for more info)
    static constexpr int FirstNull = 1 * TimingFrac; // 1 | if the first polygon is "null" (probably wrong?)
    static constexpr int FirstPolyDelay = 4 * TimingFrac; // 4 | Min amount of cycles to begin a scanline? (minimum time it takes to init the first polygon?)
                                                          // (Amount of time before the end of the cycle a scanline must abort?)


class Renderer3D
{
public:
    virtual ~Renderer3D() = default;

    Renderer3D(const Renderer3D&) = delete;
    Renderer3D& operator=(const Renderer3D&) = delete;

    virtual void Reset(GPU& gpu) = 0;

    // This "Accelerated" flag currently communicates if the framebuffer should
    // be allocated differently and other little misc handlers. Ideally there
    // are more detailed "traits" that we can ask of the Renderer3D type
    const bool Accelerated;

    virtual void VCount144(GPU& gpu) {};
    virtual void Stop(const GPU& gpu) {}
    virtual void RenderFrame(GPU& gpu) = 0;
    virtual void RestartFrame(GPU& gpu) {};
    virtual u32* GetLine(int line) = 0;
    virtual void ScanlineSync(int line) {};
    virtual void Blit(const GPU& gpu) {};
    virtual void PrepareCaptureFrame() {}
protected:
    Renderer3D(bool Accelerated);
};

}

#endif
