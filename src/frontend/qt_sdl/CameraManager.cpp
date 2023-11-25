/*
    Copyright 2016-2023 melonDS team

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

#include <QEventLoop>

#include "CameraManager.h"
#include "Config.h"

using namespace melonDS;

#if QT_VERSION >= 0x060000

CameraFrameDumper::CameraFrameDumper(QObject* parent) : QVideoSink(parent)
{
    cam = (CameraManager*)parent;

    connect(this, &CameraFrameDumper::videoFrameChanged, this, &CameraFrameDumper::present);
}

void CameraFrameDumper::present(const QVideoFrame& _frame)
{
    QVideoFrame frame(_frame);
    if (!frame.map(QVideoFrame::ReadOnly))
        return;
    if (!frame.isReadable())
    {
        frame.unmap();
        return;
    }

    switch (frame.pixelFormat())
    {
    case QVideoFrameFormat::Format_XRGB8888:
    case QVideoFrameFormat::Format_YUYV:
        cam->feedFrame((u32*)frame.bits(0), frame.width(), frame.height(), frame.pixelFormat() == QVideoFrameFormat::Format_YUYV);
        break;

    case QVideoFrameFormat::Format_UYVY:
        cam->feedFrame_UYVY((u32*)frame.bits(0), frame.width(), frame.height());
        break;

    case QVideoFrameFormat::Format_NV12:
        cam->feedFrame_NV12((u8*)frame.bits(0), (u8*)frame.bits(1), frame.width(), frame.height());
        break;
    }

    frame.unmap();
}

#else

CameraFrameDumper::CameraFrameDumper(QObject* parent) : QAbstractVideoSurface(parent)
{
    cam = (CameraManager*)parent;
}

bool CameraFrameDumper::present(const QVideoFrame& _frame)
{
    QVideoFrame frame(_frame);
    if (!frame.map(QAbstractVideoBuffer::ReadOnly))
        return false;
    if (!frame.isReadable())
    {
        frame.unmap();
        return false;
    }

    switch (frame.pixelFormat())
    {
    case QVideoFrame::Format_RGB32:
    case QVideoFrame::Format_YUYV:
        cam->feedFrame((u32*)frame.bits(0), frame.width(), frame.height(), frame.pixelFormat() == QVideoFrame::Format_YUYV);
        break;

    case QVideoFrame::Format_UYVY:
        cam->feedFrame_UYVY((u32*)frame.bits(0), frame.width(), frame.height());
        break;

    case QVideoFrame::Format_NV12:
        cam->feedFrame_NV12((u8*)frame.bits(0), (u8*)frame.bits(1), frame.width(), frame.height());
        break;
    }

    frame.unmap();

    return true;
}

QList<QVideoFrame::PixelFormat> CameraFrameDumper::supportedPixelFormats(QAbstractVideoBuffer::HandleType type) const
{
    QList<QVideoFrame::PixelFormat> ret;

    ret.append(QVideoFrame::Format_RGB32);
    ret.append(QVideoFrame::Format_YUYV);
    ret.append(QVideoFrame::Format_UYVY);
    ret.append(QVideoFrame::Format_NV12);

    return ret;
}

#endif


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
    tempFrameBuffer = new u32[fbsize];

    inputType = -1;
    xFlip = false;
    init();
}

CameraManager::~CameraManager()
{
    deInit();

    // save settings here?

    delete[] frameBuffer;
    delete[] tempFrameBuffer;
}

void CameraManager::init()
{
    if (inputType != -1)
        deInit();

    startNum = 0;

    inputType = Config::Camera[num].InputType;
    imagePath = QString::fromStdString(Config::Camera[num].ImagePath);
    camDeviceName = QString::fromStdString(Config::Camera[num].CamDeviceName);

    camDevice = nullptr;

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
                                   frameBuffer, frameWidth, frameHeight,
                                   false);
            }
            else
            {
                copyFrame_Straight((u32*)img.bits(), img.width(), img.height(),
                                   frameBuffer, frameWidth, frameHeight,
                                   false, false);
            }
        }
    }
    else if (inputType == 2)
    {
        // physical camera

#if QT_VERSION >= 0x060000
        const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
        for (const QCameraDevice& cam : cameras)
        {
            if (QString(cam.id()) == camDeviceName)
            {
                camDevice = new QCamera(cam);
                break;
            }
        }

        if (camDevice)
        {
            const QList<QCameraFormat> supported = camDevice->cameraDevice().videoFormats();
            bool good = false;
            for (const QCameraFormat& item : supported)
            {
                if (item.pixelFormat() != QVideoFrameFormat::Format_YUYV &&
                    item.pixelFormat() != QVideoFrameFormat::Format_UYVY &&
                    item.pixelFormat() != QVideoFrameFormat::Format_NV12 &&
                    item.pixelFormat() != QVideoFrameFormat::Format_XRGB8888)
                    continue;

                if (item.resolution().width() != 640 && item.resolution().height() != 480)
                    continue;

                camDevice->setCameraFormat(item);
                good = true;
                break;
            }

            if (!good)
            {
                delete camDevice;
                camDevice = nullptr;
            }
            else
            {
                camDumper = new CameraFrameDumper(this);

                camSession = new QMediaCaptureSession(this);
                camSession->setCamera(camDevice);
                camSession->setVideoOutput(camDumper);
            }
        }
#else
        camDevice = new QCamera(camDeviceName.toUtf8());
        if (camDevice->error() != QCamera::NoError)
        {
            delete camDevice;
            camDevice = nullptr;
        }

        if (camDevice)
        {
            camDevice->load();
            if (camDevice->status() == QCamera::LoadingStatus)
            {
                QEventLoop loop;
                connect(camDevice, &QCamera::statusChanged, &loop, &QEventLoop::quit);
                loop.exec();
            }

            const QList<QCameraViewfinderSettings> supported = camDevice->supportedViewfinderSettings();
            bool good = false;
            for (const QCameraViewfinderSettings& item : supported)
            {
                if (item.pixelFormat() != QVideoFrame::Format_YUYV &&
                    item.pixelFormat() != QVideoFrame::Format_UYVY &&
                    item.pixelFormat() != QVideoFrame::Format_NV12 &&
                    item.pixelFormat() != QVideoFrame::Format_RGB32)
                    continue;

                if (item.resolution().width() != 640 && item.resolution().height() != 480)
                    continue;

                camDevice->setViewfinderSettings(item);
                good = true;
                break;
            }

            camDevice->unload();

            if (!good)
            {
                delete camDevice;
                camDevice = nullptr;
            }
            else
            {
                camDumper = new CameraFrameDumper(this);
                camDevice->setViewfinder(camDumper);
            }
        }
#endif
    }
}

void CameraManager::deInit()
{
    if (inputType == 2)
    {
        if (camDevice)
        {
            camDevice->stop();
            delete camDevice;
            delete camDumper;
#if QT_VERSION >= 0x060000
            delete camSession;
#endif
        }
    }

    camDevice = nullptr;
    inputType = -1;
}

void CameraManager::start()
{
    if (startNum == 1) return;
    startNum = 1;

    if (inputType == 2)
    {
        emit camStartSignal();
    }
}

void CameraManager::stop()
{
    if (startNum == 0) return;
    startNum = 0;

    if (inputType == 2)
    {
        emit camStopSignal();
    }
}

bool CameraManager::isStarted()
{
    return startNum != 0;
}

void CameraManager::camStart()
{
    if (camDevice)
        camDevice->start();
}

void CameraManager::camStop()
{
    if (camDevice)
        camDevice->stop();
}

void CameraManager::setXFlip(bool flip)
{
    xFlip = flip;
}

void CameraManager::captureFrame(u32* frame, int width, int height, bool yuv)
{
    frameMutex.lock();

    if ((width == frameWidth) &&
        (height == frameHeight) &&
        (yuv == frameFormatYUV) &&
        (!xFlip))
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
                               xFlip, yuv);
        }
        else if (yuv)
        {
            copyFrame_RGBtoYUV(frameBuffer, frameWidth, frameHeight,
                               frame, width, height,
                               xFlip);
        }
        else
        {
            copyFrame_YUVtoRGB(frameBuffer, frameWidth, frameHeight,
                               frame, width, height,
                               xFlip);
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
                               false, yuv);
        }
        else if (yuv)
        {
            copyFrame_RGBtoYUV(frame, width, height,
                               frameBuffer, frameWidth, frameHeight,
                               false);
        }
        else
        {
            copyFrame_YUVtoRGB(frame, width, height,
                               frameBuffer, frameWidth, frameHeight,
                               false);
        }
    }

    frameMutex.unlock();
}

void CameraManager::feedFrame_UYVY(u32* frame, int width, int height)
{
    for (int y = 0; y < frameHeight; y++)
    {
        int sy = (y * height) / frameHeight;

        for (int x = 0; x < frameWidth; x+=2)
        {
            int sx = (x * width) / frameWidth;

            u32 val = frame[((sy*width) + sx) >> 1];

            val = ((val & 0xFF00FF00) >> 8) | ((val & 0x00FF00FF) << 8);

            tempFrameBuffer[((y*frameWidth) + x) >> 1] = val;
        }
    }

    feedFrame(tempFrameBuffer, frameWidth, frameHeight, true);
}

void CameraManager::feedFrame_NV12(u8* planeY, u8* planeUV, int width, int height)
{
    for (int y = 0; y < frameHeight; y++)
    {
        int sy = (y * height) / frameHeight;

        for (int x = 0; x < frameWidth; x+=2)
        {
            int sx1 = (x * width) / frameWidth;
            int sx2 = ((x+1) * width) / frameWidth;

            u32 val;

            u8 y1 = planeY[(sy*width) + sx1];
            u8 y2 = planeY[(sy*width) + sx2];

            int uvpos = (((sy>>1)*(width>>1)) + (sx1>>1));
            u8 u = planeUV[uvpos << 1];
            u8 v = planeUV[(uvpos << 1) + 1];

            val = y1 | (u << 8) | (y2 << 16) | (v << 24);
            tempFrameBuffer[((y*frameWidth) + x) >> 1] = val;
        }
    }

    feedFrame(tempFrameBuffer, frameWidth, frameHeight, true);
}

void CameraManager::copyFrame_Straight(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight, bool xflip, bool yuv)
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
            if (xflip) sx = swidth-1 - sx;

            u32 val = src[(sy * swidth) + sx];

            if (yuv)
            {
                if (xflip)
                    val = (val & 0xFF00FF00) |
                          ((val >> 16) & 0xFF) |
                          ((val & 0xFF) << 16);
            }
            else
                val |= 0xFF000000;

            dst[(dy * dwidth) + dx] = val;
        }
    }
}

void CameraManager::copyFrame_RGBtoYUV(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight, bool xflip)
{
    for (int dy = 0; dy < dheight; dy++)
    {
        int sy = (dy * sheight) / dheight;

        for (int dx = 0; dx < dwidth; dx+=2)
        {
            int sx;

            sx = (dx * swidth) / dwidth;
            if (xflip) sx = swidth-1 - sx;

            u32 pixel1 = src[sy*swidth + sx];

            sx = ((dx+1) * swidth) / dwidth;
            if (xflip) sx = swidth-1 - sx;

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

void CameraManager::copyFrame_YUVtoRGB(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight, bool xflip)
{
    for (int dy = 0; dy < dheight; dy++)
    {
        int sy = (dy * sheight) / dheight;

        for (int dx = 0; dx < dwidth; dx+=2)
        {
            int sx = (dx * swidth) / dwidth;
            if (xflip) sx = swidth-2 - sx;

            u32 val = src[(sy*swidth + sx) / 2];

            int y1, y2;
            if (xflip)
            {
                y1 = (val >> 16) & 0xFF;
                y2 = val & 0xFF;
            }
            else
            {
                y1 = val & 0xFF;
                y2 = (val >> 16) & 0xFF;
            }
            int u = (val >> 8) & 0xFF;
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

            u32 col1 = 0xFF000000 | (r1 << 16) | (g1 << 8) | b1;
            u32 col2 = 0xFF000000 | (r2 << 16) | (g2 << 8) | b2;

            dst[dy*dwidth + dx  ] = col1;
            dst[dy*dwidth + dx+1] = col2;
        }
    }
}
