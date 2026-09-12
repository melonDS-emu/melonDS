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

#ifndef OPENGLSUPPORT_H
#define OPENGLSUPPORT_H

#include <stdio.h>
#include <string.h>

#include "Platform.h"
#include "PlatformOGL.h"

namespace melonDS::OpenGL
{

// Pass OpenGL::GenerateNow to a GLHandles constructor
// to call the OpenGL initializer defined in Create()
inline constexpr struct GenerateNow_t { explicit GenerateNow_t() = default; } GenerateNow {};

/// Functions that decide what resource to allocate
namespace Traits
{
struct Buffer
{
    static void Gen(GLsizei n, GLuint* buffers) noexcept
    {
        glGenBuffers(n, buffers);
    }

    static void Delete(GLsizei n, const GLuint* buffers) noexcept
    {
        glDeleteBuffers(n, buffers);
    }

    static bool Available() noexcept
    {
        return glGenBuffers && glDeleteBuffers;
    }
};

struct VertexArray
{
    static void Gen(GLsizei n, GLuint* arrays) noexcept
    {
        glGenVertexArrays(n, arrays);
    }

    static void Delete(GLsizei n, const GLuint* arrays) noexcept
    {
        glDeleteVertexArrays(n, arrays);
    }

    static bool Available() noexcept
    {
        return glGenVertexArrays && glDeleteVertexArrays;
    }
};

struct Framebuffer
{
    static void Gen(GLsizei n, GLuint* ids) noexcept
    {
        glGenFramebuffers(n, ids);
    }

    static void Delete(GLsizei n, const GLuint* ids) noexcept
    {
        glDeleteFramebuffers(n, ids);
    }

    static bool Available() noexcept
    {
        return glGenFramebuffers && glDeleteFramebuffers;
    }
};

struct Texture
{
    static void Gen(GLsizei n, GLuint* textures) noexcept
    {
        glGenTextures(n, textures);
    }

    static void Delete(GLsizei n, const GLuint* textures) noexcept
    {
        glDeleteTextures(n, textures);
    }

    static bool Available() noexcept
    {
        return glGenTextures && glDeleteTextures;
    }
};


/// A thin RAII-scented wrapper around one or more OpenGL resource handles.
/// Doesn't do any querying or state management,
/// it just manages the lifetime of an OpenGL object.
/// WARNING: Assumes that the context is current when creating and destroying!
template<class T, std::size_t N>
struct GLHandles
{
    std::array<GLuint, N> ids {};

    explicit GLHandles(GenerateNow_t) noexcept
    {
        T::Gen(ids.size(), ids.data());
    }

    GLHandles() noexcept = default;

    ~GLHandles() noexcept
    {
        T::Delete(ids.size(), ids.data());
    }

    /// Clears the existing OpenGL object and generates a new one in-place.
    GLHandles& operator=(GenerateNow_t) noexcept
    {
        T::Delete(ids.size(), ids.data());
        T::Gen(ids.size(), ids.data());
        return *this;
    }

    // Can't implicitly copy OpenGL resources,
    // so let the compiler stop us
    GLHandles(const GLHandles&) = delete;
    GLHandles& operator=(const GLHandles&) = delete;

    GLHandles(GLHandles&& other) noexcept
    {
        ids = other.ids;
        other.ids.fill(0);
    }

    [[nodiscard]] operator GLuint*() noexcept
    {
        return ids.data();
    }

    [[nodiscard]] operator const GLuint*() const noexcept
    {
        return ids.data();
    }

    GLHandles& operator=(GLHandles&& other) noexcept
    {
        if (ids != other.ids)
        {
            T::Delete(ids.size(), ids.data());
            ids = other.ids;
            other.ids.fill(0);
        }

        return *this;
    }

    [[nodiscard]] static bool Available() noexcept
    {
        return T::Available();
    }

    [[nodiscard]] operator GLuint() const noexcept
    {
        static_assert(N == 1, "Implicit conversion to GLuint only possible for single handles");
        return ids[0];
    }
};
}
template<std::size_t N>
using BufferHandles = Traits::GLHandles<Traits::Buffer, N>;
using BufferHandle = BufferHandles<1>;

template<std::size_t N>
using VertexArrayHandles = Traits::GLHandles<Traits::VertexArray, N>;
using VertexArrayHandle = VertexArrayHandles<1>;

template<std::size_t N>
using FramebufferHandles = Traits::GLHandles<Traits::Framebuffer, N>;
using FramebufferHandle = FramebufferHandles<1>;

template<std::size_t N>
using TextureHandles = Traits::GLHandles<Traits::Texture, N>;
using TextureHandle = TextureHandles<1>;

// Doesn't use GLHandles because shader programs don't follow
// the usual glGenThings/glDeleteThings convention
struct ProgramHandle
{
    GLuint id {};

    explicit ProgramHandle(GenerateNow_t) noexcept
        : id(glCreateProgram()) {}

    ProgramHandle() noexcept = default;

    ~ProgramHandle() noexcept
    {
        glDeleteProgram(id);
    }

    // Can't implicitly copy OpenGL resources,
    // so let the compiler stop us
    ProgramHandle(const ProgramHandle&) = delete;
    ProgramHandle& operator=(const ProgramHandle&) = delete;

    ProgramHandle(ProgramHandle&& other) noexcept
    {
        id = other.id;
        other.id = 0;
    }

    ProgramHandle& operator=(ProgramHandle&& other) noexcept
    {
        if (id != other.id)
        {
            glDeleteProgram(id);
            id = other.id;
            other.id = 0;
        }

        return *this;
    }

    [[nodiscard]] operator GLuint() const noexcept
    {
        return id;
    }

    [[nodiscard]] static bool Available() noexcept
    {
        return glCreateProgram && glDeleteProgram;
    }
};

struct ShaderHandle
{
    GLuint id {};

    ShaderHandle() noexcept = default;

    explicit ShaderHandle(GLenum type) noexcept
        : id(glCreateShader(type)) {}

    ~ShaderHandle() noexcept
    {
        glDeleteShader(id);
    }

    ShaderHandle(const ShaderHandle&) = delete;
    ShaderHandle& operator=(const ShaderHandle&) = delete;

    ShaderHandle(ShaderHandle&& other) noexcept
    {
        id = other.id;
        other.id = 0;
    }

    ShaderHandle& operator=(ShaderHandle&& other) noexcept
    {
        if (id != other.id)
        {
            glDeleteShader(id);
            id = other.id;
            other.id = 0;
        }

        return *this;
    }

    [[nodiscard]] operator GLuint() const noexcept
    {
        return id;
    }

    [[nodiscard]] static bool Available() noexcept
    {
        return glCreateShader && glDeleteShader;
    }
};

void LoadShaderCache();
void SaveShaderCache();

struct AttributeTarget
{
    const char* Name;
    u32 Location;
};


bool CompileVertexFragmentProgram(GLuint& result,
    const std::string& vs, const std::string& fs,
    const std::string& name,
    const std::initializer_list<AttributeTarget>& vertexInAttrs,
    const std::initializer_list<AttributeTarget>& fragmentOutAttrs);

bool CompileComputeProgram(GLuint& result, const std::string& source, const std::string& name);

}

#endif // OPENGLSUPPORT_H
