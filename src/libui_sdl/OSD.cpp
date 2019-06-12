/*
    Copyright 2016-2019 Arisotura

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
#include <string.h>
#include <deque>
#include <SDL2/SDL.h>
#include "../types.h"

#include "libui/ui.h"
#include "../OpenGLSupport.h"

#include "OSD.h"
#include "font.h"

#include "PlatformConfig.h"

extern int WindowWidth, WindowHeight;

namespace OSD
{

const u32 kOSDMargin = 6;

struct Item
{
    Uint32 Timestamp;
    char Text[256];
    u32 Color;

    u32 Width, Height;
    u32* Bitmap;

    bool DrawBitmapLoaded;
    uiDrawBitmap* DrawBitmap;

    bool GLTextureLoaded;
    GLuint GLTexture;

};

std::deque<Item> ItemQueue;

GLint uOSDPos, uOSDSize;
GLuint OSDVertexArray;
GLuint OSDVertexBuffer;

volatile bool Rendering;


bool Init(bool opengl)
{
    if (opengl)
    {
        GLuint prog; glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&prog);
        uOSDPos = glGetUniformLocation(prog, "uOSDPos");
        uOSDSize = glGetUniformLocation(prog, "uOSDSize");

        float vertices[6*2] =
        {
            0, 0,
            1, 1,
            1, 0,
            0, 0,
            0, 1,
            1, 1
        };

        glGenBuffers(1, &OSDVertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, OSDVertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glGenVertexArrays(1, &OSDVertexArray);
        glBindVertexArray(OSDVertexArray);
        glEnableVertexAttribArray(0); // position
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)(0));
    }

    return true;
}

void DeInit(bool opengl)
{
    for (auto it = ItemQueue.begin(); it != ItemQueue.end(); )
    {
        Item& item = *it;

        if (item.DrawBitmapLoaded && item.DrawBitmap) uiDrawFreeBitmap(item.DrawBitmap);
        if (item.GLTextureLoaded && opengl) glDeleteTextures(1, &item.GLTexture);
        if (item.Bitmap) delete[] item.Bitmap;

        it = ItemQueue.erase(it);
    }
}


int FindBreakPoint(const char* text, int i)
{
    // i = character that went out of bounds

    for (int j = i; j >= 0; j--)
    {
        if (text[j] == ' ')
            return j;
    }

    return i;
}

void LayoutText(const char* text, u32* width, u32* height, int* breaks)
{
    u32 w = 0;
    u32 h = 14;
    u32 totalw = 0;
    u32 maxw = WindowWidth - (kOSDMargin*2);
    int lastbreak = -1;
    int numbrk = 0;
    u16* ptr;

    memset(breaks, 0, sizeof(int)*64);

    for (int i = 0; text[i] != '\0'; )
	{
	    int glyphsize;
		if (text[i] == ' ')
		{
			glyphsize = 6;
		}
		else
        {
            u32 ch = text[i];
            if (ch < 0x10 || ch > 0x7E) ch = 0x7F;

            ptr = &font[(ch-0x10) << 4];
            glyphsize = ptr[0];
            if (!glyphsize) glyphsize = 6;
            else            glyphsize += 2; // space around the character
        }

		w += glyphsize;
		if (w > maxw)
        {
            // wrap shit as needed
            if (text[i] == ' ')
            {
                if (numbrk >= 64) break;
                breaks[numbrk++] = i;
                i++;
            }
            else
            {
                int brk = FindBreakPoint(text, i);
                if (brk != lastbreak) i = brk;

                if (numbrk >= 64) break;
                breaks[numbrk++] = i;

                lastbreak = brk;
            }

            w = 0;
            h += 14;
        }
        else
            i++;

        if (w > totalw) totalw = w;
    }

    *width = totalw;
    *height = h;
}

u32 RainbowColor(u32 inc)
{
    // inspired from Acmlmboard

    if      (inc < 100) return 0xFFFF9B9B + (inc << 8);
    else if (inc < 200) return 0xFFFFFF9B - ((inc-100) << 16);
    else if (inc < 300) return 0xFF9BFF9B + (inc-200);
    else if (inc < 400) return 0xFF9BFFFF - ((inc-300) << 8);
    else if (inc < 500) return 0xFF9B9BFF + ((inc-400) << 16);
    else                return 0xFFFF9BFF - (inc-500);
}

void RenderText(u32 color, const char* text, Item* item)
{
    u32 w, h;
    int breaks[64];

    bool rainbow = (color == 0);
    u32 rainbowinc = ((text[0] * 17) + (SDL_GetTicks() * 13)) % 600;

    color |= 0xFF000000;
    const u32 shadow = 0xE0000000;

    LayoutText(text, &w, &h, breaks);

    item->Width = w;
    item->Height = h;
    item->Bitmap = new u32[w*h];
    memset(item->Bitmap, 0, w*h*sizeof(u32));

    u32 x = 0, y = 1;
    u32 maxw = WindowWidth - (kOSDMargin*2);
    int curline = 0;
    u16* ptr;

    for (int i = 0; text[i] != '\0'; )
	{
	    int glyphsize;
		if (text[i] == ' ')
		{
			x += 6;
		}
		else
        {
            u32 ch = text[i];
            if (ch < 0x10 || ch > 0x7E) ch = 0x7F;

            ptr = &font[(ch-0x10) << 4];
            int glyphsize = ptr[0];
            if (!glyphsize) x += 6;
            else
            {
                x++;

                if (rainbow)
                {
                    color = RainbowColor(rainbowinc);
                    rainbowinc = (rainbowinc + 30) % 600;
                }

                // draw character
                for (int cy = 0; cy < 12; cy++)
                {
                    u16 val = ptr[4+cy];

                    for (int cx = 0; cx < glyphsize; cx++)
                    {
                        if (val & (1<<cx))
                            item->Bitmap[((y+cy) * w) + x+cx] = color;
                    }
                }

                x += glyphsize;
                x++;
            }
        }

		i++;
		if (breaks[curline] && i >= breaks[curline])
        {
            i = breaks[curline++];
            if (text[i] == ' ') i++;

            x = 0;
            y += 14;
        }
    }

    // shadow
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            u32 val;

            val = item->Bitmap[(y * w) + x];
            if ((val >> 24) == 0xFF) continue;

            if (x > 0)   val  = item->Bitmap[(y * w) + x-1];
            if (x < w-1) val |= item->Bitmap[(y * w) + x+1];
            if (y > 0)
            {
                if (x > 0)   val |= item->Bitmap[((y-1) * w) + x-1];
                val |= item->Bitmap[((y-1) * w) + x];
                if (x < w-1) val |= item->Bitmap[((y-1) * w) + x+1];
            }
            if (y < h-1)
            {
                if (x > 0)   val |= item->Bitmap[((y+1) * w) + x-1];
                val |= item->Bitmap[((y+1) * w) + x];
                if (x < w-1) val |= item->Bitmap[((y+1) * w) + x+1];
            }

            if ((val >> 24) == 0xFF)
                item->Bitmap[(y * w) + x] = shadow;
        }
    }
}


void AddMessage(u32 color, const char* text)
{
    if (!Config::ShowOSD) return;

    while (Rendering);

    Item item;

    item.Timestamp = SDL_GetTicks();
    strncpy(item.Text, text, 255); item.Text[255] = '\0';
    item.Color = color;
    item.Bitmap = NULL;

    item.DrawBitmapLoaded = false;
    item.GLTextureLoaded = false;

    ItemQueue.push_back(item);
}

void WindowResized(bool opengl)
{
    /*for (auto it = ItemQueue.begin(); it != ItemQueue.end(); )
    {
        Item& item = *it;

        if (item->DrawBitmapLoaded && item->DrawBitmap) uiDrawFreeBitmap(item->DrawBitmap);
        //if (item->GLTextureLoaded && opengl) glDeleteTextures(1, &item->GLTexture);

        item->DrawBitmapLoaded = false;
        item->GLTextureLoaded = false;

        if (item->Bitmap) delete[] item->Bitmap;

        it++;
    }*/
}

