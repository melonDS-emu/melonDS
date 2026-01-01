#version 140

uniform sampler2D BGLayerTex;
uniform sampler2D _3DLayerTex;
uniform sampler2DArray CaptureTex;

struct sScanline
{
    ivec2 BGOffset[4];
    ivec4 BGRotscale[2];
    int BackColor;
    uint WinRegs;
    int WinMask;
    ivec4 WinPos;
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
    ivec4 uCaptureMask[8];
    sBGConfig uBGConfig[4];
};

uniform int uScaleFactor;
uniform int uCurUnit;
uniform int uCurBG;

smooth in vec2 fTexcoord;

out vec4 oColor;
out vec4 oCaptureColor;

vec4 GetBGLayerPixel(int layer, vec2 coord)
{
    return texelFetch(BGLayerTex, ivec2(coord), 0);
}

void main()
{
    vec2 coord;
    int line = int(fTexcoord.y);
    vec2 bgsize = vec2(uBGConfig[uCurBG].Size);
    vec4 _3dcolor = vec4(0);

    if ((uCurUnit == 0) && (uCurBG == 0))
    {
        int hofs = uScanline[line].BGOffset[uCurBG].x & 0x1FF;
        hofs -= ((hofs & 0x100) << 1);
        coord = vec2(float(hofs), 0) + fTexcoord;

        if (coord.x >= 0 && coord.x < 256)
            _3dcolor = texelFetch(_3DLayerTex, ivec2(coord * uScaleFactor), 0);

        oCaptureColor = _3dcolor;
    }

    if (uBGConfig[uCurBG].Type == 6)
    {
        // 3D layer
        oColor = _3dcolor;
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

    if ((uBGConfig[uCurBG].Type == 5) && (uBGConfig[uCurBG].Size.x <= 256))
    {
        // direct bitmap BG
        // check for a display capture

        ivec2 icoord = ivec2(coord);
        int mapoffset = uBGConfig[uCurBG].MapOffset +
            ((icoord.x +
            (icoord.y * uBGConfig[uCurBG].Size.x)) << 1);

        int block = (mapoffset >> 14) & (uVRAMMask >> 4);
        int cap = uCaptureMask[block >> 2][block & 0x3];
        if (cap != -1)
        {
            if (uBGConfig[uCurBG].Size.x == 128)
            {
                icoord = ivec2(coord * uScaleFactor);
                oColor = texelFetch(CaptureTex, ivec3(icoord, cap), 0);
            }
            else
            {
                coord.y += (uBGConfig[uCurBG].MapOffset >> 9);
                coord.y = mod(coord.y, 256);
                icoord = ivec2(coord * uScaleFactor);
                oColor = texelFetch(CaptureTex, ivec3(icoord, cap>>2), 0);
            }
            return;
        }
    }

    oColor = GetBGLayerPixel(uCurBG, coord);
}
