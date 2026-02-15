#version 140

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

in uvec4 vPosition;
in uvec4 vColor;
in ivec2 vTexcoord;
in ivec3 vPolygonAttr;

smooth out vec4 fColor;
smooth out vec2 fTexcoord;
flat out ivec3 fPolygonAttr;

#ifdef WBuffer
smooth out float fZ;
#endif

void main()
{
    int attr = vPolygonAttr.x;
    int zshift = (attr >> 16) & 0x1F;

    vec4 fpos;
    fpos.xy = (((vec2(vPosition.xy) ) * 2.0) / uScreenSize) - 1.0;
#ifdef WBuffer
    fZ = float(vPosition.z << zshift) / 16777216.0;
    fpos.z = 0;
#else
    fpos.z = (float(vPosition.z << zshift) / 8388608.0) - 1.0;
#endif
    fpos.w = float(vPosition.w) / 65536.0f;
    fpos.xyz *= fpos.w;

    int texwidth = vPolygonAttr.z & 0xFFFF;
    int texheight = (vPolygonAttr.z >> 16) & 0xFFFF;
    vec2 texfactor = 1.0 / (16 * vec2(texwidth, texheight));

    vec2 texcoord = vec2(vTexcoord);
    int capyoffset = vPolygonAttr.y >> 16;
    int attrz = 0;
    if (capyoffset != -1)
    {
        texcoord.y += capyoffset;
        if (texwidth == 128)
            attrz = 1;
        else
            attrz = 2;
    }

    fColor = vec4(vColor) / vec4(255.0,255.0,255.0,31.0);
    fTexcoord = texcoord * texfactor;
    fPolygonAttr = ivec3(vPolygonAttr.x, vPolygonAttr.y & 0xFFFF, attrz);

    gl_Position = fpos;
}
