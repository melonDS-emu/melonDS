#version 140

uniform sampler2D InputTexA;
uniform sampler2D InputTexB;

layout(std140) uniform uCaptureConfig
{
    ivec2 uCaptureSize;
    int uScaleFactor;
    int uSrcBOffset;
    int uDstOffset;
    int uDstMode;
    ivec2 uBlendFactors;
};

smooth in vec2 fTexcoord;

out vec4 oColor;

ivec4 ConvertColor(int col)
{
    ivec4 ret;
    ret.r = (col & 0x1F) << 1;
    ret.g = (col & 0x3E0) >> 4;
    ret.b = (col & 0x7C00) >> 9;
    ret.a = col >> 15;
    return ret;
}

void main()
{
    vec2 coord;
    if (uCaptureSize.x == 128)
    {
        coord.x = mod(fTexcoord.x, 128);
        coord.y = int(fTexcoord.x / 128) + (fTexcoord.y * 2);
    }
    else
        coord = fTexcoord;

    ivec2 coordA = ivec2(coord * uScaleFactor);
    ivec2 coordB = ivec2(coord);
    coordB.y = (coordB.y + uSrcBOffset) & 0xFF;
    ivec4 cap_out;

    if (uDstMode == 0)
    {
        // source A only
        cap_out = ivec4(texelFetch(InputTexA, coordA, 0) * vec4(63,63,63,31));
    }
    else if (uDstMode == 1)
    {
        // source B only
        cap_out = ivec4(texelFetch(InputTexB, coordB, 0) * vec4(63,63,63,31));
    }
    else
    {
        // sources A and B blended
        ivec4 srcA = ivec4(texelFetch(InputTexA, coordA, 0) * vec4(63,63,63,31));
        ivec4 srcB = ivec4(texelFetch(InputTexB, coordB, 0) * vec4(63,63,63,31));

        int eva = uBlendFactors[0];
        int evb = uBlendFactors[1];

        int aa = (srcA.a > 0) ? 1 : 0;
        int ab = (srcB.a > 0) ? 1 : 0;

        cap_out.rgb = ((srcA.rgb * aa * eva) + (srcB.rgb * ab * evb) + 0x8) >> 4;
        cap_out.rgb = min(cap_out.rgb, 0x3F);
        cap_out.a = (eva>0 ? aa : 0) | (evb>0 ? ab : 0);
    }

    oColor = vec4(vec3(cap_out.rgb) / 63.0, (cap_out.a>0) ? 1.0 : 0.0);
}
