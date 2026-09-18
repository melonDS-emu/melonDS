#ifndef GPU3D_TEXCACHEOPENGL
#define GPU3D_TEXCACHEOPENGL

#include "GPU3D_Texcache.h"
#include "OpenGLSupport.h"

namespace melonDS
{

template <typename, typename, typename>
class Texcache;

class TexcacheOpenGLLoader
{
public:
    TexcacheOpenGLLoader(bool compute) : IsCompute(compute) {}

    OpenGL::TextureHandle GenerateTexture(u32 width, u32 height, u32 layers);
    void UploadTexture(GLuint handle, u32 width, u32 height, u32 layer, void* data);

private:
    bool IsCompute;
};

using TexcacheOpenGL = Texcache<TexcacheOpenGLLoader, GLuint, OpenGL::TextureHandle>;

}

#endif