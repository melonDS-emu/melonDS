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
    vec4 col, flags = vec4(0);

    if (uOAM[fSpriteIndex].Rotscale == -1)
    {
        // regular sprite
        // fTexcoord is the position within the sprite bitmap

        col = GetSpritePixel(fSpriteIndex, ivec2(fTexcoord));
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

        col = GetSpritePixel(fSpriteIndex, rscoord);
    }

    if (col.a == 0) discard;

    // oFlags:
    // r = sprite blending flag
    // g = mosaic flag
    // b = OBJ window flag
    // a = BG prio

    if (uOAM[fSpriteIndex].OBJMode == 1)
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
            flags.r = 1 / 255;
        }
        else if (uOAM[fSpriteIndex].OBJMode == 3)
        {
            // bitmap sprite
            col.a = float(uOAM[fSpriteIndex].PalOffset) / 31;
            flags.r = 2 / 255;
        }

        if (uOAM[fSpriteIndex].Mosaic)
            flags.g = 1;

        flags.a = uOAM[fSpriteIndex].BGPrio / 255;
    }

    oColor = col;
    oFlags = flags;
}
