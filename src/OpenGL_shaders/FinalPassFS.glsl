#version 140

uniform sampler2D MainInputTexA;
uniform sampler2D MainInputTexB;
uniform sampler2D AuxInputTex;
uniform sampler2DArray AuxCapInputTex;

layout(std140) uniform uFinalPassConfig
{
    bvec4 uScreenSwap[48]; // one bool per scanline
    int uScaleFactor;
    int uAuxCapBlock;
    int uDispModeA;
    int uDispModeB;
    int uBrightModeA;
    int uBrightModeB;
    int uBrightFactorA;
    int uBrightFactorB;
};

smooth in vec4 fTexcoord;

out vec4 oTopColor;
out vec4 oBottomColor;

ivec3 MasterBrightness(ivec3 color, int brightmode, int evy)
{
    if (brightmode == 1)
    {
        // up
        color += (((0x3F - color) * evy) >> 4);
    }
    else if (brightmode == 2)
    {
        // down
        color -= (((color * evy) + 0xF) >> 4);
    }

    return color;
}

void main()
{
    ivec2 coord = ivec2(fTexcoord.zw);

    ivec4 col_main = ivec4(texelFetch(MainInputTexA, coord, 0) * vec4(63,63,63,31));
    ivec4 col_sub = ivec4(texelFetch(MainInputTexB, coord, 0) * vec4(63,63,63,31));

    ivec3 output_main, output_sub;

    // TODO not always sample those? (VRAM/FIFO)

    ivec4 col_aux;
    if (uAuxCapBlock != -1)
        col_aux = ivec4(texelFetch(AuxCapInputTex, ivec3(coord, uAuxCapBlock), 0) * vec4(63,63,63,31));
    else
        col_aux = ivec4(texelFetch(AuxInputTex, ivec2(fTexcoord.xy), 0) * vec4(63,63,63,31));

    if (uDispModeA == 0)
    {
        // screen disabled (white)
        output_main = ivec3(63, 63, 63);
    }
    else if (uDispModeA == 1)
    {
        // BG/OBJ layers
        output_main = col_main.rgb;
    }
    else
    {
        // VRAM display / mainmem FIFO
        output_main = col_aux.rgb;
    }

    if (uDispModeB == 0)
    {
        // screen disabled (white)
        output_sub = ivec3(63, 63, 63);
    }
    else
    {
        // BG/OBJ layers
        output_sub = col_sub.rgb;
    }

    if (uDispModeA != 0)
        output_main = MasterBrightness(output_main, uBrightModeA, uBrightFactorA);
    if (uDispModeB != 0)
        output_sub = MasterBrightness(output_sub, uBrightModeB, uBrightFactorB);

    int scry = int(fTexcoord.y);
    bvec4 swap = uScreenSwap[scry >> 3];
    bool swapbit = swap[scry & 0x3];

    if (swapbit)
    {
        oTopColor = vec4(vec3(output_sub) / 63.0, 1.0);
        oBottomColor = vec4(vec3(output_main) / 63.0, 1.0);
    }
    else
    {
        oTopColor = vec4(vec3(output_main) / 63.0, 1.0);
        oBottomColor = vec4(vec3(output_sub) / 63.0, 1.0);
    }
}