void Update(bool opengl, uiAreaDrawParams* params)
{
    if (!Config::ShowOSD)
    {
        Rendering = true;
        if (ItemQueue.size() > 0) DeInit(opengl);
        Rendering = false;
        return;
    }

    Rendering = true;

    Uint32 tick_now = SDL_GetTicks();
    Uint32 tick_min = tick_now - 2500;
    u32 y = kOSDMargin;

    if (opengl)
    {
        glBindBuffer(GL_ARRAY_BUFFER, OSDVertexBuffer);
        glBindVertexArray(OSDVertexArray);

        glActiveTexture(GL_TEXTURE0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    for (auto it = ItemQueue.begin(); it != ItemQueue.end(); )
    {
        Item& item = *it;

        if (item.Timestamp < tick_min)
        {
            if (item.DrawBitmapLoaded && item.DrawBitmap) uiDrawFreeBitmap(item.DrawBitmap);
            if (item.GLTextureLoaded && opengl) glDeleteTextures(1, &item.GLTexture);
            if (item.Bitmap) delete[] item.Bitmap;

            it = ItemQueue.erase(it);
            continue;
        }

        if (!item.Bitmap)
        {
            RenderText(item.Color, item.Text, &item);
        }

        if (opengl)
        {
            if (!item.GLTextureLoaded)
            {
                glGenTextures(1, &item.GLTexture);
                glBindTexture(GL_TEXTURE_2D, item.GLTexture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, item.Width, item.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, item.Bitmap);

                item.GLTextureLoaded = true;
            }

            glBindTexture(GL_TEXTURE_2D, item.GLTexture);
            glUniform2i(uOSDPos, kOSDMargin, y);
            glUniform2i(uOSDSize, item.Width, item.Height);
            glDrawArrays(GL_TRIANGLES, 0, 2*3);
        }
        else
        {
            if (!item.DrawBitmapLoaded)
            {
                item.DrawBitmap = uiDrawNewBitmap(params->Context, item.Width, item.Height, 1);
                uiDrawBitmapUpdate(item.DrawBitmap, item.Bitmap);

                item.DrawBitmapLoaded = true;
            }

            uiRect rc_src = {0, 0, item.Width, item.Height};
            uiRect rc_dst = {kOSDMargin, y, item.Width, item.Height};

            uiDrawBitmapDraw(params->Context, item.DrawBitmap, &rc_src, &rc_dst, 0);
        }

        y += item.Height;
        it++;
    }

    Rendering = false;
}

}
