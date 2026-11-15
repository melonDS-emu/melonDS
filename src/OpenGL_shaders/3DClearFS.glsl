#version 140

uniform uvec4 uColor;
uniform uint uOpaquePolyID;
uniform uint uFogFlag;

out vec4 oColor;
out vec4 oAttr;

void main()
{
    oColor = vec4(uColor).rgba / 31.0;
    oAttr.r = float(uOpaquePolyID) / 63.0;
    oAttr.g = 0;
    oAttr.b = float(uFogFlag);
    oAttr.a = 1;
}
