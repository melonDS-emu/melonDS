#version 140

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

in vec2 vPosition;
in vec2 vTexcoord;

smooth out vec4 fTexcoord;

void main()
{
    vec4 fpos;
    fpos.xy = vPosition;
    fpos.z = 0.0;
    fpos.w = 1.0;

    gl_Position = fpos;
    fTexcoord.xy = vTexcoord;
    fTexcoord.zw = vTexcoord * uScaleFactor;
}
