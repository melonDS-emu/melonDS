#version 140

layout(std140) uniform ubFinalPassConfig
{
    bvec4 uScreenSwap[48]; // one bool per scanline
    int uScaleFactor;
    int uAuxLayer;
    int uDispModeA;
    int uDispModeB;
    int uBrightModeA;
    int uBrightModeB;
    int uBrightFactorA;
    int uBrightFactorB;
    float uAuxColorFactor;
};

in vec2 vPosition;

smooth out vec3 fTexcoord;

void main()
{
    gl_Position = vec4(vPosition, 0, 1);
    fTexcoord = (vPosition.xyy + 1) * vec3(0.5, 0.5, 0.375);
}
