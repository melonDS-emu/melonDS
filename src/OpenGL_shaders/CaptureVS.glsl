#version 140

layout(std140) uniform uCaptureConfig
{
    ivec2 uCaptureSize;
    int uScaleFactor;
    int uSrcAOffset;
    int uSrcBLayer;
    int uSrcBOffset;
    int uDstOffset;
    int uDstMode;
    ivec2 uBlendFactors;
};

in ivec2 vPosition;
in ivec2 vTexcoord;

smooth out vec4 fTexcoord;

void main()
{
    vec2 pos = vec2(vPosition) / uCaptureSize.xx;
    gl_Position = vec4((pos * 2) - 1, 0, 1);
    fTexcoord.xy = (vec2(vTexcoord) + vec2(uSrcAOffset, 0)) / vec2(256,192);
    fTexcoord.zw = (vec2(vTexcoord) + vec2(0, uSrcBOffset)) / 256;
}
