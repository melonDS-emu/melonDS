#version 140

uniform sampler2DArray BGLayerTex;

struct sScanline
{
    ivec2 BGOffset[4];
    ivec4 BGRotscale[2];
};

layout(std140) uniform uScanlineConfig
{
    sScanline uScanline[192];
};

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

    // TODO non-text coords
    // TODO also provision for hi-res capture
    coord = vec2(uScanline[line].BGOffset[uCurBG]) + fTexcoord;

    oColor = GetBGLayerPixel(uCurBG, coord);
}
