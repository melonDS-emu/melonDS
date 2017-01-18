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
#include "NDS.h"
#include "GPU.h"
#include "GPU2D.h"


GPU2D::GPU2D(u32 num)
{
    Num = num;
}

GPU2D::~GPU2D()
{
}

void GPU2D::Reset()
{
    //
}

void GPU2D::SetFramebuffer(u16* buf)
{
    // framebuffer is 256x192 16bit.
    // might eventually support other framebuffer types/sizes
    Framebuffer = buf;
}


u8 GPU2D::Read8(u32 addr)
{
    printf("!! GPU2D READ8 %08X\n", addr);
}

u16 GPU2D::Read16(u32 addr)
{
    switch (addr & 0x00000FFF)
    {
        //
    }
}

u32 GPU2D::Read32(u32 addr)
{
    switch (addr & 0x00000FFF)
    {
        //
    }

    return Read16(addr) | (Read16(addr+2) << 16);
}

void GPU2D::Write8(u32 addr, u8 val)
{
    printf("!! GPU2D WRITE8 %08X %02X\n", addr, val);
}

void GPU2D::Write16(u32 addr, u16 val)
{
    switch (addr & 0x00000FFF)
    {
        //
    }
}

void GPU2D::Write32(u32 addr, u32 val)
{
    switch (addr & 0x00000FFF)
    {
        //
    }

    Write16(addr, val&0xFFFF);
    Write16(addr+2, val>>16);
}


void GPU2D::DrawScanline(u32 line)
{
    //
}
