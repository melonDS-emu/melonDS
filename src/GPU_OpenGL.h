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

#pragma once

#include "OpenGLSupport.h"

#include <array>
#include <memory>

namespace GPU
{

struct RenderSettings;

class GLCompositor
{
public:
    static std::unique_ptr<GLCompositor> New() noexcept;
    GLCompositor(const GLCompositor&) = delete;
    GLCompositor& operator=(const GLCompositor&) = delete;
    ~GLCompositor();

    void Reset();

    void SetRenderSettings(RenderSettings& settings);

    void Stop();
    void RenderFrame();
    void BindOutputTexture(int buf);
private:
    GLCompositor(std::array<GLuint, 3> CompShader) noexcept;

    int Scale;
    int ScreenH, ScreenW;

    std::array<GLuint, 3> CompShader;
    GLuint CompScaleLoc;
    GLuint Comp3DXPosLoc;

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