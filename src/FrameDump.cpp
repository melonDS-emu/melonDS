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
#include "FrameDump.h"
#include "GPU3D.h"

namespace melonDS
{

FrameDump::FrameDump(melonDS::GPU& gpu) :
    GPU(gpu)
{
}

std::string FrameDump::FinishFileName()
{
    std::string ext = GPU.FDSavePNG ? ".ndsfd.png" : ".ndsfd";
    std::string base = GPU.FDFileBase + '.';
    int num = 0;
    std::string full = base + std::to_string(num) + ext;
    while (Platform::LocalFileExists(full) && num < 1000)
    {
        num++;
        full = base + std::to_string(num) + ext;
    }
    return full;
}

bool FrameDump::FDWrite(u16 cmd, u32* param)
{
    // don't write more cmds to buffer if the frame is already done.
    if (GPU.FDBeginPNG) return true;

    // abort frame dump if it writes too many commands & params to the buffer
    if (Cmds.size() + (Params.size()*4) >= MaxDataSize) [[unlikely]]
    {
        // add osd error message here
        GPU.QueueFrameDump = false;
        return false;
    }

    Cmds.push_back(cmd);
    
    // note: 0dotdisp is assigned an id of 0x256
    if (cmd == 256)
        Params.push_back(*param);
    else
        for (int i = 0; i < CmdNumParams[cmd]; i++)
            Params.push_back(param[i]);

    return true;
}

void FrameDump::FDBufferWrite(void* input, int bytes, std::vector<u8>& buffer)
{
    // store data to a buffer instead of directly to a file to allow for compression to be applied
    // todo: better way of doing this?
    for (int i = 0; i < bytes; i++)
        buffer.push_back(((u8*)input)[i]);
}

void FrameDump::StartFrameDump()
{
    // save these values here because its way easier this way.
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

    // they should already be empty, but just in case.
    Cmds.clear();
    Params.clear();
}

void FrameDump::FinFrameDump()
{    
    Platform::FileHandle* file = Platform::OpenFile(FinishFileName(), Platform::FileMode::Write);

    // save png if needed
    std::vector<u8> png;
    if (GPU.FDSavePNG)
    {
        // offload the effort of making the png to the frontend (probably should be done by the core instead?)
        png = Platform::MakePNGFrameDump(TempFrameBuffer, 256, 192);
        // write the bare minimum to make the png a png
        Platform::FileWrite(&png[0], 1, 33, file);
        
        // make our own png chunk to hold framedump data :D
        u8 temp[] = {
            0, 0, 0, 0, // placeholder for length field (will be overwritten later)
            'n', 'd', 'S', 'f', // Chunk name
        };
        Platform::FileWrite(temp, 1, sizeof(temp), file);
    }
    std::vector<u8> buffer;

    // save final state vars to file
    FDBufferWrite(&GPU.GPU3D.RenderDispCnt, 2, buffer); // only write two bytes, (only 16 bits are used, and thus only those are stored for frame dumps)
    FDBufferWrite(GPU.GPU3D.RenderEdgeTable, sizeof(GPU.GPU3D.RenderEdgeTable), buffer);
    FDBufferWrite(&GPU.GPU3D.RenderAlphaRef, sizeof(GPU.GPU3D.RenderAlphaRef), buffer);
    FDBufferWrite(&GPU.GPU3D.RenderClearAttr1, sizeof(GPU.GPU3D.RenderClearAttr1), buffer);
    FDBufferWrite(&GPU.GPU3D.RenderClearAttr2, sizeof(GPU.GPU3D.RenderClearAttr2), buffer);
    FDBufferWrite(&GPU.GPU3D.RenderFogColor, sizeof(GPU.GPU3D.RenderFogColor), buffer);
    u16 fogoffset = GPU.GPU3D.RenderFogOffset >> 9; // correct fog offset value, only write 2 bytes.
    FDBufferWrite(&fogoffset, sizeof(fogoffset), buffer);
    // only write the 32 "real" entries of the density table (entries 0 and 33 are dupes and not part of the actual register)
    FDBufferWrite(&GPU.GPU3D.RenderFogDensityTable[1], sizeof(GPU.GPU3D.RenderFogDensityTable[0])*32, buffer);
    FDBufferWrite(GPU.GPU3D.RenderToonTable, sizeof(GPU.GPU3D.RenderToonTable), buffer);

    // save initial state vars to file.
    FDBufferWrite(&ZDotDisp, sizeof(ZDotDisp), buffer);
    FDBufferWrite(&PolyAttr, sizeof(PolyAttr), buffer);
    FDBufferWrite(&PolyAttrUnset, sizeof(PolyAttrUnset), buffer);
    FDBufferWrite(&VtxColor, sizeof(VtxColor), buffer);
    FDBufferWrite(&Viewport, sizeof(Viewport), buffer);
    FDBufferWrite(ProjStack, sizeof(ProjStack), buffer);
    FDBufferWrite(PosStack, sizeof(PosStack), buffer);
    FDBufferWrite(VecStack, sizeof(VecStack), buffer);
    FDBufferWrite(TexStack, sizeof(TexStack), buffer);
    FDBufferWrite(ProjMtx, sizeof(ProjMtx), buffer);
    FDBufferWrite(PosMtx, sizeof(PosMtx), buffer);
    FDBufferWrite(VecMtx, sizeof(VecMtx), buffer);
    FDBufferWrite(TexMtx, sizeof(TexMtx), buffer);
    FDBufferWrite(&MatrixMode, sizeof(MatrixMode), buffer);
    FDBufferWrite(&Polygon, sizeof(Polygon), buffer);
    FDBufferWrite(&VtxXY, sizeof(VtxXY), buffer);
    FDBufferWrite(&VtxZ, sizeof(VtxZ), buffer);
    FDBufferWrite(&TexCoord, sizeof(TexCoord), buffer);
    FDBufferWrite(&TexParam, sizeof(TexParam), buffer);
    FDBufferWrite(&TexPalette, sizeof(TexPalette), buffer);
    FDBufferWrite(&DiffAmbi, sizeof(DiffAmbi), buffer);
    FDBufferWrite(&SpecEmis, sizeof(SpecEmis), buffer);
    FDBufferWrite(Shininess, sizeof(Shininess), buffer);
    FDBufferWrite(LightVec, sizeof(LightVec), buffer);
    FDBufferWrite(LightColor, sizeof(LightColor), buffer);
    FDBufferWrite(&SwapBuffer, sizeof(SwapBuffer), buffer);
    
    // skip banks H and I, we only care about dumping 3d engine state currently, and banks H and I cannot be used by the 3d engine
    // save vram control regs to file
    FDBufferWrite(GPU.VRAMCNT, sizeof(GPU.VRAMCNT[0])*7, buffer);

    // only save vram if the bank is enabled and allocated to be used as texture image/texture palette.
    if ((GPU.VRAMCNT[0] & 0x83) == 0x83) FDBufferWrite(GPU.VRAM_A, sizeof(GPU.VRAM_A), buffer);
    if ((GPU.VRAMCNT[1] & 0x83) == 0x83) FDBufferWrite(GPU.VRAM_B, sizeof(GPU.VRAM_B), buffer);
    if ((GPU.VRAMCNT[2] & 0x87) == 0x83) FDBufferWrite(GPU.VRAM_C, sizeof(GPU.VRAM_C), buffer);
    if ((GPU.VRAMCNT[3] & 0x87) == 0x83) FDBufferWrite(GPU.VRAM_D, sizeof(GPU.VRAM_D), buffer);
    if ((GPU.VRAMCNT[4] & 0x87) == 0x83) FDBufferWrite(GPU.VRAM_E, sizeof(GPU.VRAM_E), buffer);
    if ((GPU.VRAMCNT[5] & 0x87) == 0x83) FDBufferWrite(GPU.VRAM_F, sizeof(GPU.VRAM_F), buffer);
    if ((GPU.VRAMCNT[6] & 0x87) == 0x83) FDBufferWrite(GPU.VRAM_G, sizeof(GPU.VRAM_G), buffer);
    
    u8* tempcmds = (u8*)malloc(Cmds.size() * sizeof(u8));
    u32* tempparams = (u32*)malloc(Params.size() * sizeof(Params[0]));

    u32 numcmd = 0;
    u32 numparam = 0;

    for (int i = 0, j = 0; i < Cmds.size(); i++)
    {
        // filter out the bulk of commands that're useless to framedumps
        u16 command = Cmds[i];
        if (command == 256)
        {
            tempcmds[numcmd++] = 255;
            tempparams[numparam++] = Params[j++];
        }
        else
        {
            u8 paramcount = CmdNumParams[command];
            if (paramcount == 0)
            {
                if (command == 0x15 || command == 0x11) // filter out all nops (all paramless commands except for: mtx push, and mtx identity)
                    tempcmds[numcmd++] = command;
            }
            else 
            {
                if (command == 0x70 || command == 0x72) // filter out box and vector tests
                {
                    j += paramcount;
                }
                else
                {
                    tempcmds[numcmd++] = command;
                    memcpy(&tempparams[numparam], &Params[j], paramcount*4);
                    j += paramcount;
                    numparam += paramcount;
                }
            }
        }
    }
    FDBufferWrite(&numcmd, sizeof(numcmd), buffer);
    FDBufferWrite(&numparam, sizeof(numparam), buffer);
    FDBufferWrite(tempcmds, numcmd, buffer);
    FDBufferWrite(tempparams, (sizeof(Params[0]) * numparam), buffer);
    free(tempcmds);
    free(tempparams);
    
    // todo: implement compression algorithm?

    // write header
    constexpr u8 headertitle[5] = {'N', 'T', 'R', 'F', 'D'};
    Platform::FileWrite(&headertitle, 1, sizeof(headertitle), file);
    constexpr u16 fdversion = 0;
    Platform::FileWrite(&fdversion, 1, sizeof(fdversion), file);
    u8 comprtype = 0;
    Platform::FileWrite(&comprtype, 1, sizeof(comprtype), file);
    // write the actual file
    Platform::FileWrite(&buffer[0], 1, buffer.size(), file);

    if (GPU.FDSavePNG)
    {
        u32 seekptr1 = 33; // how much of the png was initially written (offset of the framedump chunk's length field)
        u32 seekptr2 = 41; // the offset where the frame dump chunk began
        u32 seekptr3 = Platform::FileTell(file); // offset of the end of the framedump chunk

        // we dont *actually* need to calculate a checksum (citation needed?)
        // and i dont feel like implementing the calc for one. so we just fill the field with 0s. :)
        u8 temp[] = {0, 0, 0, 0};
        Platform::FileWrite(temp, 1, sizeof(temp), file);

        // calculate the length of the framedump chunk
        u32 byteswritten = seekptr3 - seekptr2;

        // write the rest of the png
        Platform::FileWrite(&png[seekptr1], 1, png.size() - (33 * sizeof(png[0])), file);

        // seek back to the framedump's length field was
        Platform::FileSeek(file, seekptr1, Platform::FileSeekOrigin::Start);
        // write length field (needs to be written in reverse endian order (big endian))
        Platform::FileWrite(&((u8*)&byteswritten)[3], 1, 1, file);
        Platform::FileWrite(&((u8*)&byteswritten)[2], 1, 1, file);
        Platform::FileWrite(&((u8*)&byteswritten)[1], 1, 1, file);
        Platform::FileWrite(&((u8*)&byteswritten)[0], 1, 1, file);
    }

    Platform::CloseFile(file);
    GPU.QueueFrameDump = false;
    GPU.FDBeginPNG = false;

    // add osd success message here
}

}
