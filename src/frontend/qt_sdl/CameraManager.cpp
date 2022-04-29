/*
    Copyright 2016-2022 melonDS team

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

#include "CameraManager.h"


CameraFrameDumper::CameraFrameDumper(QObject* parent) : QAbstractVideoSurface(parent)
{
}

bool CameraFrameDumper::present(const QVideoFrame& _frame)
{
    QVideoFrame frame(_frame);
    if (!frame.map(QAbstractVideoBuffer::ReadOnly))
        return false;
printf("FRAMEZORZ!! %d %d %d\n", frame.pixelFormat(), frame.isMapped(), frame.isReadable());
    //NDS::CamInputFrame(0, (u32*)frame.bits(), frame.width(), frame.height(), false);

    frame.unmap();

    return true;
}

QList<QVideoFrame::PixelFormat> CameraFrameDumper::supportedPixelFormats(QAbstractVideoBuffer::HandleType type) const
{
    QList<QVideoFrame::PixelFormat> ret;

    ret.append(QVideoFrame::Format_RGB32);
    ret.append(QVideoFrame::Format_YUYV);

    return ret;
}


CameraManager::CameraManager(int num, int width, int height, bool yuv)
{
    Num = num;

    FrameWidth = width;
    FrameHeight = height;
    FrameFormatYUV = yuv;

    int fbsize = FrameWidth * FrameHeight;
    if (yuv) fbsize /= 2;
    FrameBuffer = new u32[fbsize];

    InputType = -1;
    Init();
}

CameraManager::~CameraManager()
{
    DeInit();

    // save settings here?

    delete[] FrameBuffer;
}

void CameraManager::Init()
{
    if (InputType != -1)
        DeInit();

    // TODO: load settings from config!!
    InputType = 0;
    InputType = 1;
    ImagePath = "test.jpg";

    {
        // fill the framebuffer with black

        int total = FrameWidth * FrameHeight;
        u32 fill = 0;
        if (FrameFormatYUV)
        {
            total /= 2;
            fill = 0x80008000;
        }

        for (int i = 0; i < total; i++)
            FrameBuffer[i] = fill;
    }

    if (InputType == 1)
    {
        // still image

        QImage img(ImagePath);
        if (!img.isNull())
        {
            QImage imgconv = img.convertToFormat(QImage::Format_RGB32);
            if (FrameFormatYUV)
            {
                CopyFrame_RGBtoYUV((u32*)img.bits(), img.width(), img.height(),
                                   FrameBuffer, FrameWidth, FrameHeight);
            }
            else
            {
                CopyFrame_Straight((u32*)img.bits(), img.width(), img.height(),
                                   FrameBuffer, FrameWidth, FrameHeight,
                                   false);
            }
        }
    }
}

void CameraManager::DeInit()
{
    InputType = -1;
}

void CameraManager::Start()
{
    //
}

void CameraManager::Stop()
{
    //
}

void CameraManager::CaptureFrame(u32* frame, int width, int height, bool yuv)
{
    FrameMutex.lock();

    if (width == FrameWidth && height == FrameHeight && yuv == FrameFormatYUV)
    {
        int len = width * height;
        if (yuv) len /= 2;
        memcpy(frame, FrameBuffer, len * sizeof(u32));
    }
    else
    {
        //
    }

    FrameMutex.unlock();
}

void CameraManager::CopyFrame_Straight(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight, bool yuv)
{
    if (yuv)
    {
        swidth /= 2;
        dwidth /= 2;
    }

    for (int dy = 0; dy < dheight; dy++)
    {
        int sy = (dy * sheight) / dheight;

        for (int dx = 0; dx < dwidth; dx++)
        {
            int sx = (dx * swidth) / dwidth;

            dst[(dy * dwidth) + dx] = src[(sy * swidth) + sx];
        }
    }
}

void CameraManager::CopyFrame_RGBtoYUV(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight)
{
    for (int dy = 0; dy < dheight; dy++)
    {
        int sy = (dy * sheight) / dheight;

        for (int dx = 0; dx < dwidth; dx+=2)
        {
            int sx;

            sx = (dx * swidth) / dwidth;
            u32 pixel1 = src[sy*swidth + sx];

            sx = ((dx+1) * swidth) / dwidth;
            u32 pixel2 = src[sy*swidth + sx];

            int r1 = (pixel1 >> 16) & 0xFF;
            int g1 = (pixel1 >> 8) & 0xFF;
            int b1 = pixel1 & 0xFF;

            int r2 = (pixel2 >> 16) & 0xFF;
            int g2 = (pixel2 >> 8) & 0xFF;
            int b2 = pixel2 & 0xFF;

            int y1 = ((r1 * 19595) + (g1 * 38470) + (b1 * 7471)) >> 16;
            int u1 = ((b1 - y1) * 32244) >> 16;
            int v1 = ((r1 - y1) * 57475) >> 16;

            int y2 = ((r2 * 19595) + (g2 * 38470) + (b2 * 7471)) >> 16;
            int u2 = ((b2 - y2) * 32244) >> 16;
            int v2 = ((r2 - y2) * 57475) >> 16;

            u1 += 128; v1 += 128;
            u2 += 128; v2 += 128;

            y1 = std::clamp(y1, 0, 255); u1 = std::clamp(u1, 0, 255); v1 = std::clamp(v1, 0, 255);
            y2 = std::clamp(y2, 0, 255); u2 = std::clamp(u2, 0, 255); v2 = std::clamp(v2, 0, 255);

            // huh
            u1 = (u1 + u2) >> 1;
            v1 = (v1 + v2) >> 1;

            dst[(dy*dwidth + dx) / 2] = y1 | (u1 << 8) | (y2 << 16) | (v1 << 24);
        }
    }
}

void CameraManager::CopyFrame_YUVtoRGB(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight)
{
    for (int dy = 0; dy < dheight; dy++)
    {
        int sy = (dy * sheight) / dheight;

        for (int dx = 0; dx < dwidth; dx+=2)
        {
            int sx = (dx * swidth) / dwidth;
            u32 val = src[(sy*swidth + sx) / 2];

            int y1 = val & 0xFF;
            int u = (val >> 8) & 0xFF;
            int y2 = (val >> 16) & 0xFF;
            int v = (val >> 24) & 0xFF;

            u -= 128; v -= 128;

            int r1 = y1 + ((v * 91881) >> 16);
            int g1 = y1 - ((v * 46793) >> 16) - ((u * 22544) >> 16);
            int b1 = y1 + ((u * 116129) >> 16);

            int r2 = y2 + ((v * 91881) >> 16);
            int g2 = y2 - ((v * 46793) >> 16) - ((u * 22544) >> 16);
            int b2 = y2 + ((u * 116129) >> 16);

            r1 = std::clamp(r1, 0, 255); g1 = std::clamp(g1, 0, 255); b1 = std::clamp(b1, 0, 255);
            r2 = std::clamp(r2, 0, 255); g2 = std::clamp(g2, 0, 255); b2 = std::clamp(b2, 0, 255);

            u32 col1 = (r1 << 16) | (g1 << 8) | b1;
            u32 col2 = (r2 << 16) | (g2 << 8) | b2;

            dst[dy*dwidth + dx  ] = col1;
            dst[dy*dwidth + dx+1] = col1;
        }
    }
}
