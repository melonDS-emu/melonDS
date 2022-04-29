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

        //
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
