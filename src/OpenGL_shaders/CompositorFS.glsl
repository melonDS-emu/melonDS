#version 140

uniform sampler2D _3DTex;
uniform sampler2DArray LayerTex;

struct sScanline
{
    ivec2 BGOffset[4];
    ivec4 BGRotscale[2];
    //bvec4 BGEnable;
    //ivec4 BGAssign;
    int BackColor;
    //bool BG03D;
};

layout(std140) uniform uScanlineConfig
{
    sScanline uScanline[192];
};

layout(std140) uniform uCompositorConfig
{
    ivec4 uBGPrio;
    bool uEnableOBJ;
};

//uniform int uScaleFactor;

smooth in vec2 fTexcoord;

out vec4 oColor;

ivec3 ConvertColor(int col)
{
    ivec3 ret;
    ret.r = (col & 0x1F) << 1;
    ret.g = ((col & 0x3E0) >> 4) | (col >> 15);
    ret.b = (col & 0x7C00) >> 9;
    return ret;
}

vec4 CompositeLayers()
{
    ivec2 coord = ivec2(fTexcoord);
    int line = coord.y;
    ivec4 col1 = ivec4(ConvertColor(uScanline[line].BackColor), 0x20);
    ivec4 col2 = ivec4(0);

    vec4 bgcol[4];
    for (int bg = 0; bg < 4; bg++)
        bgcol[bg] = texelFetch(LayerTex, ivec3(coord, bg), 0);

    for (int prio = 3; prio >= 0; prio--)
    {
        for (int bg = 3; bg >= 0; bg--)
        {
            if ((uBGPrio[bg] == prio) && (bgcol[bg].a > 0))
            {
                col2 = col1;
                col1 = ivec4(bgcol[bg] * vec4(63,63,63,255));
            }
        }

        // TODO do sprite layer here
    }

    // TODO compositor.
    return vec4(vec3(col1.rgb) / vec3(63,63,63), 1);
}

void main()
{
    oColor = CompositeLayers();
}
