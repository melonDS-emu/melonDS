#version 140

in vec2 vPosition;

smooth out vec2 fTexcoord;

void main()
{
    gl_Position = vec4((vPosition * 2) - 1, 0, 1);
    fTexcoord = vPosition;
}
