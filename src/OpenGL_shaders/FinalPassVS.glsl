#version 140

in vec2 vPosition;
in vec2 vTexcoord;

smooth out vec2 fTexcoord;

void main()
{
    vec4 fpos;
    fpos.xy = vPosition;
    fpos.z = 0.0;
    fpos.w = 1.0;

    gl_Position = fpos;
    fTexcoord = vTexcoord;
}
