/*
    Copyright 2016-2021 Arisotura

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

#include "OpenGLSupport.h"

namespace GPU
{

struct RenderSettings;

class GLCompositor
{
public:
    GLCompositor() = default;
    GLCompositor(const GLCompositor&) = delete;
    GLCompositor& operator=(const GLCompositor&) = delete;

    bool Init();
    void DeInit();
    void Reset();

    void SetRenderSettings(RenderSettings& settings);

    void Stop();
    void RenderFrame();
    void BindOutputTexture(int buf);
private:

    int Scale;
    int ScreenH, ScreenW;

    GLuint CompShader[1][3];
    GLuint CompScaleLoc[1];
    GLuint Comp3DXPosLoc[1];

    GLuint CompVertexBufferID;
    GLuint CompVertexArrayID;

    struct CompVertex
    {
        float Position[2];
        float Texcoord[2];
    };
    CompVertex CompVertices[2 * 3*2];

    GLuint CompScreenInputTex;
    GLuint CompScreenOutputTex[2];
    GLuint CompScreenOutputFB[2];
};

}