/*
    Copyright 2016-2017 StapleButter

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <windows.h>
#include "NDS.h"
#include "GPU.h"


#define VERSION "0.1"


HINSTANCE instance;
HWND melon;
BITMAPV4HEADER bmp;
bool quit;

bool touching;


LRESULT CALLBACK derpo(HWND window, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CLOSE:
        printf("close\n");
        {
            FILE* f = fopen("debug/wram.bin", "wb");
            if (f)
            {
                for (u32 i = 0x37F8000; i < 0x3808000; i+=4)
                {
                    u32 blarg = NDS::ARM7Read32(i);
                    fwrite(&blarg, 4, 1, f);
                }
                fclose(f);
            }
            f = fopen("debug/arm7vram.bin", "wb");
            if (f)
            {
                for (u32 i = 0x6000000; i < 0x6040000; i+=4)
                {
                    u32 blarg = NDS::ARM7Read32(i);
                    fwrite(&blarg, 4, 1, f);
                }
                fclose(f);
            }
            f = fopen("debug/mainram.bin", "wb");
            if (f)
            {
                for (u32 i = 0x2000000; i < 0x2400000; i+=4)
                {
                    u32 blarg = NDS::ARM9Read32(i);
                    fwrite(&blarg, 4, 1, f);
                }
                fclose(f);
            }
        }
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        switch (wparam)
        {
        case VK_RETURN: NDS::PressKey(3); break;
        case VK_SPACE:  NDS::PressKey(2); break;
        case VK_UP:     NDS::PressKey(6); break;
        case VK_DOWN:   NDS::PressKey(7); break;
        case VK_LEFT:   NDS::PressKey(5); break;
        case VK_RIGHT:  NDS::PressKey(4); break;
        case 'A': NDS::PressKey(0); break;
        case 'B': NDS::PressKey(1); break;
        case 'D': NDS::debug(0); break;
        }
        return 0;

    case WM_KEYUP:
        switch (wparam)
        {
        case VK_RETURN: NDS::ReleaseKey(3); break;
        case VK_SPACE:  NDS::ReleaseKey(2); break;
        case VK_UP:     NDS::ReleaseKey(6); break;
        case VK_DOWN:   NDS::ReleaseKey(7); break;
        case VK_LEFT:   NDS::ReleaseKey(5); break;
        case VK_RIGHT:  NDS::ReleaseKey(4); break;
        case 'A': NDS::ReleaseKey(0); break;
        case 'B': NDS::ReleaseKey(1); break;
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (!touching)
        {
            s16 x = (s16)(lparam & 0xFFFF);
            s16 y = (s16)(lparam >> 16);

            y -= 192;
            if (x >= 0 && x < 256 && y >= 0 && y < 192)
            {
                NDS::TouchScreen(x, y);
                NDS::PressKey(16+6);
                touching = true;
            }
        }
        return 0;

    case WM_LBUTTONUP:
    case WM_NCLBUTTONUP:
        if (touching)
        {
            NDS::ReleaseScreen();
            NDS::ReleaseKey(16+6);
            touching = false;
        }
        return 0;

    case WM_MOUSEMOVE:
        if (touching)
        {
            s16 x = (s16)(lparam & 0xFFFF);
            s16 y = (s16)(lparam >> 16);

            y -= 192;
            if (x >= 0 && x < 256 && y >= 0 && y < 192)
                NDS::TouchScreen(x, y);
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT partisocialiste;
            HDC dc = BeginPaint(window, &partisocialiste);

            SetDIBitsToDevice(dc, 0, 0, 256, 384, 0, 0, 0, 384, GPU::Framebuffer, (BITMAPINFO*)&bmp, DIB_RGB_COLORS);

            EndPaint(window, &partisocialiste);
        }
        return 0;
    }

    return DefWindowProc(window, msg, wparam, lparam);
}


int main()
{
    printf("melonDS version uh... 0.1??\n");
    printf("it's a DS emulator!!!\n");
    printf("http://melonds.kuribo64.net/\n");
    quit = false;
    touching = false;

    instance = GetModuleHandle(NULL);

    //SetThreadAffinityMask(GetCurrentThread(), 0x8);

    // god this shit sucks
    WNDCLASSEX shit;
    shit.cbSize = sizeof(shit);
    shit.style = CS_HREDRAW | CS_VREDRAW;
    shit.lpfnWndProc = derpo;
    shit.cbClsExtra = 0;
    shit.cbWndExtra = 0;
    shit.hInstance = instance;
    shit.hIcon = NULL;
    shit.hIconSm = NULL;
    shit.hCursor = NULL;
    shit.hbrBackground = (HBRUSH)(COLOR_WINDOWFRAME+1);
    shit.lpszMenuName = NULL;
    shit.lpszClassName = "v0ltmeters";
    RegisterClassEx(&shit);

    RECT rekt;
    rekt.left = 0;    rekt.top = 0;
    rekt.right = 256; rekt.bottom = 384;
    AdjustWindowRect(&rekt, WS_OVERLAPPEDWINDOW, FALSE);

    melon = CreateWindow("v0ltmeters",
                         "melonDS " VERSION,
                         WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         rekt.right-rekt.left, rekt.bottom-rekt.top,
                         NULL,
                         NULL,
                         instance,
                         NULL);

    ShowWindow(melon, SW_SHOW);

    // more sucky shit!
    memset(&bmp, 0, sizeof(bmp));
    bmp.bV4Size = sizeof(bmp);
    bmp.bV4Width = 256;
    bmp.bV4Height = -384;
    bmp.bV4Planes = 1;
    bmp.bV4BitCount = 16;
    bmp.bV4V4Compression = BI_RGB|BI_BITFIELDS;
    bmp.bV4RedMask = 0x001F;
    bmp.bV4GreenMask = 0x03E0;
    bmp.bV4BlueMask = 0x7C00;

    NDS::Init();

    u32 nframes = 0;
    u32 lasttick = GetTickCount();

    for (;;)
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                quit = true;
                break;
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (quit) break;

        NDS::RunFrame();

        //HDC dc = GetDC(melon);
        //SetDIBitsToDevice(dc, 0, 0, 256, 384, 0, 0, 0, 384, GPU::Framebuffer, (BITMAPINFO*)&bmp, DIB_RGB_COLORS);
        InvalidateRect(melon, NULL, false);
        UpdateWindow(melon);

        nframes++;
        if (nframes >= 30)
        {
            u32 tick = GetTickCount();
            u32 diff = tick - lasttick;
            lasttick = tick;

            u32 fps = (nframes * 1000) / diff;
            nframes = 0;

            char melontitle[100];
            sprintf(melontitle, "melonDS " VERSION " | %d FPS", fps);
            SetWindowText(melon, melontitle);
        }
    }

    NDS::DeInit();

    return 0;
}
