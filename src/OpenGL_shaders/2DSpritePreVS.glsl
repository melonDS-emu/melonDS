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
in int vSpriteIndex;

flat out int fSpriteIndex;
smooth out vec2 fTexcoord;

void main()
{
    ivec2 sprpos = ivec2((vSpriteIndex & 0xF) * 64, (vSpriteIndex >> 4) * 64);
    ivec2 sprsize = uOAM[vSpriteIndex].Size;
    vec2 vtxpos = vec2(sprpos) + (vPosition * vec2(sprsize));
    vec2 fbsize = vec2(1024, 512);

    gl_Position = vec4(((vtxpos * 2) / fbsize) - 1, 0, 1);
    fSpriteIndex = vSpriteIndex;
    fTexcoord = vPosition * vec2(sprsize);
}
