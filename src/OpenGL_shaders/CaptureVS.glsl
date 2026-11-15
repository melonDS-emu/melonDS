#version 140

layout(std140) uniform ubCaptureConfig
{
    vec2 uInvCaptureSize;
    int uSrcALayer;
    int uSrcBLayer;
    int uSrcBOffset;
    int uDstMode;
    ivec2 uBlendFactors;
    vec4 uSrcAOffset[48];
    float uSrcBColorFactor;
};

in ivec2 vPosition;
in ivec2 vTexcoord;

smooth out vec4 fTexcoord;

void main()
{
    vec2 pos = vec2(vPosition) * uInvCaptureSize.xx;
    gl_Position = vec4((pos * 2) - 1, 0, 1);
    fTexcoord.xy = vec2(vTexcoord) / vec2(256,192);
    fTexcoord.z = vTexcoord.y;
    fTexcoord.w = float(vTexcoord.y + uSrcBOffset) / 256.0;
}
