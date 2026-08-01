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

#include "ShaderManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <QImage>

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv_cross/spirv_glsl.hpp>

class SlangIncluder : public glslang::TShader::Includer {
public:
    SlangIncluder(const std::string& baseDir) : m_baseDir(baseDir) {}

    IncludeResult* includeSystem(const char* headerName, const char* includerName, size_t inclusionDepth) override {
        return nullptr;
    }

    IncludeResult* includeLocal(const char* headerName, const char* includerName, size_t inclusionDepth) override {
        std::string fullPath = m_baseDir + "/" + headerName;
        std::ifstream file(fullPath);
        if (!file.is_open()) return nullptr;

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        char* allocatedContent = new char[content.length() + 1];
        std::strcpy(allocatedContent, content.c_str());

        return new IncludeResult(fullPath, allocatedContent, content.length(), allocatedContent);
    }

    void releaseInclude(IncludeResult* result) override {
        if (result && result->userData) {
            delete[] static_cast<char*>(result->userData);
        }
        delete result;
    }

private:
    std::string m_baseDir;
};

void CheckGLError(const char* label) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Error at " << label << ": " << err << std::endl;
    }
}

void InitResources(TBuiltInResource& Resources) {
    Resources.maxLights = 32;
    Resources.maxClipPlanes = 6;
    Resources.maxTextureUnits = 32;
    Resources.maxTextureCoords = 32;
    Resources.maxVertexAttribs = 64;
    Resources.maxVertexUniformComponents = 4096;
    Resources.maxVaryingFloats = 64;
    Resources.maxVertexTextureImageUnits = 32;
    Resources.maxCombinedTextureImageUnits = 80;
    Resources.maxTextureImageUnits = 32;
    Resources.maxFragmentUniformComponents = 4096;
    Resources.maxDrawBuffers = 32;
    Resources.maxVertexUniformVectors = 128;
    Resources.maxVaryingVectors = 8;
    Resources.maxFragmentUniformVectors = 16;
    Resources.maxVertexOutputVectors = 16;
    Resources.maxFragmentInputVectors = 15;
    Resources.minProgramTexelOffset = -8;
    Resources.maxProgramTexelOffset = 7;
    Resources.maxClipDistances = 8;
    Resources.maxComputeWorkGroupCountX = 65535;
    Resources.maxComputeWorkGroupCountY = 65535;
    Resources.maxComputeWorkGroupCountZ = 65535;
    Resources.maxComputeWorkGroupSizeX = 1024;
    Resources.maxComputeWorkGroupSizeY = 1024;
    Resources.maxComputeWorkGroupSizeZ = 64;
    Resources.maxComputeUniformComponents = 1024;
    Resources.maxComputeTextureImageUnits = 16;
    Resources.maxComputeImageUniforms = 8;
    Resources.maxComputeAtomicCounters = 8;
    Resources.maxComputeAtomicCounterBuffers = 1;
    Resources.maxVaryingComponents = 60;
    Resources.maxVertexOutputComponents = 64;
    Resources.maxGeometryInputComponents = 64;
    Resources.maxGeometryOutputComponents = 128;
    Resources.maxFragmentInputComponents = 128;
    Resources.maxImageUnits = 8;
    Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
    Resources.maxCombinedShaderOutputResources = 8;
    Resources.maxImageSamples = 0;
    Resources.maxVertexImageUniforms = 0;
    Resources.maxTessControlImageUniforms = 0;
    Resources.maxTessEvaluationImageUniforms = 0;
    Resources.maxGeometryImageUniforms = 0;
    Resources.maxFragmentImageUniforms = 8;
    Resources.maxCombinedImageUniforms = 8;
    Resources.maxGeometryTextureImageUnits = 16;
    Resources.maxGeometryOutputVertices = 256;
    Resources.maxGeometryTotalOutputComponents = 1024;
    Resources.maxGeometryUniformComponents = 1024;
    Resources.maxGeometryVaryingComponents = 64;
    Resources.maxTessControlInputComponents = 128;
    Resources.maxTessControlOutputComponents = 128;
    Resources.maxTessControlTextureImageUnits = 16;
    Resources.maxTessControlUniformComponents = 1024;
    Resources.maxTessControlTotalOutputComponents = 4096;
    Resources.maxTessEvaluationInputComponents = 128;
    Resources.maxTessEvaluationOutputComponents = 128;
    Resources.maxTessEvaluationTextureImageUnits = 16;
    Resources.maxTessEvaluationUniformComponents = 1024;
    Resources.maxTessPatchComponents = 120;
    Resources.maxPatchVertices = 32;
    Resources.maxTessGenLevel = 64;
    Resources.maxViewports = 16;
    Resources.maxVertexAtomicCounters = 0;
    Resources.maxTessControlAtomicCounters = 0;
    Resources.maxTessEvaluationAtomicCounters = 0;
    Resources.maxGeometryAtomicCounters = 0;
    Resources.maxFragmentAtomicCounters = 8;
    Resources.maxCombinedAtomicCounters = 8;
    Resources.maxAtomicCounterBindings = 1;
    Resources.maxVertexAtomicCounterBuffers = 0;
    Resources.maxTessControlAtomicCounterBuffers = 0;
    Resources.maxTessEvaluationAtomicCounterBuffers = 0;
    Resources.maxGeometryAtomicCounterBuffers = 0;
    Resources.maxFragmentAtomicCounterBuffers = 1;
    Resources.maxCombinedAtomicCounterBuffers = 1;
    Resources.maxAtomicCounterBufferSize = 16384;
    Resources.maxTransformFeedbackBuffers = 4;
    Resources.maxTransformFeedbackInterleavedComponents = 64;
    Resources.maxCullDistances = 8;
    Resources.maxCombinedClipAndCullDistances = 8;
    Resources.maxSamples = 4;
    Resources.maxMeshOutputVerticesNV = 256;
    Resources.maxMeshOutputPrimitivesNV = 512;
    Resources.maxMeshWorkGroupSizeX_NV = 32;
    Resources.maxMeshWorkGroupSizeY_NV = 1;
    Resources.maxMeshWorkGroupSizeZ_NV = 1;
    Resources.maxTaskWorkGroupSizeX_NV = 32;
    Resources.maxTaskWorkGroupSizeY_NV = 1;
    Resources.maxTaskWorkGroupSizeZ_NV = 1;
    Resources.maxMeshViewCountNV = 4;

    Resources.limits.nonInductiveForLoops = 1;
    Resources.limits.whileLoops = 1;
    Resources.limits.doWhileLoops = 1;
    Resources.limits.generalUniformIndexing = 1;
    Resources.limits.generalAttributeMatrixVectorIndexing = 1;
    Resources.limits.generalVaryingIndexing = 1;
    Resources.limits.generalSamplerIndexing = 1;
    Resources.limits.generalVariableIndexing = 1;
    Resources.limits.generalConstantMatrixVectorIndexing = 1;
}

