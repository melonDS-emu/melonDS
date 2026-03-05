#version 140

in vec2 vPosition;

void main()
{
    // heh
    gl_Position = vec4(vPosition, 0.0, 1.0);
}
