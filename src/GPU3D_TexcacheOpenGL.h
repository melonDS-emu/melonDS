#ifndef GPU3D_TEXCACHEOPENGL
#define GPU3D_TEXCACHEOPENGL

#include "GPU3D_Texcache.h"
#include "OpenGLSupport.h"

namespace melonDS
{

template <typename, typename>
class Texcache;

class TexcacheOpenGLLoader
{
public:
    TexcacheOpenGLLoader(bool compute) : IsCompute(compute) {}

    GLuint GenerateTexture(u32 width, u32 height, u32 layers);
    void UploadTexture(GLuint handle, u32 width, u32 height, u32 layer, void* data);
    void DeleteTexture(GLuint handle);

private:
    bool IsCompute;
};

using TexcacheOpenGL = Texcache<TexcacheOpenGLLoader, GLuint>;

}

#endif