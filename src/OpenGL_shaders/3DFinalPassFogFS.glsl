#version 140

uniform sampler2D DepthBuffer;
uniform sampler2D AttrBuffer;

layout(std140) uniform uConfig
{
    vec2 uScreenSize;
    int uDispCnt;
    vec4 uToonColors[32];
    vec4 uEdgeColors[8];
    vec4 uFogColor;
    float uFogDensity[34];
    int uFogOffset;
    int uFogShift;
};

out vec4 oColor;

vec4 CalculateFog(float depth)
{
    int idepth = int(depth * 16777216.0);
    int densityid, densityfrac;

    if (idepth < uFogOffset)
    {
        densityid = 0;
        densityfrac = 0;
    }
    else
    {
        uint udepth = uint(idepth);
        udepth -= uint(uFogOffset);
        udepth = (udepth >> 2) << uint(uFogShift);

        densityid = int(udepth >> 17);
        if (densityid >= 32)
        {
            densityid = 32;
            densityfrac = 0;
        }
        else
        densityfrac = int(udepth & uint(0x1FFFF));
    }

    float density = mix(uFogDensity[densityid], uFogDensity[densityid+1], float(densityfrac)/131072.0);

    return vec4(density, density, density, density);
}

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);

    vec4 ret = vec4(0,0,0,0);
    vec4 depth = texelFetch(DepthBuffer, coord, 0);
    vec4 attr = texelFetch(AttrBuffer, coord, 0);

    if (attr.b != 0) ret = CalculateFog(depth.r);

    oColor = ret;
}
