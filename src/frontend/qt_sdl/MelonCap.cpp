/*
    Copyright 2016-2022 Arisotura

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

#include <QPainter>
#include <QMessageBox>

#include <stdio.h>
#include <string.h>
#include "MelonCap.h"
#include "EmuInstance.h"
#include "Window.h"
#include "../NDS.h"
#include "../GPU.h"

#if 0
#include <windows.h>
#include <setupapi.h>
#include <guiddef.h>
#include <winusb.h>
#endif

#include <libusb-1.0/libusb.h>


using namespace melonDS;

MelonCapWindow* MelonCapWindow::currentWin = nullptr;

// this crap was built from the reverse-engineering of ds_capture.exe
// mixed in with their Linux capture sample code

#if 0
GUID InterfaceClass = {0xA0B880F6, 0xD6A5, 0x4700, {0xA8, 0xEA, 0x22, 0x28, 0x2A, 0xCA, 0x55, 0x87}};
HANDLE CapHandle;
WINUSB_INTERFACE_HANDLE CapUSBHandle;
#endif

static libusb_device_handle* capHandle = nullptr;

bool initCap();
void deInitCap();


MelonCapWindow::MelonCapWindow(QWidget* parent) : QMainWindow(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);

    hasBuffers = false;

    screen[0] = QImage(256, 192, QImage::Format_RGB32);
    screen[1] = QImage(256, 192, QImage::Format_RGB32);

    capBuffer = new u32[256 * 384];
    compBuffer = new u32[256 * 384];

    capImage = QImage(256, 384, QImage::Format_RGB32);
    compImage = QImage(256, 384, QImage::Format_RGB32);

    emuInstance = ((MainWindow*)parent)->getEmuInstance();

    if (!initCap())
    {
        QMessageBox::critical(this, "melonCap",
                              "Failed to open capture card device.");

        close();
        return;
    }

    setWindowTitle("melonCap(tm)");
    setGeometry(0, 0, 256*3, 384);

    //connect(emuInstance->getEmuThread(), SIGNAL(windowUpdate()), this, SLOT(repaint()));
    connect(this, SIGNAL(doRepaint()), this, SLOT(repaint()));

    show();
}

MelonCapWindow::~MelonCapWindow()
{
    //disconnect(emuInstance->getEmuThread(), SIGNAL(windowUpdate()), this, SLOT(repaint()));

    delete[] capBuffer;
    delete[] compBuffer;

    if (capHandle)
        deInitCap();

    closeWin();
}


void MelonCapWindow::drawScreen()
{
    auto emuThread = emuInstance->getEmuThread();
    if (!emuThread->emuIsActive())
    {
        hasBuffers = false;
        return;
    }

    auto nds = emuInstance->getNDS();
    assert(nds != nullptr);

    bufferLock.lock();
    hasBuffers = nds->GPU.GetFramebuffers(&topBuffer, &bottomBuffer);
    bufferLock.unlock();

    updateCap();

    emit doRepaint();
}

void MelonCapWindow::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    // fill background
    painter.fillRect(event->rect(), QColor::fromRgb(0, 0, 0));

    auto emuThread = emuInstance->getEmuThread();

    if (emuThread->emuIsActive())
    {
        emuInstance->renderLock.lock();

        bufferLock.lock();
        if (hasBuffers)
        {
            memcpy(screen[0].scanLine(0), topBuffer, 256 * 192 * 4);
            memcpy(screen[1].scanLine(0), bottomBuffer, 256 * 192 * 4);
            memcpy(capImage.scanLine(0), capBuffer, 256 * 384 * 4);
            memcpy(compImage.scanLine(0), compBuffer, 256 * 384 * 4);
        }
        bufferLock.unlock();

        QRect toprc(0, 0, 256, 192);
        QRect bottomrc(0, 192, 256, 192);
        painter.drawImage(toprc, screen[0]);
        painter.drawImage(bottomrc, screen[1]);

        QRect caprc(256, 0, 256, 384);
        painter.drawImage(caprc, capImage);

        QRect comprc(512, 0, 256, 384);
        painter.drawImage(comprc, compImage);

        emuInstance->renderLock.unlock();
    }
}


bool initCap()
{
    printf("MelonCap init\n");

#if 0
    HDEVINFO devinfo = SetupDiGetClassDevsW(&InterfaceClass, NULL, NULL, DIGCF_DEVICEINTERFACE|DIGCF_PRESENT);
    if (devinfo == INVALID_HANDLE_VALUE) return;

    int member = 0;
    bool good = false;
    for (;;)
    {
        SP_DEVICE_INTERFACE_DATA interfacedata;
        memset(&interfacedata, 0, sizeof(interfacedata));
        interfacedata.cbSize = sizeof(interfacedata);

        BOOL ret = SetupDiEnumDeviceInterfaces(devinfo, NULL, &InterfaceClass, member, &interfacedata);
        if (!ret)
        {
            printf("found %d interfaces\n", member);
            break;
        }

        DWORD requiredsize = 0;
        SetupDiGetDeviceInterfaceDetailW(devinfo, &interfacedata, NULL, NULL, &requiredsize, NULL);
        printf("%d: required size %d\n", member, requiredsize);

        PSP_DEVICE_INTERFACE_DETAIL_DATA_W interfacedetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)new u8[requiredsize];
        interfacedetail->cbSize = sizeof(SP_INTERFACE_DEVICE_DETAIL_DATA_W);
        ret = SetupDiGetDeviceInterfaceDetailW(devinfo, &interfacedata, interfacedetail, requiredsize, NULL, NULL);
        if (ret)
        {
            printf("got interface detail: path=%S\n", interfacedetail->DevicePath);
            HANDLE file = CreateFileW(interfacedetail->DevicePath, GENERIC_READ|GENERIC_WRITE,
                                      FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
            if (file != INVALID_HANDLE_VALUE)
            {
                WINUSB_INTERFACE_HANDLE usbhandle;
                ret = WinUsb_Initialize(file, &usbhandle);
                if (ret)
                {
                    int val;
                    val = 0x1E;
                    WinUsb_SetPipePolicy(usbhandle, 0x00, PIPE_TRANSFER_TIMEOUT, 4, &val);
                    val = 0x32;
                    WinUsb_SetPipePolicy(usbhandle, 0x82, PIPE_TRANSFER_TIMEOUT, 4, &val);
                    val = 0x01;
                    WinUsb_SetPipePolicy(usbhandle, 0x82, RAW_IO, 1, &val);

                    printf("looking good\n");
                    good = true;

                    CapHandle = file;
                    CapUSBHandle = usbhandle;
                }
                else
                    CloseHandle(file);
            }
        }

        delete[] (u8*)interfacedetail;

        if (good) break;

        member++;
    }

    SetupDiDestroyDeviceInfoList(devinfo);


    WinBitmapData = new u32[256*3 * 384];
    CapWin = new CapWindow();
#endif

    if (libusb_init(nullptr) != LIBUSB_SUCCESS)
        return false;

    capHandle = libusb_open_device_with_vid_pid(nullptr, 0x16D0, 0x0647);
    if (!capHandle)
    {
        libusb_exit(nullptr);
        return false;
    }

    if (libusb_set_configuration(capHandle, 1) != LIBUSB_SUCCESS)
    {
        libusb_close(capHandle);
        capHandle = nullptr;
        libusb_exit(nullptr);
        return false;
    }

    if (libusb_claim_interface(capHandle, 0) != LIBUSB_SUCCESS)
    {
        libusb_close(capHandle);
        capHandle = nullptr;
        libusb_exit(nullptr);
        return false;
    }

    return true;
}

void deInitCap()
{
#if 0
    CapWin->close();
    delete CapWin;
    delete[] WinBitmapData;

    WinUsb_Free(CapUSBHandle);
    CloseHandle(CapHandle);
#endif

    libusb_release_interface(capHandle, 0);
    libusb_close(capHandle);
    capHandle = nullptr;
    libusb_exit(nullptr);
}


static int vendorIn(u8 req, u16 len, u8* buf)
{
#if 0
    WINUSB_SETUP_PACKET pkt;
    pkt.RequestType = 0xC0; // device to host
    pkt.Request = req;
    pkt.Value = 0; // ?????
    pkt.Index = 0;
    pkt.Length = len;

    ULONG ret = 0;
    BOOL res = WinUsb_ControlTransfer(CapUSBHandle, pkt, buf, len, &ret, NULL);
    if (!res) return -1;
    return ret;
#endif

    int ret = libusb_control_transfer(capHandle, (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN),
                                      req, 0, 0, buf, len, 500);
    if (ret < 0) return -1;
    return ret;
}

static int vendorOut(u8 req, u16 val, u16 len, u8* buf)
{
#if 0
    WINUSB_SETUP_PACKET pkt;
    pkt.RequestType = 0x40; // host to device
    pkt.Request = req;
    pkt.Value = val;
    pkt.Index = 0;
    pkt.Length = len;

    ULONG ret = 0;
    BOOL res = WinUsb_ControlTransfer(CapUSBHandle, pkt, buf, len, &ret, NULL);
    if (!res) return -1;
    return ret;
#endif

    int ret = libusb_control_transfer(capHandle, (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT),
                                      req, val, 0, buf, len, 500);
    if (ret < 0) return -1;
    return ret;
}

static int bulkIn(u8* buf, u32 len)
{
#if 0
    ULONG ret = 0;
    BOOL res = WinUsb_ReadPipe(CapUSBHandle, 0x82, buf, len, &ret, NULL);
    if (!res) return -1;
    return ret;
#endif

    int transferred = 0;
    int ret = libusb_bulk_transfer(capHandle, LIBUSB_ENDPOINT_IN | 2,
                                   buf, len, &transferred, 500);
    if (ret < 0) return -1;
    return transferred;
}


static u32 convertColor(u16 col)
{
    u32 b = col & 0x001F;
    u32 g = (col & 0x07E0) >> 5;
    u32 r = (col & 0xF800) >> 11;

    u32 ret = 0xFF000000;
    ret |= ((r << 3) | (r >> 2)) << 16;
    ret |= ((g << 2) | (g >> 4)) << 8;
    ret |= (b << 3) | (b >> 2);
    return ret;
}

void captureFrame(u32* output)
{
    u32 ret;
    u8 derp;
    u32 framelen = 256*384*2;
    u16 frame[framelen/2];
    u32 framepos = 0;
    u8 frameinfo[64];

    ret = vendorOut(0x30, 0, 0, &derp);
    if (ret < 0) return;

    int tries = 0;
    while (framepos < framelen)
    {
        ret = bulkIn((u8*)&frame[framepos/2], framelen-framepos);
        if (ret < 0) break;
        if (ret == 0)
        {
            tries++;
            if (tries >= 100) break;
            continue;
        }
        framepos += ret;
    }

    ret = vendorIn(0x30, 64, frameinfo);
    if (ret < 0) return;
    if ((frameinfo[0] & 0x03) != 0x03) return;
    if (!frameinfo[52]) return;

    u16* in = &frame[0];

    for (int y = 0; y < 384; y++)
    {
        u32* out = &output[((y/2)*256) + ((y&1)*128) ];

        if (!(frameinfo[y>>3] & (1<<(y&7))))
        {
            continue;
        }

        for (int x = 0; x < 256/2; x++)
        {
            out[0] = convertColor(in[1]);
            out[256*192] = convertColor(in[0]);
            out++;
            in += 2;
        }
    }
}

void MelonCapWindow::updateCap()
{
    if (!hasBuffers) return;
    if (!capHandle) return;

    // DS capture

    captureFrame(capBuffer);

    // compare

    for (int y = 0; y < 384; y++)
    {
        for (int x = 0; x < 256; x++)
        {
            u32 colA;
            if (y < 192) colA = ((u32*)topBuffer)[y*256 + x];
            else colA = ((u32*)bottomBuffer)[(y-192)*256 + x];
            u32 colB = capBuffer[y*256 + x];

            // best we get from the capture card is RGB565
            // so we'll ignore the lower bits
            const u32 mask = 0x00F8FCF8;
            colA &= mask;
            colB &= mask;

            if (colA == colB) compBuffer[(y*256) + x] = 0xFF000000;
            else              compBuffer[(y*256) + x] = 0xFFFFFFFF;
        }
    }
}
