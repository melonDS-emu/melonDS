#version 140

struct sOAM
{
    ivec2 Position;
    bvec2 Flip;
    ivec2 Size;
    ivec2 BoundSize;
    int OBJMode;
    int Type;
    int PalOffset;
    int TileOffset;
    int TileStride;
    int Rotscale;
    int BGPrio;
    bool Mosaic;
};

layout(std140) uniform ubSpriteConfig
{
    int uVRAMMask;
    ivec4 uRotscale[32];
    sOAM uOAM[128];
};

in ivec2 vPosition;
in ivec2 vTexcoord;
in int vSpriteIndex;

flat out int fSpriteIndex;
smooth out vec2 fPosition;
smooth out vec2 fTexcoord;

void main()
{
    vec2 sprsize = vec2(uOAM[vSpriteIndex].BoundSize);
    vec2 fbsize = vec2(256, 192);

    int totalprio = (uOAM[vSpriteIndex].BGPrio * 128) + vSpriteIndex;
    float z = float(totalprio) / 512.0;
    gl_Position = vec4(((vec2(vPosition) * 2) / fbsize) - 1, z, 1);
    fPosition = vPosition;
    fSpriteIndex = vSpriteIndex;

    if (uOAM[vSpriteIndex].Rotscale == -1)
    {
        vec2 tmp = vec2(vTexcoord) * sprsize;
        fTexcoord = mix(tmp, (sprsize - tmp), uOAM[vSpriteIndex].Flip);
    }
    else
        fTexcoord = (vec2(vTexcoord) * sprsize) - (sprsize / 2);
}
