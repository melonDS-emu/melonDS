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
#include "Config.h"


CameraFrameDumper::CameraFrameDumper(QObject* parent) : QAbstractVideoSurface(parent)
{
    camList.append((CameraManager*)parent);
}

bool CameraFrameDumper::present(const QVideoFrame& _frame)
{
    QVideoFrame frame(_frame);
    if (!frame.map(QAbstractVideoBuffer::ReadOnly))
        return false;
    if (!frame.isReadable())
        return false;

    for (CameraManager* cam : camList)
        cam->feedFrame((u32*)frame.bits(), frame.width(), frame.height(), frame.pixelFormat() == QVideoFrame::Format_YUYV);

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


CameraManager::CameraManager(int num, int width, int height, bool yuv) : QObject()
{
    this->num = num;

    startNum = 0;

    // QCamera needs to be controlled from the UI thread, hence this
    connect(this, SIGNAL(camStartSignal()), this, SLOT(camStart()));
    connect(this, SIGNAL(camStopSignal()), this, SLOT(camStop()));

    frameWidth = width;
    frameHeight = height;
    frameFormatYUV = yuv;

    int fbsize = frameWidth * frameHeight;
    if (yuv) fbsize /= 2;
    frameBuffer = new u32[fbsize];

    inputType = -1;
    init();
}

CameraManager::~CameraManager()
{
    deInit();

    // save settings here?

    delete[] frameBuffer;
}

void CameraManager::init()
{
    if (inputType != -1)
        deInit();

    startNum = 0;

    inputType = Config::Camera[num].InputType;
    imagePath = QString::fromStdString(Config::Camera[num].ImagePath);
    camDeviceName = QString::fromStdString(Config::Camera[num].CamDeviceName);

    {
        // fill the framebuffer with black

        int total = frameWidth * frameHeight;
        u32 fill = 0;
        if (frameFormatYUV)
        {
            total /= 2;
            fill = 0x80008000;
        }

        for (int i = 0; i < total; i++)
            frameBuffer[i] = fill;
    }

    if (inputType == 1)
    {
        // still image

        QImage img(imagePath);
        if (!img.isNull())
        {
            QImage imgconv = img.convertToFormat(QImage::Format_RGB32);
            if (frameFormatYUV)
            {
                copyFrame_RGBtoYUV((u32*)img.bits(), img.width(), img.height(),
                                   frameBuffer, frameWidth, frameHeight);
            }
            else
            {
                copyFrame_Straight((u32*)img.bits(), img.width(), img.height(),
                                   frameBuffer, frameWidth, frameHeight,
                                   false);
            }
        }
    }
    else if (inputType == 2)
    {
        // physical camera

        camDevice = new QCamera(camDeviceName.toUtf8());
        camDumper = new CameraFrameDumper(this);
        camDevice->setViewfinder(camDumper);

        /*camDevice->load();
        QCameraViewfinderSettings settings;

        auto resolutions = camDevice->supportedViewfinderResolutions();
        for (auto& res : resolutions)
        {
            printf("RESOLUTION: %d x %d\n", res.width(), res.height());
        }

        camDevice->unload();*/

        QCameraViewfinderSettings settings;
        settings.setResolution(640, 480);
        settings.setPixelFormat(QVideoFrame::Format_YUYV);
        camDevice->setViewfinderSettings(settings);
    }
}

void CameraManager::deInit()
{
    if (inputType == 2)
    {
        camDevice->stop();
        delete camDevice;
        delete camDumper;
    }

    inputType = -1;
}

void CameraManager::start()
{
    startNum++;
    if (startNum > 1) return;

    if (inputType == 2)
    {
        emit camStartSignal();
    }
}

void CameraManager::stop()
{
    startNum--;
    if (startNum > 0) return;

    if (inputType == 2)
    {
        emit camStopSignal();
    }
}

void CameraManager::camStart()
{
    camDevice->start();
}

void CameraManager::camStop()
{
    camDevice->stop();
}

void CameraManager::captureFrame(u32* frame, int width, int height, bool yuv)
{
    frameMutex.lock();

    if (width == frameWidth && height == frameHeight && yuv == frameFormatYUV)
    {
        int len = width * height;
        if (yuv) len /= 2;
        memcpy(frame, frameBuffer, len * sizeof(u32));
    }
    else
    {
        if (yuv == frameFormatYUV)
        {
            copyFrame_Straight(frameBuffer, frameWidth, frameHeight,
                               frame, width, height,
                               yuv);
        }
        else if (yuv)
        {
            copyFrame_RGBtoYUV(frameBuffer, frameWidth, frameHeight,
                               frame, width, height);
        }
        else
        {
            copyFrame_YUVtoRGB(frameBuffer, frameWidth, frameHeight,
                               frame, width, height);
        }
    }

    frameMutex.unlock();
}

void CameraManager::feedFrame(u32* frame, int width, int height, bool yuv)
{
    frameMutex.lock();

    if (width == frameWidth && height == frameHeight && yuv == frameFormatYUV)
    {
        int len = width * height;
        if (yuv) len /= 2;
        memcpy(frameBuffer, frame, len * sizeof(u32));
    }
    else
    {
        if (yuv == frameFormatYUV)
        {
            copyFrame_Straight(frame, width, height,
                               frameBuffer, frameWidth, frameHeight,
                               yuv);
        }
        else if (yuv)
        {
            copyFrame_RGBtoYUV(frame, width, height,
                               frameBuffer, frameWidth, frameHeight);
        }
        else
        {
            copyFrame_YUVtoRGB(frame, width, height,
                               frameBuffer, frameWidth, frameHeight);
        }
    }

    frameMutex.unlock();
}

void CameraManager::copyFrame_Straight(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight, bool yuv)
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

void CameraManager::copyFrame_RGBtoYUV(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight)
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

void CameraManager::copyFrame_YUVtoRGB(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight)
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
