#version 140

uniform sampler2DArray InputTex;
uniform int uInputLayer;

smooth in vec2 fTexcoord;

out vec4 oColor;

void main()
{
    ivec4 col = ivec4(texture(InputTex, vec3(fTexcoord, uInputLayer)) * 255.0);
    oColor.rgb = vec3(col.rgb >> 3) / 31.0;
    oColor.a = (col.a>0) ? 1 : 0;
}
