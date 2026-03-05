#version 140

struct sBGConfig
{
    ivec2 Size;
    int Type;
    int PalOffset;
    int TileOffset;
    int MapOffset;
    bool Clamp;
};

layout(std140) uniform ubBGConfig
{
    int uVRAMMask;
    sBGConfig uBGConfig[4];
};

uniform int uCurBG;

in vec2 vPosition;

smooth out vec2 fTexcoord;

void main()
{
    gl_Position = vec4((vPosition * 2) - 1, 0, 1);
    fTexcoord = vPosition * vec2(uBGConfig[uCurBG].Size);
}
