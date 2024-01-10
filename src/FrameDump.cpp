/*
    Copyright 2016-2024 melonDS team

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

#include "Platform.h"
#include "frontend/qt_sdl/ROMManager.h"
#include "FrameDump.h"
#include "GPU3D.h"

namespace melonDS
{

FrameDump::FrameDump(melonDS::GPU& gpu) :
    GPU(gpu)
{
}

void FrameDump::FDWrite(u16 cmd, u32* param)
{
    // note: 0dotdisp is assigned an id of 0x256
    if (NumParams + (NumCmds*4) >= 500000) return; // should probably abort the framedump entirely

    Cmds.push_back(cmd);
    NumCmds++;

    if (cmd == 256)
    {
        Params.push_back(*param);
        NumParams++;
    }
    else
        for (int i = 0; i < CmdNumParams[cmd]; i++)
        {
            Params.push_back(param[i]);
            NumParams++;
        }
}

void FrameDump::StartFrameDump()
{
    GPU.QueueFrameDump = false;

    // save these values here because its way easier this way.
    ZDotDisp_Track = (GPU.GPU3D.ZeroDotWLimit != 0);
    ZDotDisp = ((GPU.GPU3D.ZeroDotWLimit == 0) ? 0 : ((GPU.GPU3D.ZeroDotWLimit - 0x1FF) / 0x200));
    PolyAttr = GPU.GPU3D.CurPolygonAttr;
    PolyAttrUnset = GPU.GPU3D.PolygonAttr;
    VtxColor = GPU.GPU3D.VertexColor[2] << 10 | GPU.GPU3D.VertexColor[1] << 5 | GPU.GPU3D.VertexColor[0];
    Viewport = ((191 - GPU.GPU3D.Viewport[3]) & 0xFF) << 24 | GPU.GPU3D.Viewport[2] << 16 | ((191 - GPU.GPU3D.Viewport[1]) & 0xFF) << 8 | GPU.GPU3D.Viewport[0];
    memcpy(ProjStack, GPU.GPU3D.ProjMatrixStack, sizeof(GPU.GPU3D.ProjMatrixStack));
    memcpy(PosStack, GPU.GPU3D.PosMatrixStack, sizeof(GPU.GPU3D.PosMatrixStack));
    memcpy(VecStack, GPU.GPU3D.VecMatrixStack, sizeof(GPU.GPU3D.VecMatrixStack));
    memcpy(TexStack, GPU.GPU3D.TexMatrixStack, sizeof(GPU.GPU3D.TexMatrixStack));
    memcpy(ProjMtx, GPU.GPU3D.ProjMatrix, sizeof(GPU.GPU3D.ProjMatrix));
    memcpy(PosMtx, GPU.GPU3D.PosMatrix, sizeof(GPU.GPU3D.PosMatrix));
    memcpy(VecMtx, GPU.GPU3D.VecMatrix, sizeof(GPU.GPU3D.VecMatrix));
    memcpy(TexMtx, GPU.GPU3D.TexMatrix, sizeof(GPU.GPU3D.TexMatrix));
    MatrixMode = GPU.GPU3D.MatrixMode;
    Polygon = GPU.GPU3D.PolygonMode;
    VtxXY = GPU.GPU3D.CurVertex[1] << 16 | GPU.GPU3D.CurVertex[0];
    VtxZ = GPU.GPU3D.CurVertex[2];
    TexCoord = GPU.GPU3D.TexCoords[1] << 16 | GPU.GPU3D.TexCoords[0]; // use final texcoords so i dont have to worry about setting up the matrix & params properly.
    TexParam = GPU.GPU3D.TexParam;
    TexPalette = GPU.GPU3D.TexPalette;
    DiffAmbi = GPU.GPU3D.MatAmbient[2] << 26 | GPU.GPU3D.MatAmbient[1] << 21 | GPU.GPU3D.MatAmbient[0] << 16
             | GPU.GPU3D.MatDiffuse[2] << 10 | GPU.GPU3D.MatDiffuse[1] << 5 | GPU.GPU3D.MatDiffuse[0];
    SpecEmis = GPU.GPU3D.MatEmission[2] << 26 | GPU.GPU3D.MatEmission[1] << 21 | GPU.GPU3D.MatEmission[0] << 16
             | GPU.GPU3D.MatSpecular[2] << 10 | GPU.GPU3D.MatSpecular[1] << 5 | GPU.GPU3D.MatSpecular[0];
    for (int i = 0; i < 32; i++)
    {
        u8 tbl = i << 2;
        Shininess[i] = GPU.GPU3D.ShininessTable[tbl];
        Shininess[i] |= GPU.GPU3D.ShininessTable[tbl + 1] << 8;
        Shininess[i] |= GPU.GPU3D.ShininessTable[tbl + 2] << 16;
        Shininess[i] |= GPU.GPU3D.ShininessTable[tbl + 3] << 24;
    }
    for (u32 i = 0; i < 4; i++)
    {
        LightVec[i] = i << 30 | ((u32)GPU.GPU3D.LightDirection[i][2] & 0x3FF) << 20 | ((u32)GPU.GPU3D.LightDirection[i][1] & 0x3FF) << 10 | ((u32)GPU.GPU3D.LightDirection[i][0] & 0x3FF);
        LightColor[i] = i << 30 | GPU.GPU3D.LightColor[i][2] << 10 | GPU.GPU3D.LightColor[i][1] << 5 | GPU.GPU3D.LightColor[i][0];
    }
    SwapBuffer = GPU.GPU3D.FlushAttributes;
    // set writes to zero to "clear" the write list
    NumCmds = 0;
    NumParams = 0;
}

void FrameDump::FinFrameDump()
{
    // save final latched values
    // this step is technically completely unnecessary, does it just get optimized out?
    Disp3DCnt = GPU.GPU3D.RenderDispCnt;
    for (int i = 0; i < 8; i++)
    {
        EdgeColor[i] = GPU.GPU3D.RenderEdgeTable[i];
    }
    AlphaTest = GPU.GPU3D.RenderAlphaRef;
    ClearColor = GPU.GPU3D.RenderClearAttr1;
    ClearDepOff = GPU.GPU3D.RenderClearAttr2;
    FogColor = GPU.GPU3D.RenderFogColor;
    FogOffset = GPU.GPU3D.RenderFogOffset;
    for (int i = 0; i < 32; i++)
    {
        FogTable[i] = GPU.GPU3D.RenderFogDensityTable[i+1];
        ToonTable[i] = GPU.GPU3D.RenderToonTable[i];
    }

    std::string filename = ROMManager::GetFrameDumpName();
    Platform::FileHandle* file = Platform::OpenLocalFile(filename, Platform::FileMode::Write);
    
    // save final state vars to file
    Platform::FileWrite(&Disp3DCnt, 1, sizeof(Disp3DCnt), file);
    Platform::FileWrite(EdgeColor, 1, sizeof(EdgeColor), file);
    Platform::FileWrite(&AlphaTest, 1, sizeof(AlphaTest), file);
    Platform::FileWrite(&ClearColor, 1, sizeof(ClearColor), file);
    Platform::FileWrite(&ClearDepOff, 1, sizeof(ClearDepOff), file);
    Platform::FileWrite(&FogColor, 1, sizeof(FogColor), file);
    Platform::FileWrite(&FogOffset, 1, sizeof(FogOffset), file);
    Platform::FileWrite(FogTable, 1, sizeof(FogTable), file);
    Platform::FileWrite(ToonTable, 1, sizeof(ToonTable), file);

    // save initial state vars to file.
    Platform::FileWrite(&ZDotDisp_Track, 1, 1, file);
    Platform::FileWrite(&ZDotDisp, 1, sizeof(ZDotDisp), file);
    Platform::FileWrite(&PolyAttr, 1, sizeof(PolyAttr), file);
    Platform::FileWrite(&PolyAttrUnset, 1, sizeof(PolyAttrUnset), file);
    Platform::FileWrite(&VtxColor, 1, sizeof(VtxColor), file);
    Platform::FileWrite(&Viewport, 1, sizeof(Viewport), file);
    Platform::FileWrite(ProjStack, 1, sizeof(ProjStack), file);
    Platform::FileWrite(PosStack, 1, sizeof(PosStack), file);
    Platform::FileWrite(VecStack, 1, sizeof(VecStack), file);
    Platform::FileWrite(TexStack, 1, sizeof(TexStack), file);
    Platform::FileWrite(ProjMtx, 1, sizeof(ProjMtx), file);
    Platform::FileWrite(PosMtx, 1, sizeof(PosMtx), file);
    Platform::FileWrite(VecMtx, 1, sizeof(VecMtx), file);
    Platform::FileWrite(TexMtx, 1, sizeof(TexMtx), file);
    Platform::FileWrite(&MatrixMode, 1, sizeof(MatrixMode), file);
    Platform::FileWrite(&Polygon, 1, sizeof(Polygon), file);
    Platform::FileWrite(&VtxXY, 1, sizeof(VtxXY), file);
    Platform::FileWrite(&VtxZ, 1, sizeof(VtxZ), file);
    Platform::FileWrite(&TexCoord, 1, sizeof(TexCoord), file);
    Platform::FileWrite(&TexParam, 1, sizeof(TexParam), file);
    Platform::FileWrite(&TexPalette, 1, sizeof(TexPalette), file);
    Platform::FileWrite(&DiffAmbi, 1, sizeof(DiffAmbi), file);
    Platform::FileWrite(&SpecEmis, 1, sizeof(SpecEmis), file);
    Platform::FileWrite(Shininess, 1, sizeof(Shininess), file);
    Platform::FileWrite(LightVec, 1, sizeof(LightVec), file);
    Platform::FileWrite(LightColor, 1, sizeof(LightColor), file);
    Platform::FileWrite(&SwapBuffer, 1, sizeof(SwapBuffer), file);
    
    // skip banks H and I, we only care about dumping 3d engine state currently, and banks H and I cannot be used by the 3d engine
    // save vram control regs to file
    Platform::FileWrite(GPU.VRAMCNT, 1, sizeof(GPU.VRAMCNT[0])*7, file);

    // only save vram if the bank is enabled and allocated to be used as texture image/texture palette.
    if ((GPU.VRAMCNT[0] & 0x83) == 0x83) Platform::FileWrite(GPU.VRAM_A, 1, sizeof(GPU.VRAM_A), file);
    if ((GPU.VRAMCNT[1] & 0x83) == 0x83) Platform::FileWrite(GPU.VRAM_B, 1, sizeof(GPU.VRAM_B), file);
    if ((GPU.VRAMCNT[2] & 0x87) == 0x83) Platform::FileWrite(GPU.VRAM_C, 1, sizeof(GPU.VRAM_C), file);
    if ((GPU.VRAMCNT[3] & 0x87) == 0x83) Platform::FileWrite(GPU.VRAM_D, 1, sizeof(GPU.VRAM_D), file);
    if ((GPU.VRAMCNT[4] & 0x87) == 0x83) Platform::FileWrite(GPU.VRAM_E, 1, sizeof(GPU.VRAM_E), file);
    if ((GPU.VRAMCNT[5] & 0x87) == 0x83) Platform::FileWrite(GPU.VRAM_F, 1, sizeof(GPU.VRAM_F), file);
    if ((GPU.VRAMCNT[6] & 0x87) == 0x83) Platform::FileWrite(GPU.VRAM_G, 1, sizeof(GPU.VRAM_G), file);
    
    u8* tempcmds = (u8*)malloc(NumCmds * sizeof(Cmds[0]));
    u32* tempparams = (u32*)malloc(NumParams * sizeof(Params[0]));

    u32 numcmd2 = 0;
    u32 numparam2 = 0;

    for (int i = 0, j = 0; i < NumCmds; i++)
    {
        // filter out the bulk of commands that're useless to framedumps
        u16 command = Cmds[i];
        if (command == 256)
        {
            tempcmds[numcmd2++] = 255;
            tempparams[numparam2++] = Params[j++];
        }
        else
        {
            u8 paramcount = CmdNumParams[command];
            if (paramcount == 0)
            {
                if (command == 0x15 || command == 0x11) // filter out all nops (all paramless commands except for: mtx push, and mtx identity)
                    tempcmds[numcmd2++] = command;
            }
            else 
            {
                if (command == 0x70 || command == 0x72) // filter out box and vector tests
                {
                    j += paramcount;
                }
                else
                {
                    tempcmds[numcmd2++] = command;
                    memcpy(&tempparams[numparam2], &Params[j], paramcount*4);
                    j += paramcount;
                    numparam2 += paramcount;
                }
            }
        }
    }
    Platform::FileWrite(&numcmd2, 1, sizeof(numcmd2), file);
    Platform::FileWrite(&numparam2, 1, sizeof(numparam2), file);
    Platform::FileWrite(tempcmds, 1, numcmd2, file);
    Platform::FileWrite(tempparams, sizeof(Params[0]), numparam2, file);
    free(tempcmds);
    free(tempparams);
    Platform::CloseFile(file);
}


}
