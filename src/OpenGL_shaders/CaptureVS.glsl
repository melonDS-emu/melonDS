#version 140

layout(std140) uniform uCaptureConfig
{
    ivec2 uCaptureSize;
    int uScaleFactor;
    int uSrcBOffset;
    int uDstOffset;
    int uDstMode;
    ivec2 uBlendFactors;
};

in ivec2 vPosition;
in ivec2 vTexcoord;

smooth out vec2 fTexcoord;

void main()
{
    vec2 pos = vec2(vPosition) / 256;
    gl_Position = vec4((pos * 2) - 1, 0, 1);
    fTexcoord = vec2(vTexcoord);
}
