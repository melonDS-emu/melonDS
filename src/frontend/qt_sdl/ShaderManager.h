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

#ifndef SHADERMANAGER_H
#define SHADERMANAGER_H

#include "SlangPreset.h"
#include "glad/glad.h"

#include <string>
#include <vector>
#include <map>
#include <mutex>

struct ShaderFBO {
    GLuint fbo = 0;
    GLuint texture = 0;
    int width = 0;
    int height = 0;
};

struct UniformLayout {
    GLuint blockIndex = GL_INVALID_INDEX;
    GLint blockSize = 0;
    std::map<std::string, size_t> offsets;
};

struct ProgramLayout {
    UniformLayout globalUBO;
    UniformLayout pushUBO;
    std::map<std::string, GLint> uniformLocations;
};

class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    // Loads a preset immediately (called from Process() if a pending load was requested)
    bool LoadPreset(const SlangPreset& preset);
    // Queues a preset load to be applied on the next Process() call (thread-safe)
    void RequestPresetLoad(const SlangPreset& preset);
    
    // Processes inputTexArray through all shader passes, returns filtered 2D array texture
    GLuint Process(GLuint inputTexArray, int inputWidth, int inputHeight, int windowWidth, int windowHeight, int& outWidth, int& outHeight);

    void SetParameter(const std::string& name, float value);

private:
    SlangPreset m_preset;
    
    bool m_presetPending = false;
    SlangPreset m_pendingPreset;
    std::mutex m_pendingMutex;
    
    ShaderFBO m_fboA;
    ShaderFBO m_fboB;
    GLuint m_extractFBO = 0;
    
    GLuint m_outputArrayFBO = 0;
    GLuint m_outputArrayTex = 0;
    int m_outputArrayWidth = 0;
    int m_outputArrayHeight = 0;
    
    std::vector<GLuint> m_shaderPrograms;
    std::vector<ProgramLayout> m_programLayouts;
    std::map<std::string, float> m_parameters;
    std::map<std::string, GLuint> m_lutTextures;
    std::vector<ShaderFBO> m_passFBOs;
    uint32_t m_frameCount = 0;

    GLuint m_globalUBO = 0;
    GLuint m_pushConstantsUBO = 0;

    GLuint m_quadVAO = 0;
    GLuint m_quadVBO = 0;

    void InitQuad();
    void CleanupFBOs();
    void ResizeFBO(ShaderFBO& fbo, int width, int height, bool linearFilter, bool srgb = false, bool floatFbo = false);
    void ResizeOutputArray(int width, int height);
    
    // Compiles a .slang shader to GLSL via glslang->SPIRV->spirv-cross pipeline
    bool CompileShaderPass(const std::string& slangPath, GLuint& outProgram);
};

#endif // SHADERMANAGER_H