ShaderManager::ShaderManager() {
    static bool glslangInitialized = false;
    if (!glslangInitialized) {
        glslang::InitializeProcess();
        glslangInitialized = true;
    }
    InitQuad();
    glGenBuffers(1, &m_globalUBO);
    glGenBuffers(1, &m_pushConstantsUBO);
}

ShaderManager::~ShaderManager() {
    CleanupFBOs();

    for (GLuint prog : m_shaderPrograms) {
        if (prog) glDeleteProgram(prog);
    }

    if (m_globalUBO) glDeleteBuffers(1, &m_globalUBO);
    if (m_pushConstantsUBO) glDeleteBuffers(1, &m_pushConstantsUBO);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
}

void ShaderManager::InitQuad() {
    // Full-screen quad used to render shader pass outputs
    float vertices[] = {
        // pos        // tex
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);

    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

void ShaderManager::CleanupFBOs() {
    if (m_fboA.fbo) {
        glDeleteFramebuffers(1, &m_fboA.fbo);
        glDeleteTextures(1, &m_fboA.texture);
    }
    if (m_fboB.fbo) {
        glDeleteFramebuffers(1, &m_fboB.fbo);
        glDeleteTextures(1, &m_fboB.texture);
    }
    for (auto& fbo : m_passFBOs) {
        if (fbo.fbo) {
            glDeleteFramebuffers(1, &fbo.fbo);
            glDeleteTextures(1, &fbo.texture);
        }
    }
    m_passFBOs.clear();

    for (auto& pair : m_lutTextures) {
        glDeleteTextures(1, &pair.second);
    }
    m_lutTextures.clear();

    if (m_extractFBO) glDeleteFramebuffers(1, &m_extractFBO);
    if (m_outputArrayFBO) glDeleteFramebuffers(1, &m_outputArrayFBO);
    if (m_outputArrayTex) glDeleteTextures(1, &m_outputArrayTex);

    m_fboA = ShaderFBO{};
    m_fboB = ShaderFBO{}; // Reset FBO trackers after deleting
    m_extractFBO = 0;
    m_outputArrayFBO = 0;
    m_outputArrayTex = 0;
}
void ShaderManager::ResizeOutputArray(int width, int height) {
    if (m_outputArrayWidth == width && m_outputArrayHeight == height && m_outputArrayTex != 0) {
        return; // Already the correct size
    }

    if (m_outputArrayTex == 0) {
        glGenTextures(1, &m_outputArrayTex);
        glGenFramebuffers(1, &m_outputArrayFBO);
    }

    m_outputArrayWidth = width;
    m_outputArrayHeight = height;

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_outputArrayTex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, width, height, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void ShaderManager::ResizeFBO(ShaderFBO& fbo, int width, int height, bool linearFilter, bool srgb, bool floatFbo) {
    if (fbo.width == width && fbo.height == height && fbo.fbo != 0) {
        return; // Already the correct size
    }

    if (fbo.fbo == 0) {
        glGenFramebuffers(1, &fbo.fbo);
        glGenTextures(1, &fbo.texture);
    }

    fbo.width = width;
    fbo.height = height;

    glBindTexture(GL_TEXTURE_2D, fbo.texture);
    GLint internalFormat = GL_RGBA8;
    GLenum type = GL_UNSIGNED_BYTE;

    if (floatFbo) {
        internalFormat = GL_RGBA32F;
        type = GL_FLOAT;
    } else if (srgb) {
        internalFormat = GL_SRGB8_ALPHA8;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, type, nullptr);

    GLint filter = linearFilter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo.texture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool ShaderManager::CompileShaderPass(const std::string& slangPath, GLuint& outProgram) {
    std::ifstream file(slangPath);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader: " << slangPath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    size_t lastSlash = slangPath.find_last_of("/\\");
    std::string baseDir = (lastSlash == std::string::npos) ? "." : slangPath.substr(0, lastSlash);

    auto expandIncludes = [](const std::string& src, const std::string& dir, auto& expandRef) -> std::string {
        std::stringstream in(src);
        std::stringstream out;
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("#include") != std::string::npos) {
                size_t firstQuote = line.find('"');
                size_t lastQuote = line.find('"', firstQuote + 1);
                if (firstQuote != std::string::npos && lastQuote != std::string::npos) {
                    std::string incFile = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                    std::string incPath = dir + "/" + incFile;
                    std::ifstream inc(incPath);
                    if (inc.is_open()) {
                        std::stringstream incBuf;
                        incBuf << inc.rdbuf();
                        size_t incSlash = incPath.find_last_of("/\\");
                        std::string incDir = (incSlash == std::string::npos) ? "." : incPath.substr(0, incSlash);
                        out << expandRef(incBuf.str(), incDir, expandRef) << "\n";
                        continue;
                    }
                }
            }
            out << line << "\n";
        }
        return out.str();
    };

    source = expandIncludes(source, baseDir, expandIncludes);
    
    size_t versionPos = source.find("#version");
    if (versionPos != std::string::npos) {
        size_t endOfLine = source.find('\n', versionPos);
        if (endOfLine != std::string::npos) {
            source.insert(endOfLine + 1, "#extension GL_GOOGLE_include_directive : enable\n");
        } else {
            source.append("\n#extension GL_GOOGLE_include_directive : enable\n");
        }
    } else {
        source.insert(0, "#extension GL_GOOGLE_include_directive : enable\n");
    }

    const std::string P_VERT = "#pragma stage vertex";
    const std::string P_FRAG = "#pragma stage fragment";

    size_t vert_pragma_pos = source.find(P_VERT);
    size_t frag_pragma_pos = source.find(P_FRAG);

    if (vert_pragma_pos == std::string::npos) {
        std::cerr << "Shader " << slangPath << " is missing #pragma stage vertex" << std::endl;
        return false;
    }
    if (frag_pragma_pos == std::string::npos) {
        std::cerr << "Shader " << slangPath << " is missing #pragma stage fragment" << std::endl;
        return false;
    }

    std::string common_source;
    std::string vert_specific;
    std::string frag_specific;

    if (vert_pragma_pos < frag_pragma_pos) {
        common_source = source.substr(0, vert_pragma_pos);
        vert_specific = source.substr(vert_pragma_pos, frag_pragma_pos - vert_pragma_pos);
        frag_specific = source.substr(frag_pragma_pos);
    } else {
        common_source = source.substr(0, frag_pragma_pos);
        frag_specific = source.substr(frag_pragma_pos, vert_pragma_pos - frag_pragma_pos);
        vert_specific = source.substr(vert_pragma_pos);
    }

    std::string final_vert_source = common_source + vert_specific;
    std::string final_frag_source = common_source + frag_specific;

    auto compileToSPIRV = [&](EShLanguage stage, const std::string& code, std::vector<unsigned int>& spirv) -> bool {
        glslang::TShader shader(stage);

        const char* codeStrs[1] = { code.c_str() };
        shader.setStrings(codeStrs, 1);

        shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 450);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

        TBuiltInResource resources;
        InitResources(resources);

        size_t lastSlash = slangPath.find_last_of("/\\");
        std::string baseDir = (lastSlash == std::string::npos) ? "." : slangPath.substr(0, lastSlash);
        SlangIncluder includer(baseDir);

        if (!shader.parse(&resources, 450, false, (EShMessages)(EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules), includer)) {
            std::cerr << "Glslang parsing failed for " << slangPath << " (" << (stage == EShLangVertex ? "Vertex" : "Fragment") << "):\n" << shader.getInfoLog() << "\n" << shader.getInfoDebugLog() << std::endl;
            return false;
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(EShMsgDefault)) {
            std::cerr << "Glslang linking failed for " << slangPath << ":\n" << program.getInfoLog() << "\n" << program.getInfoDebugLog() << std::endl;
            return false;
        }

        glslang::GlslangToSpv(*program.getIntermediate(stage), spirv);
        return true;
    };

    std::vector<unsigned int> vertSpirv;
    std::vector<unsigned int> fragSpirv;

    if (!compileToSPIRV(EShLangVertex, final_vert_source, vertSpirv)) return false;
    if (!compileToSPIRV(EShLangFragment, final_frag_source, fragSpirv)) return false;

    auto spirvToGLSL = [&](const std::vector<unsigned int>& spirv, const std::string& stageName) -> std::string {
        spirv_cross::CompilerGLSL compiler(spirv);

        spirv_cross::CompilerGLSL::Options options;
        options.version = 430;
        options.es = false;
        options.emit_push_constant_as_uniform_buffer = true;
        compiler.set_common_options(options);
        return compiler.compile();
    };

    std::string vertGLSL, fragGLSL;
    try {
        vertGLSL = spirvToGLSL(vertSpirv, "vertex");
        fragGLSL = spirvToGLSL(fragSpirv, "fragment");
    } catch (const spirv_cross::CompilerError& e) {
        std::cerr << "SPIRV-Cross failed: " << e.what() << std::endl;
        return false;
    }

    auto compileGL = [&](GLenum type, const std::string& src) -> GLuint {
        GLuint shader = glCreateShader(type);
        const char* c_str = src.c_str();
        glShaderSource(shader, 1, &c_str, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            GLint logSize = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logSize);
            std::string infoLogStr;
            if (logSize > 0) {
                std::vector<char> infoLog(logSize);
                glGetShaderInfoLog(shader, logSize, nullptr, infoLog.data());
                infoLogStr.assign(infoLog.begin(), infoLog.end() - 1); // remove null terminator
            }
            std::cerr << "OpenGL Compile Error in " << (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment") << " shader:\n" << infoLogStr << std::endl;
            std::cerr << "--- Shader Source ---\n" << src << "\n---------------------\n";
            return 0;
        }
        return shader;
    };

    GLuint vertShader = compileGL(GL_VERTEX_SHADER, vertGLSL);
    GLuint fragShader = compileGL(GL_FRAGMENT_SHADER, fragGLSL);

    if (!vertShader || !fragShader) return false;

    outProgram = glCreateProgram();
    glAttachShader(outProgram, vertShader);
    glAttachShader(outProgram, fragShader);
    glLinkProgram(outProgram);

    GLint linkSuccess;
    glGetProgramiv(outProgram, GL_LINK_STATUS, &linkSuccess);
    if (!linkSuccess) {
        char infoLog[1024];
        glGetProgramInfoLog(outProgram, 1024, nullptr, infoLog);
        std::cerr << "OpenGL Link Error: " << infoLog << std::endl;
        glDeleteProgram(outProgram);
        outProgram = 0;
        return false;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    ProgramLayout progLayout;

    GLint numActiveUniforms;
    glGetProgramiv(outProgram, GL_ACTIVE_UNIFORMS, &numActiveUniforms);
    for (int i = 0; i < numActiveUniforms; ++i) {
        char name[256];
        GLint size;
        GLenum type;
        glGetActiveUniform(outProgram, i, 256, nullptr, &size, &type, name);
        
        std::string sName(name);
        GLint loc = glGetUniformLocation(outProgram, name);
        
        progLayout.uniformLocations[sName] = loc;
        size_t dotPos = sName.find('.');
        if (dotPos != std::string::npos) {
            std::string noPrefix = sName.substr(dotPos + 1);
            size_t bracketPos = noPrefix.find('[');
            if (bracketPos != std::string::npos) noPrefix = noPrefix.substr(0, bracketPos);
            progLayout.uniformLocations[noPrefix] = loc;
        } else {
            size_t bracketPos = sName.find('[');
            if (bracketPos != std::string::npos) sName = sName.substr(0, bracketPos);
            progLayout.uniformLocations[sName] = loc;
        }
    }

    auto extractLayout = [&](const char* name1, const char* name2, UniformLayout& layout, GLuint bindingPoint) {
        layout.blockIndex = glGetUniformBlockIndex(outProgram, name1);
        if (layout.blockIndex == GL_INVALID_INDEX && name2) {
            layout.blockIndex = glGetUniformBlockIndex(outProgram, name2);
        }

        if (layout.blockIndex != GL_INVALID_INDEX) {
            glUniformBlockBinding(outProgram, layout.blockIndex, bindingPoint);
            glGetActiveUniformBlockiv(outProgram, layout.blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &layout.blockSize);

            GLint numUniforms;
            glGetActiveUniformBlockiv(outProgram, layout.blockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &numUniforms);

            if (numUniforms > 0) {
                std::vector<GLint> indices(numUniforms);
                glGetActiveUniformBlockiv(outProgram, layout.blockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, indices.data());

                std::vector<GLint> offsets(numUniforms);
                glGetActiveUniformsiv(outProgram, numUniforms, (GLuint*)indices.data(), GL_UNIFORM_OFFSET, offsets.data());

                for (int i = 0; i < numUniforms; ++i) {
                    char name[256];
                    glGetActiveUniformName(outProgram, indices[i], 256, nullptr, name);
                    std::string sName(name);
                    
                    size_t dotPos = sName.find('.');
                    if (dotPos != std::string::npos) sName = sName.substr(dotPos + 1);
                    
                    size_t bracketPos = sName.find('[');
                    if (bracketPos != std::string::npos) sName = sName.substr(0, bracketPos);
                    
                    layout.offsets[sName] = (size_t)offsets[i];
                }
            }
        }
    };

    extractLayout("UBO", "global", progLayout.globalUBO, 0);
    extractLayout("Push", "params", progLayout.pushUBO, 1);

    m_programLayouts.push_back(progLayout);

    return true;
}

void ShaderManager::RequestPresetLoad(const SlangPreset& preset) {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pendingPreset = preset;
    m_presetPending = true;
}

bool ShaderManager::LoadPreset(const SlangPreset& preset) {
    if (preset.getPasses().empty()) {
        for (GLuint prog : m_shaderPrograms) {
            if (prog) glDeleteProgram(prog);
        }
        m_shaderPrograms.clear();
        m_programLayouts.clear();
        m_preset.clear();
        m_parameters.clear();
        for (auto& pair : m_lutTextures) {
            glDeleteTextures(1, &pair.second);
        }
        m_lutTextures.clear();
        return true;
    }

    m_preset = preset;

    for (auto& fbo : m_passFBOs) {
        if (fbo.fbo) {
            glDeleteFramebuffers(1, &fbo.fbo);
            glDeleteTextures(1, &fbo.texture);
        }
    }
    m_passFBOs.clear();

    for (GLuint prog : m_shaderPrograms) {
        if (prog) glDeleteProgram(prog);
    }
    m_shaderPrograms.clear();
    m_programLayouts.clear();
    
    for (auto& pair : m_lutTextures) {
        glDeleteTextures(1, &pair.second);
    }
    m_lutTextures.clear();

    for (const auto& lut : m_preset.getLUTs()) {
        QImage img(QString::fromStdString(lut.path));
        if (img.isNull()) {
            std::cerr << "Failed to load LUT: " << lut.path << std::endl;
            continue;
        }
        img = img.convertToFormat(QImage::Format_RGBA8888);

        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width(), img.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
        
        GLint filter = lut.linear ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        m_lutTextures[lut.id] = tex;
    }

    std::map<std::string, float> oldParams = m_parameters;
    m_parameters = m_preset.getParameters();
    for (const auto& pragma_param : m_preset.getPragmaParameters()) {
        if (m_parameters.find(pragma_param.name) == m_parameters.end()) {
             m_parameters[pragma_param.name] = pragma_param.defaultValue;
        }
    }
    for (const auto& [name, val] : oldParams) {
        if (m_parameters.count(name)) {
            m_parameters[name] = val;
        }
    }

    const auto& passes = m_preset.getPasses();
    for (const auto& pass : passes) {
        GLuint program = 0;
        if (!CompileShaderPass(pass.shaderPath, program)) {
            std::cerr << "Failed to compile pass: " << pass.shaderPath << std::endl;
            for (GLuint prog : m_shaderPrograms) {
                if (prog) glDeleteProgram(prog);
            }
            m_shaderPrograms.clear();
            m_programLayouts.clear();
            return false;
        }
        m_shaderPrograms.push_back(program);
    }

    return true;
}

void ShaderManager::SetParameter(const std::string& name, float value) {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_parameters[name] = value;
}

GLuint ShaderManager::Process(GLuint inputTexArray, int inputWidth, int inputHeight, int windowWidth, int windowHeight, int& outWidth, int& outHeight) {
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        if (m_presetPending) {
            LoadPreset(m_pendingPreset);
            m_presetPending = false;
        }
    }

    const auto& passes = m_preset.getPasses();

    if (passes.empty() || m_shaderPrograms.empty() || m_shaderPrograms[0] == 0) {
        outWidth = inputWidth;
        outHeight = inputHeight;
        return inputTexArray;
    }

    int finalWidth = inputWidth;
    int finalHeight = inputHeight;
    for (const auto& pass : passes) {
        if (pass.scaleTypeX == "source") finalWidth = static_cast<int>(finalWidth * pass.scaleX);
        if (pass.scaleTypeY == "source") finalHeight = static_cast<int>(finalHeight * pass.scaleY);
        if (pass.scaleTypeX == "viewport") finalWidth = static_cast<int>(windowWidth * pass.scaleX);
        if (pass.scaleTypeY == "viewport") finalHeight = static_cast<int>(windowHeight * pass.scaleY);
        if (pass.scaleTypeX == "absolute") finalWidth = static_cast<int>(pass.scaleX);
        if (pass.scaleTypeY == "absolute") finalHeight = static_cast<int>(pass.scaleY);
    }

    outWidth = finalWidth;
    outHeight = finalHeight;

    ResizeOutputArray(outWidth, outHeight);

    if (m_extractFBO == 0) {
        glGenFramebuffers(1, &m_extractFBO);
    }

    glBindVertexArray(m_quadVAO);

    m_frameCount++;

    if (m_passFBOs.size() != passes.size()) {
        for (auto& fbo : m_passFBOs) {
            if (fbo.fbo) {
                glDeleteFramebuffers(1, &fbo.fbo);
                glDeleteTextures(1, &fbo.texture);
            }
        }
        m_passFBOs.clear();
        m_passFBOs.resize(passes.size());
    }

    for (int layer = 0; layer < 2; ++layer) {
        ResizeFBO(m_fboA, inputWidth, inputHeight, false);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_extractFBO);
        glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, inputTexArray, 0, layer);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fboA.fbo);
        glBlitFramebuffer(0, 0, inputWidth, inputHeight,
                          0, 0, inputWidth, inputHeight,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        GLuint currentInputTex = m_fboA.texture;
        int currentWidth = inputWidth;
        int currentHeight = inputHeight;

        GLuint originalTex = currentInputTex;
        int originalWidth = currentWidth;
        int originalHeight = currentHeight;

        for (size_t i = 0; i < passes.size(); ++i) {
            const auto& pass = passes[i];
            if (m_shaderPrograms[i] == 0) continue;

            int targetWidth = currentWidth;
            int targetHeight = currentHeight;

            if (pass.scaleTypeX == "source") {
                targetWidth = static_cast<int>(currentWidth * pass.scaleX);
            } else if (pass.scaleTypeX == "viewport") {
                targetWidth = static_cast<int>(windowWidth * pass.scaleX);
            } else if (pass.scaleTypeX == "absolute") {
                targetWidth = static_cast<int>(pass.scaleX);
            }

            if (pass.scaleTypeY == "source") {
                targetHeight = static_cast<int>(currentHeight * pass.scaleY);
            } else if (pass.scaleTypeY == "viewport") {
                targetHeight = static_cast<int>(windowHeight * pass.scaleY);
            } else if (pass.scaleTypeY == "absolute") {
                targetHeight = static_cast<int>(pass.scaleY);
            }

            bool isLastPass = (i == passes.size() - 1);

            if (isLastPass) {
                glBindFramebuffer(GL_FRAMEBUFFER, m_outputArrayFBO);
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_outputArrayTex, 0, layer);
                glClear(GL_COLOR_BUFFER_BIT);
            } else {
                ResizeFBO(m_passFBOs[i], targetWidth, targetHeight, pass.filterLinear, pass.srgbFramebuffer, pass.floatFramebuffer);
                glBindFramebuffer(GL_FRAMEBUFFER, m_passFBOs[i].fbo);
                glClear(GL_COLOR_BUFFER_BIT);
            }

            if (pass.srgbFramebuffer) glEnable(GL_FRAMEBUFFER_SRGB);
            else glDisable(GL_FRAMEBUFFER_SRGB);

            glViewport(0, 0, targetWidth, targetHeight);
            glUseProgram(m_shaderPrograms[i]);

            const auto& progLayout = m_programLayouts[i];
            
            auto fillUBO = [&](const UniformLayout& layout, GLuint uboID, GLuint bindingPoint, bool isPush) {
                std::vector<uint8_t> uboData;
                if (layout.blockSize > 0) uboData.resize(layout.blockSize, 0);

                auto setF = [&](const std::string& name, float val) {
                    if (layout.blockSize > 0 && layout.offsets.count(name)) {
                        *(float*)(uboData.data() + layout.offsets.at(name)) = val;
                    } else if (isPush || name == "MVP") {
                        if (progLayout.uniformLocations.count(name)) {
                            glUniform1f(progLayout.uniformLocations.at(name), val);
                        }
                    }
                };
                auto setV4 = [&](const std::string& name, float x, float y, float z, float w) {
                    if (layout.blockSize > 0 && layout.offsets.count(name)) {
                        float* ptr = (float*)(uboData.data() + layout.offsets.at(name));
                        ptr[0] = x; ptr[1] = y; ptr[2] = z; ptr[3] = w;
                    } else if (isPush || name.find("Size") != std::string::npos) {
                        if (progLayout.uniformLocations.count(name)) {
                            glUniform4f(progLayout.uniformLocations.at(name), x, y, z, w);
                        }
                    }
                };
                auto setM4 = [&](const std::string& name, const float* m) {
                    if (layout.blockSize > 0 && layout.offsets.count(name)) {
                        float* ptr = (float*)(uboData.data() + layout.offsets.at(name));
                        for (int j = 0; j < 16; ++j) ptr[j] = m[j];
                    } else if (isPush || name == "MVP") {
                        if (progLayout.uniformLocations.count(name)) {
                            glUniformMatrix4fv(progLayout.uniformLocations.at(name), 1, GL_FALSE, m);
                        }
                    }
                };

                const float mvp[16] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f };
                setM4("MVP", mvp);
                setV4("SourceSize", (float)currentWidth, (float)currentHeight, 1.0f / (float)currentWidth, 1.0f / (float)currentHeight);
                setV4("OriginalSize", (float)originalWidth, (float)originalHeight, 1.0f / (float)originalWidth, 1.0f / (float)originalHeight);
                setV4("OutputSize", (float)targetWidth, (float)targetHeight, 1.0f / (float)targetWidth, 1.0f / (float)targetHeight);
                setF("FrameCount", (float)m_frameCount);
                setF("FrameDirection", 1.0f);
                setF("Rotation", 0.0f);

                for (size_t prevPass = 0; prevPass < i; ++prevPass) {
                    if (!passes[prevPass].alias.empty()) {
                        std::string aliasSizeName = passes[prevPass].alias + "Size";
                        setV4(aliasSizeName, (float)m_passFBOs[prevPass].width, (float)m_passFBOs[prevPass].height, 
                              1.0f / (float)m_passFBOs[prevPass].width, 1.0f / (float)m_passFBOs[prevPass].height);
                    }
                    std::string passSizeName = "PassOutput" + std::to_string(prevPass) + "Size";
                    setV4(passSizeName, (float)m_passFBOs[prevPass].width, (float)m_passFBOs[prevPass].height, 
                          1.0f / (float)m_passFBOs[prevPass].width, 1.0f / (float)m_passFBOs[prevPass].height);
                }

                for (const auto& [name, value] : m_parameters) {
                    setF(name, value);
                }

                if (layout.blockSize > 0) {
                    glBindBuffer(GL_UNIFORM_BUFFER, uboID);
                    glBufferData(GL_UNIFORM_BUFFER, layout.blockSize, uboData.data(), GL_DYNAMIC_DRAW);
                    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, uboID);
                }
            };

            fillUBO(progLayout.globalUBO, m_globalUBO, 0, false);
            fillUBO(progLayout.pushUBO, m_pushConstantsUBO, 1, true);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, currentInputTex);
            if (progLayout.uniformLocations.count("Source")) glUniform1i(progLayout.uniformLocations.at("Source"), 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, originalTex);
            if (progLayout.uniformLocations.count("Original")) glUniform1i(progLayout.uniformLocations.at("Original"), 1);

            int texUnit = 2;
            for (size_t prevPass = 0; prevPass < i; ++prevPass) {
                glActiveTexture(GL_TEXTURE0 + texUnit);
                glBindTexture(GL_TEXTURE_2D, m_passFBOs[prevPass].texture);
                
                std::string passName = "PassOutput" + std::to_string(prevPass);
                if (progLayout.uniformLocations.count(passName)) glUniform1i(progLayout.uniformLocations.at(passName), texUnit);

                if (!passes[prevPass].alias.empty()) {
                    if (progLayout.uniformLocations.count(passes[prevPass].alias)) glUniform1i(progLayout.uniformLocations.at(passes[prevPass].alias), texUnit);
                }
                texUnit++;
            }

            for (const auto& pair : m_lutTextures) {
                glActiveTexture(GL_TEXTURE0 + texUnit);
                glBindTexture(GL_TEXTURE_2D, pair.second);
                if (progLayout.uniformLocations.count(pair.first)) glUniform1i(progLayout.uniformLocations.at(pair.first), texUnit);
                texUnit++;
            }

            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            if (!isLastPass) {
                currentInputTex = m_passFBOs[i].texture;
                currentWidth = targetWidth;
                currentHeight = targetHeight;
            }
        }
        glDisable(GL_FRAMEBUFFER_SRGB);
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return m_outputArrayTex;
}
