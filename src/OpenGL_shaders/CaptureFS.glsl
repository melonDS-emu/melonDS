#version 140

uniform sampler2D InputTexA;
uniform sampler2DArray InputTexB;

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

smooth in vec4 fTexcoord;

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
    vec2 coordA = fTexcoord.xy;
    vec3 coordB = vec3(fTexcoord.zw, uSrcBLayer);
    ivec4 cap_out;

    if (uDstMode == 0)
    {
        // source A only
        cap_out = ivec4(texture(InputTexA, coordA) * vec4(63,63,63,31));
    }
    else if (uDstMode == 1)
    {
        // source B only
        cap_out = ivec4(texture(InputTexB, coordB) * vec4(63,63,63,31));
    }
    else
    {
        // sources A and B blended
        ivec4 srcA = ivec4(texture(InputTexA, coordA) * vec4(63,63,63,31));
        ivec4 srcB = ivec4(texture(InputTexB, coordB) * vec4(63,63,63,31));

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
