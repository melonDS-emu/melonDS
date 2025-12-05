#version 140

uniform sampler2DArray BGLayerTex;
uniform sampler2D _3DLayerTex;

struct sScanline
{
    ivec2 BGOffset[4];
    ivec4 BGRotscale[2];
    int BackColor;
};

layout(std140) uniform uScanlineConfig
{
    sScanline uScanline[192];
};

struct sBGConfig
{
    ivec2 Size;
    int Type;
    int PalOffset;
    int TileOffset;
    int MapOffset;
    bool Clamp;
};

layout(std140) uniform uConfig
{
    int uVRAMMask;
    sBGConfig uBGConfig[4];
};

uniform int uScaleFactor;
uniform int uCurBG;

smooth in vec2 fTexcoord;

out vec4 oColor;

vec4 GetBGLayerPixel(int layer, vec2 coord)
{
    return texelFetch(BGLayerTex, ivec3(ivec2(coord), layer), 0);
}

void main()
{
    vec2 coord;
    int line = int(fTexcoord.y);
    vec2 bgsize = vec2(uBGConfig[uCurBG].Size);

    if (uBGConfig[uCurBG].Type == 6)
    {
        // 3D layer
        int hofs = uScanline[line].BGOffset[uCurBG].x & 0x1FF;
        hofs -= ((hofs & 0x100) << 1);
        coord = vec2(float(hofs), 0) + fTexcoord;

        if (coord.x < 0 || coord.x >= 256)
        {
            oColor = vec4(0);
            return;
        }

        vec4 col = texelFetch(_3DLayerTex, ivec2(coord * uScaleFactor), 0);
        col.a *= float(0x01) / 255;
        oColor = col;
        return;
    }

    if (uBGConfig[uCurBG].Type >= 2)
    {
        // rotscale BG
        coord = vec2(uScanline[line].BGOffset[uCurBG]) / 256;
        vec4 rotscale = vec4(uScanline[line].BGRotscale[uCurBG-2]) / 256;
        mat2 rsmatrix = mat2(rotscale.xy, rotscale.zw);
        coord = coord + (vec2(fTexcoord.x, fract(fTexcoord.y)) * rsmatrix);
    }
    else
    {
        // text-mode BG
        coord = vec2(uScanline[line].BGOffset[uCurBG]) + fTexcoord;
    }

    // TODO also provision for hi-res capture

    if (uBGConfig[uCurBG].Clamp)
    {
        if (any(lessThan(coord, vec2(0))) || any(greaterThanEqual(coord, bgsize)))
        {
            oColor = vec4(0);
            return;
        }
    }
    else
    {
        coord = mod(coord, bgsize);
    }

    vec4 col = GetBGLayerPixel(uCurBG, coord);
    col.a *= float(1 << uCurBG) / 255;
    oColor = col;
}
