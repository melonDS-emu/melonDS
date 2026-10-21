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

#include "OpenGLSupport.h"

#include <unordered_map>
#include <vector>

#include <assert.h>

#define XXH_STATIC_LINKING_ONLY
#include "xxhash/xxhash.h"

namespace melonDS
{

using Platform::Log;
using Platform::LogLevel;

namespace OpenGL
{

struct ShaderCacheEntry
{
    u32 Length;
    u8* Data;
    u32 BinaryFormat;

    ShaderCacheEntry(u8* data, u32 length, u32 binaryFmt)
        : Length(length), Data(data), BinaryFormat(binaryFmt)
    {
        assert(data != nullptr);
    }

    ShaderCacheEntry(const ShaderCacheEntry&) = delete;
    ShaderCacheEntry(ShaderCacheEntry&& other)
    {
        Data = other.Data;
        Length = other.Length;
        BinaryFormat = other.BinaryFormat;

        other.Data = nullptr;
        other.Length = 0;
        other.BinaryFormat = 0;
    }

    ~ShaderCacheEntry()
    {
        if (Data) // check whether it was moved
            delete[] Data;
    }
};

std::unordered_map<u64, ShaderCacheEntry> ShaderCache;
std::vector<u64> NewShaders;

constexpr u32 ShaderCacheMagic = 0x11CAC4E1;
constexpr u32 ShaderCacheVersion = 1;

void LoadShaderCache()
{
    // for now the shader cache only contains only compute shaders
    // because they take the longest to compile
    Platform::FileHandle* file = Platform::OpenLocalFile("shadercache", Platform::FileMode::Read);
    if (file == nullptr)
    {
        Log(LogLevel::Error, "Could not find shader cache\n");
        return;
    }

    u32 magic, version, numPrograms;
    if (Platform::FileRead(&magic, 4, 1, file) != 1 || magic != ShaderCacheMagic)
    {
        Log(LogLevel::Error, "Shader cache file has invalid magic\n");
        goto fileInvalid;
    }

    if (Platform::FileRead(&version, 4, 1, file) != 1 || version != ShaderCacheVersion)
    {
        Log(LogLevel::Error, "Shader cache file has bad version\n");
        goto fileInvalid;
    }

    if (Platform::FileRead(&numPrograms, 4, 1, file) != 1)
    {
        Log(LogLevel::Error, "Shader cache file invalid program count\n");
        goto fileInvalid;
    }

    // not the best approach, because once changes pile up
    // we read and overwrite the old files
    for (u32 i = 0; i < numPrograms; i++)
    {
        int error = 3;

        u32 length, binaryFormat;
        u64 sourceHash;
        error -= Platform::FileRead(&sourceHash, 8, 1, file);
        error -= Platform::FileRead(&length, 4, 1, file);
        error -= Platform::FileRead(&binaryFormat, 4, 1, file);

        if (error != 0)
        {
            Log(LogLevel::Error, "Invalid shader cache entry\n");
            goto fileInvalid;
        }

        u8* data = new u8[length];
        if (Platform::FileRead(data, length, 1, file) != 1)
        {
            Log(LogLevel::Error, "Could not read shader cache entry data\n");
            delete[] data;
            goto fileInvalid;
        }

        ShaderCache.erase(sourceHash);
        ShaderCache.emplace(sourceHash, ShaderCacheEntry(data, length, binaryFormat));
    }

fileInvalid:
    Platform::CloseFile(file);
}

void SaveShaderCache()
{
    Platform::FileHandle* file = Platform::OpenLocalFile("shadercache", Platform::FileMode::ReadWrite);

    if (file == nullptr)
    {
        Log(LogLevel::Error, "Could not open or create shader cache file\n");
        return;
    }

    int written = 3;
    u32 magic = ShaderCacheMagic, version = ShaderCacheVersion, numPrograms = ShaderCache.size();
    written -= Platform::FileWrite(&magic, 4, 1, file);
    written -= Platform::FileWrite(&version, 4, 1, file);
    written -= Platform::FileWrite(&numPrograms, 4, 1, file);

    if (written != 0)
    {
        Log(LogLevel::Error, "Could not write shader cache header\n");
        goto writeError;
    }

    Platform::FileSeek(file, 0, Platform::FileSeekOrigin::End);

    printf("new shaders %zu\n", NewShaders.size());

    for (u64 newShader : NewShaders)
    {
        int error = 4;
        auto it = ShaderCache.find(newShader);

        error -= Platform::FileWrite(&it->first, 8, 1, file);
        error -= Platform::FileWrite(&it->second.Length, 4, 1, file);
        error -= Platform::FileWrite(&it->second.BinaryFormat, 4, 1, file);
        error -= Platform::FileWrite(it->second.Data, it->second.Length, 1, file);

        if (error != 0)
        {
            Log(LogLevel::Error, "Could not insert new shader cache entry\n");
            goto writeError;
        }
    }

writeError:
    Platform::CloseFile(file);

    NewShaders.clear();
}

bool CompileShader(const ShaderHandle& id, std::string_view source, std::string_view name, std::string_view type)
{
    int res;

    if (!glCreateShader)
    {
        Log(LogLevel::Error, "OpenGL: Cannot build shader program, OpenGL hasn't been loaded\n");
        return false;
    }

    const char* sourceC = source.data();
    int len = source.length();
    glShaderSource(id, 1, &sourceC, &len);

    glCompileShader(id);

    glGetShaderiv(id, GL_COMPILE_STATUS, &res);
    if (res != GL_TRUE)
    {
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &res);
        if (res < 1) res = 1024;
        char* log = new char[res+1];
        glGetShaderInfoLog(id, res+1, NULL, log);
        Log(LogLevel::Error, "OpenGL: failed to compile %s shader %s: %s\n", type.data(), name.data(), log);
        Log(LogLevel::Debug, "shader source:\n--\n%s\n--\n", source.data());
        delete[] log;

        glDeleteShader(id);

        return false;
    }

