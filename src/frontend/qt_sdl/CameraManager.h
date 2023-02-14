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

#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include <QCamera>
#if QT_VERSION >= 0x060000
    #include <QMediaDevices>
    #include <QCameraDevice>
    #include <QMediaCaptureSession>
    #include <QVideoSink>
#else
    #include <QCameraInfo>
    #include <QAbstractVideoSurface>
    #include <QVideoSurfaceFormat>
#endif
#include <QMutex>

#include "types.h"

class CameraManager;


#if QT_VERSION >= 0x060000

class CameraFrameDumper : public QVideoSink
{
    Q_OBJECT

public:
    CameraFrameDumper(QObject* parent = nullptr);

public slots:
    void present(const QVideoFrame& frame);

private:
    CameraManager* cam;
};

#else

class CameraFrameDumper : public QAbstractVideoSurface
{
    Q_OBJECT

public:
    CameraFrameDumper(QObject* parent = nullptr);

    bool present(const QVideoFrame& frame) override;
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(QAbstractVideoBuffer::HandleType type = QAbstractVideoBuffer::NoHandle) const override;

private:
    CameraManager* cam;
};

#endif


class CameraManager : public QObject
{
    Q_OBJECT

public:
    CameraManager(int num, int width, int height, bool yuv);
    ~CameraManager();

    void init();
    void deInit();

    void start();
    void stop();
    bool isStarted();

    void setXFlip(bool flip);

    void captureFrame(u32* frame, int width, int height, bool yuv);

    void feedFrame(u32* frame, int width, int height, bool yuv);
    void feedFrame_UYVY(u32* frame, int width, int height);
    void feedFrame_NV12(u8* planeY, u8* planeUV, int width, int height);

signals:
    void camStartSignal();
    void camStopSignal();

private slots:
    void camStart();
    void camStop();

private:
    int num;

    int startNum;

    int inputType;
    QString imagePath;
    QString camDeviceName;

    QCamera* camDevice;
    CameraFrameDumper* camDumper;
#if QT_VERSION >= 0x060000
    QMediaCaptureSession* camSession;
#endif

    int frameWidth, frameHeight;
    bool frameFormatYUV;
    u32* frameBuffer;
    u32* tempFrameBuffer;
    QMutex frameMutex;

    bool xFlip;

    void copyFrame_Straight(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight, bool xflip, bool yuv);
    void copyFrame_RGBtoYUV(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight, bool xflip);
    void copyFrame_YUVtoRGB(u32* src, int swidth, int sheight, u32* dst, int dwidth, int dheight, bool xflip);
};

#endif // CAMERAMANAGER_H
