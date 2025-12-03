#version 140

uniform sampler2D SpriteTex;

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

layout(std140) uniform uConfig
{
    int uVRAMMask;
    ivec4 uRotscale[32];
    sOAM uOAM[128];
};

flat in int fSpriteIndex;
smooth in vec2 fTexcoord;

out vec4 oColor;
out vec4 oFlags;

vec4 GetSpritePixel(int sprite, vec2 coord)
{
    ivec2 basecoord = ivec2((sprite & 0xF) * 64, (sprite >> 4) * 64);

    return texelFetch(SpriteTex, basecoord + ivec2(coord), 0);
}

void main()
{
    if (uOAM[fSpriteIndex].Rotscale == -1)
    {
        // regular sprite
        // fTexcoord is the position within the sprite bitmap

        vec4 col = GetSpritePixel(fSpriteIndex, ivec2(fTexcoord));
        if (col.a == 0) discard;

        oColor = col;
        oFlags = vec4(1,1,1,1);
    }
    else
    {
        // rotscale sprite
        // fTexcoord is based on the sprite center

        vec2 sprsize = vec2(uOAM[fSpriteIndex].Size);
        vec4 rotscale = vec4(uRotscale[uOAM[fSpriteIndex].Rotscale]) / 256;
        mat2 rsmatrix = mat2(rotscale.xy, rotscale.zw);
        vec2 rscoord = (fTexcoord * rsmatrix) + (sprsize / 2);
        if (any(lessThan(rscoord, vec2(0)))) discard;
        if (any(greaterThanEqual(rscoord, sprsize))) discard;

        vec4 col = GetSpritePixel(fSpriteIndex, rscoord);
        if (col.a == 0) discard;

        oColor = col;
        oFlags = vec4(1,1,1,1);
    }
}
