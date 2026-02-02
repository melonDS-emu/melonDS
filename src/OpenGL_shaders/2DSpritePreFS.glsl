#version 140

uniform usampler2D VRAMTex;
uniform sampler2D PalTex;

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

flat in int fSpriteIndex;
smooth in vec2 fTexcoord;

out vec4 oColor;

vec4 GetOBJPalEntry(int pal, int id)
{
    ivec2 coord = ivec2(id, pal);
    vec4 col = texelFetch(PalTex, coord, 0);
    col.rgb *= (62.0/63.0);
    col.g += (col.a * 1.0/63.0);
    return col;
}

int VRAMRead8(int addr)
{
    ivec2 coord = ivec2(addr & 0x3FF, (addr >> 10) & uVRAMMask);
    int val = int(texelFetch(VRAMTex, coord, 0).r);
    return val;
}

int VRAMRead16(int addr)
{
    ivec2 coord = ivec2(addr & 0x3FF, (addr >> 10) & uVRAMMask);
    int lo = int(texelFetch(VRAMTex, coord, 0).r);
    int hi = int(texelFetch(VRAMTex, coord+ivec2(1,0), 0).r);
    return lo | (hi << 8);
}

vec4 GetSpritePixel(int sprite, ivec2 coord)
{
    vec4 ret;

    if (uOAM[sprite].Type == 0)
    {
        // 16-color

        int tileoffset = uOAM[sprite].TileOffset +
            ((coord.x >> 3) * 32) +
            ((coord.y >> 3) * uOAM[sprite].TileStride) +
            ((coord.x & 0x7) >> 1) +
            ((coord.y & 0x7) << 2);

        int col = VRAMRead8(tileoffset);
        if ((coord.x & 1) != 0)
            col >>= 4;
        else
            col &= 0xF;
        col += uOAM[sprite].PalOffset;

        ret = GetOBJPalEntry(0, col);
        ret.a = ((col & 0xF) == 0) ? 0 : 1;
    }
    else if (uOAM[sprite].Type == 1)
    {
        // 256-color

        int tileoffset = uOAM[sprite].TileOffset +
            ((coord.x >> 3) * 64) +
            ((coord.y >> 3) * uOAM[sprite].TileStride) +
             (coord.x & 0x7) +
            ((coord.y & 0x7) << 3);

        int col = VRAMRead8(tileoffset);

        ret = GetOBJPalEntry(uOAM[sprite].PalOffset, col);
        ret.a = (col == 0) ? 0 : 1;
    }
    else //if (uOAM[sprite].Type == 2)
    {
        // direct color bitmap

        int tileoffset = uOAM[sprite].TileOffset +
            (coord.x * 2) +
            (coord.y * uOAM[sprite].TileStride);

        int col = VRAMRead16(tileoffset);

        ret.r = float((col << 1) & 0x3E) / 63;
        ret.g = float((col >> 4) & 0x3E) / 63;
        ret.b = float((col >> 9) & 0x3E) / 63;
        ret.a = float(col >> 15);
    }

    return ret;
}

void main()
{
    oColor = GetSpritePixel(fSpriteIndex, ivec2(fTexcoord));
}
