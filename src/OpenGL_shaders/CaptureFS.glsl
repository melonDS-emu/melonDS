#version 140

uniform sampler2D InputTexA;
uniform sampler2DArray InputTexB;

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

smooth in vec4 fTexcoord;

out vec4 oColor;

float GetSrcAPos(float line)
{
    int iline = int(line);
    float a = uSrcAOffset[iline>>2][iline&0x3];
    iline++;
    float b = uSrcAOffset[iline>>2][iline&0x3];
    return mix(a, b, fract(line));
}

void main()
{
    vec2 coordA = fTexcoord.xy;
    vec3 coordB = vec3(fTexcoord.xw, uSrcBLayer);
    ivec4 cap_out;

    // apply scroll for 3D layer, if we're capturing that
    if (uSrcALayer == 1)
    {
        int line = int(fTexcoord.z);
        coordA.x += uSrcAOffset[line>>2][line&0x3];
        //coordA.x += GetSrcAPos(fTexcoord.z);
    }

    if (uDstMode == 0)
    {
        // source A only
        cap_out = ivec4(texture(InputTexA, coordA) * 255.0);
    }
    else if (uDstMode == 1)
    {
        // source B only
        cap_out = ivec4(texture(InputTexB, coordB) * uSrcBColorFactor);
    }
    else
    {
        // sources A and B blended
        ivec4 srcA = ivec4(texture(InputTexA, coordA) * 255.0) >> 3;
        ivec4 srcB = ivec4(texture(InputTexB, coordB) * uSrcBColorFactor) >> 3;

        int eva = uBlendFactors[0];
        int evb = uBlendFactors[1];

        int aa = (srcA.a > 0) ? 1 : 0;
        int ab = (srcB.a > 0) ? 1 : 0;

        cap_out.rgb = ((srcA.rgb * aa * eva) + (srcB.rgb * ab * evb) + 0x8) >> 4;
        cap_out.rgb = min(cap_out.rgb, 0x1F) << 3;
        cap_out.a = (eva>0 ? aa : 0) | (evb>0 ? ab : 0);
    }

    oColor = vec4(vec3(cap_out.rgb) / 255.0, (cap_out.a>0) ? 1.0 : 0.0);
}
