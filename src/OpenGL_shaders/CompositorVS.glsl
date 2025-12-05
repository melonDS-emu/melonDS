#version 140

uniform int uScaleFactor;

in vec2 vPosition;

smooth out vec4 fTexcoord;

void main()
{
    gl_Position = vec4((vPosition * 2) - 1, 0, 1);
    fTexcoord.xy = vPosition * vec2(256, 192);
    fTexcoord.zw = fTexcoord.xy * uScaleFactor;
}
