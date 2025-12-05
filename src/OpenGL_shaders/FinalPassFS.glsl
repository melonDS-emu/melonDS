#version 140

uniform sampler2D MainInputTexA;
uniform sampler2D MainInputTexB;
uniform sampler2D AuxInputTex;

layout(std140) uniform uFinalPassConfig
{
    bvec4 uScreenSwap[48]; // one bool per scanline
    int uScaleFactor;
    int uAuxScaleFactor;
    int uDispModeA;
    int uDispModeB;
    int uBrightModeA;
    int uBrightModeB;
    int uBrightFactorA;
    int uBrightFactorB;
};

smooth in vec2 fTexcoord;

out vec4 oTopColor;
out vec4 oBottomColor;
out vec4 oCaptureColor;

ivec3 MasterBrightness(ivec3 color, int brightmode, int evy)
{
    if (brightmode == 1)
    {
        // up
        color += (((0x3F - color) * evy) >> 4);
    }
    else if (brightmode == 2)
    {
        // down
        color -= (((color * evy) + 0xF) >> 4);
    }

    return color;
}

void main()
{
    ivec2 coord = ivec2(fTexcoord * uScaleFactor);

    ivec4 col_main = ivec4(texelFetch(MainInputTexA, coord, 0) * vec4(63,63,63,31));
    ivec4 col_sub = ivec4(texelFetch(MainInputTexB, coord, 0) * vec4(63,63,63,31));

    ivec3 output_main, output_sub;

    // TODO not always sample those? (VRAM/FIFO)

    /*int capblock = 0;
    if (dispmode_main != 2)
        capblock = ((uCaptureReg >> 26) & 0x3);
    //capblock += int(fTexcoord.y / 64);
    capblock = (capblock & 0x3) | (((attrib_main.r >> 18) & 0x3) << 2);*/

    ivec4 col_aux = ivec4(texelFetch(AuxInputTex, ivec2(fTexcoord * uAuxScaleFactor), 0) * vec4(63,63,63,31));

    if (uDispModeA == 0)
    {
        // screen disabled (white)
        output_main = ivec3(63, 63, 63);
    }
    else if (uDispModeA == 1)
    {
        // BG/OBJ layers
        output_main = col_main.rgb;
    }
    else
    {
        // VRAM display / mainmem FIFO
        output_main = col_aux.rgb;
    }

    if (uDispModeB == 0)
    {
        // screen disabled (white)
        output_sub = ivec3(63, 63, 63);
    }
    else
    {
        // BG/OBJ layers
        output_sub = col_sub.rgb;
    }

    if (uDispModeA != 0)
        output_main = MasterBrightness(output_main, uBrightModeA, uBrightFactorA);
    if (uDispModeB != 0)
        output_sub = MasterBrightness(output_sub, uBrightModeB, uBrightFactorB);

    int scry = int(fTexcoord.y);
    bvec4 swap = uScreenSwap[scry >> 3];
    bool swapbit = swap[scry & 0x3];

    if (swapbit)
    {
        oTopColor = vec4(vec3(output_sub) / 63.0, 1.0);
        oBottomColor = vec4(vec3(output_main) / 63.0, 1.0);
    }
    else
    {
        oTopColor = vec4(vec3(output_main) / 63.0, 1.0);
        oBottomColor = vec4(vec3(output_sub) / 63.0, 1.0);
    }

    /*if ((uCaptureReg & (1<<31)) != 0)
    {
        int capwidth, capheight;
        int capsize = (uCaptureReg >> 20) & 0x3;
        if (capsize == 0)
        {
            capwidth = 128;
            capheight = 128;
        }
        else
        {
            capwidth = 256;
            capheight = 64 * capsize;
        }

        if ((fTexcoord.x < capwidth) && (fTexcoord.y < capheight))
        {
            ivec4 srcA;
            if ((uCaptureReg & (1<<24)) != 0)
                srcA = col_3d;
            else
                srcA = col_main;

            ivec4 srcB;
            if ((uCaptureReg & (1<<25)) != 0)
                srcB = col_fifo;
            else
                srcB = col_vram;

            ivec4 cap_out;
            int srcsel = (uCaptureReg >> 29) & 0x3;
            if (srcsel == 0)
                cap_out = srcA;
            else if (srcsel == 1)
                cap_out = srcB;
            else
            {
                int eva = min(uCaptureReg & 0x1F, 16);
                int evb = min((uCaptureReg >> 8) & 0x1F, 16);

                int aa = (srcA.a > 0) ? 1 : 0;
                int ab = (srcB.a > 0) ? 1 : 0;

                cap_out.rgb = ((srcA.rgb * aa * eva) + (srcB.rgb * ab * evb) + 0x8) >> 4;
                cap_out.rgb = min(cap_out.rgb, 0x3F);
                cap_out.a = (eva>0 ? aa : 0) | (evb>0 ? ab : 0);
            }

            oCaptureColor = vec4(vec3(cap_out.bgr) / 63.0, (cap_out.a>0) ? 1.0 : 0.0);
        }
    }*/
}