    return true;
}

bool LinkProgram(const ProgramHandle& result, ShaderHandle* ids, int numIds)
{
    int res;

    if (!glLinkProgram)
    {
        Log(LogLevel::Error, "OpenGL: Cannot link shader program, OpenGL hasn't been loaded\n");
        return false;
    }

    for (int i = 0; i < numIds; i++)
    {
        glAttachShader(result, ids[i]);
    }

    glLinkProgram(result);

    for (int i = 0; i < numIds; i++)
        glDetachShader(result, ids[i]);

    glGetProgramiv(result, GL_LINK_STATUS, &res);
    if (res != GL_TRUE)
    {
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &res);
        if (res < 1) res = 1024;
        char* log = new char[res+1];
        glGetProgramInfoLog(result, res+1, NULL, log);
        Log(LogLevel::Error, "OpenGL: failed to link shader program: %s\n", log);
        delete[] log;

        return false;
    }

    return true;
}

ProgramHandle CompileComputeProgram(std::string_view source, std::string_view name) noexcept
{
    if (!(ShaderHandle::Available() && ProgramHandle::Available()))
        return {};

    ProgramHandle program(GenerateNow);

    ShaderHandle shader(GL_COMPUTE_SHADER);

    if (!CompileShader(shader, source, name, "compute"))
        return {}; // Program will be cleaned up with RAII

    if (!LinkProgram(program, &shader, 1))
        return {};

    return program;
}

using melonDS::OpenGL::AttributeTarget;

ProgramHandle CompileVertexFragmentProgram(
    std::string_view vs, std::string_view fs,
    std::string_view name,
    const std::initializer_list<AttributeTarget>& vertexInAttrs,
    const std::initializer_list<AttributeTarget>& fragmentOutAttrs) noexcept
{
    ShaderHandle shaders[2] =
    {
        ShaderHandle(GL_VERTEX_SHADER),
        ShaderHandle(GL_FRAGMENT_SHADER),
    };
    ProgramHandle program(GenerateNow);

    bool linkingSucess = false;

    if (!CompileShader(shaders[0], vs, name, "vertex"))
        return {};

    if (!CompileShader(shaders[1], fs, name, "fragment"))
        return {};

    for (const AttributeTarget& target : vertexInAttrs)
    {
        glBindAttribLocation(program, target.Location, target.Name);
    }

    for (const AttributeTarget& target : fragmentOutAttrs)
    {
        glBindFragDataLocation(program, target.Location, target.Name);
    }

    if (!LinkProgram(program, shaders, 2))
        return {};

    return program;
}

}

}