#version 140

uniform sampler2D SpriteTex;
uniform sampler2DArray Capture128Tex;
uniform sampler2DArray Capture256Tex;

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
    int uScaleFactor;
    int uVRAMMask;
    ivec4 uCaptureMask[8];
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
    vec4 col, flags = vec4(0);
    vec2 coord;

    if (uOAM[fSpriteIndex].Rotscale == -1)
    {
        // regular sprite
        // fTexcoord is the position within the sprite bitmap

        coord = fTexcoord;
    }
    else
    {
        // rotscale sprite
        // fTexcoord is based on the sprite center

        vec2 sprsize = vec2(uOAM[fSpriteIndex].Size);
        vec4 rotscale = vec4(uRotscale[uOAM[fSpriteIndex].Rotscale]) / 256;
        mat2 rsmatrix = mat2(rotscale.xy, rotscale.zw);
        coord = (fTexcoord * rsmatrix) + (sprsize / 2);
        if (any(lessThan(coord, vec2(0)))) discard;
        if (any(greaterThanEqual(coord, sprsize))) discard;
    }

    if (uOAM[fSpriteIndex].Type == 3)
    {
        coord += (uOAM[fSpriteIndex].TileOffset / ivec2(2, 256));
        coord = mod(coord, 128);
        ivec3 icoord = ivec3(coord * uScaleFactor, uOAM[fSpriteIndex].TileStride);
        col = texelFetch(Capture128Tex, icoord, 0);
    }
    else if (uOAM[fSpriteIndex].Type == 4)
    {
        coord += (uOAM[fSpriteIndex].TileOffset / ivec2(2, 512));
        coord = mod(coord, 256);
        ivec3 icoord = ivec3(coord * uScaleFactor, uOAM[fSpriteIndex].TileStride);
        col = texelFetch(Capture256Tex, icoord, 0);
    }
    else
    {
        col = GetSpritePixel(fSpriteIndex, coord);
    }

    if (col.a == 0) discard;

    // oFlags:
    // r = sprite blending flag
    // g = mosaic flag
    // b = OBJ window flag
    // a = BG prio

    if (uOAM[fSpriteIndex].OBJMode == 2)
    {
        // OBJ window
        // OBJ mosaic doesn't apply to "OBJ window" sprites
        // TODO set appropriate color mask so the color buffer isn't updated
        flags.b = 1;
    }
    else
    {
        if (uOAM[fSpriteIndex].OBJMode == 1)
        {
            // semi-transparent sprite
            flags.r = 1.0 / 255;
        }
        else if (uOAM[fSpriteIndex].OBJMode == 3)
        {
            // bitmap sprite
            col.a = float(uOAM[fSpriteIndex].PalOffset) / 31;
            flags.r = 2.0 / 255;
        }

        if (uOAM[fSpriteIndex].Mosaic)
            flags.g = 1;

        flags.a = float(uOAM[fSpriteIndex].BGPrio) / 255;
    }

    oColor = col;
    oFlags = flags;
}
