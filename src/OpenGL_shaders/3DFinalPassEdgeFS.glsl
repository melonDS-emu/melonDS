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

// make up for crapo zbuffer precision
bool isless(float a, float b)
{
    return a < b;

    // a < b
    float diff = a - b;
    return diff < (256.0 / 16777216.0);
}

bool isgood(vec4 attr, float depth, int refPolyID, float refDepth)
{
    int polyid = int(attr.r * 63.0);

    if (polyid != refPolyID && isless(refDepth, depth))
    return true;

    return false;
}

void main()
{
    ivec2 coord = ivec2(gl_FragCoord.xy);
    int scale = 1;//int(uScreenSize.x / 256);

    vec4 ret = vec4(0,0,0,0);
    vec4 depth = texelFetch(DepthBuffer, coord, 0);
    vec4 attr = texelFetch(AttrBuffer, coord, 0);

    int polyid = int(attr.r * 63.0);

    if (attr.g != 0)
    {
        vec4 depthU = texelFetch(DepthBuffer, coord + ivec2(0,-scale), 0);
        vec4 attrU = texelFetch(AttrBuffer, coord + ivec2(0,-scale), 0);
        vec4 depthD = texelFetch(DepthBuffer, coord + ivec2(0,scale), 0);
        vec4 attrD = texelFetch(AttrBuffer, coord + ivec2(0,scale), 0);
        vec4 depthL = texelFetch(DepthBuffer, coord + ivec2(-scale,0), 0);
        vec4 attrL = texelFetch(AttrBuffer, coord + ivec2(-scale,0), 0);
        vec4 depthR = texelFetch(DepthBuffer, coord + ivec2(scale,0), 0);
        vec4 attrR = texelFetch(AttrBuffer, coord + ivec2(scale,0), 0);

        if (isgood(attrU, depthU.r, polyid, depth.r) ||
        isgood(attrD, depthD.r, polyid, depth.r) ||
        isgood(attrL, depthL.r, polyid, depth.r) ||
        isgood(attrR, depthR.r, polyid, depth.r))
        {
            // mark this pixel!

            ret.rgb = uEdgeColors[polyid >> 3].rgb;

            // this isn't quite accurate, but it will have to do
            if ((uDispCnt & (1<<4)) != 0)
            ret.a = 0.5;
            else
            ret.a = 1;
        }
    }

    oColor = ret;
}
