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
#include "MelonCap.h"
#include "libui/ui.h"
#include "../NDS.h"
#include "../GPU.h"

#include <windows.h>
#include <setupapi.h>
#include <guiddef.h>
#include <winusb.h>


namespace MelonCap
{

uiWindow* Window;
uiArea* Area;
uiAreaHandler AreaHandler;
uiDrawBitmap* WinBitmap;
bool WinBitmapInited;

u32* WinBitmapData;

// this crap was built from the reverse-engineering of ds_capture.exe

GUID InterfaceClass = {0xA0B880F6, 0xD6A5, 0x4700, {0xA8, 0xEA, 0x22, 0x28, 0x2A, 0xCA, 0x55, 0x87}};
HANDLE CapHandle;
WINUSB_INTERFACE_HANDLE CapUSBHandle;


void OnAreaDraw(uiAreaHandler* handler, uiArea* area, uiAreaDrawParams* params)
{
    if (!WinBitmapInited)
    {
        if (WinBitmap) uiDrawFreeBitmap(WinBitmap);

        WinBitmapInited = true;
        WinBitmap = uiDrawNewBitmap(params->Context, 768, 384, 0);
    }

    if (!WinBitmap) return;
    if (!WinBitmapData) return;

    uiRect rc = {0, 0, 768, 384};

    uiDrawBitmapUpdate(WinBitmap, WinBitmapData);
    uiDrawBitmapDraw(params->Context, WinBitmap, &rc, &rc, 0);
}

void OnAreaMouseEvent(uiAreaHandler* handler, uiArea* area, uiAreaMouseEvent* evt)
{
}

void OnAreaMouseCrossed(uiAreaHandler* handler, uiArea* area, int left)
{
}

void OnAreaDragBroken(uiAreaHandler* handler, uiArea* area)
{
}

int OnAreaKeyEvent(uiAreaHandler* handler, uiArea* area, uiAreaKeyEvent* evt)
{
    return 1;
}

void OnAreaResize(uiAreaHandler* handler, uiArea* area, int width, int height)
{
}


void Init()
{
    printf("MelonCap init\n");

    // TODO: flags value!!!!!
    HDEVINFO devinfo = SetupDiGetClassDevsW(&InterfaceClass, NULL, NULL, 0x12);
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
            HANDLE file = CreateFileW(interfacedetail->DevicePath, 0xC0000000, 3, NULL, 3, 0x40000080, NULL);
            if (file != INVALID_HANDLE_VALUE)
            {
                WINUSB_INTERFACE_HANDLE usbhandle;
                ret = WinUsb_Initialize(file, &usbhandle);
                if (ret)
                {
                    int val;
                    val = 0x1E;
                    WinUsb_SetPipePolicy(usbhandle, 0x00, 3, 4, &val);
                    val = 0x32;
                    WinUsb_SetPipePolicy(usbhandle, 0x82, 3, 4, &val);
                    val = 0x01;
                    WinUsb_SetPipePolicy(usbhandle, 0x82, 7, 1, &val);

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


    AreaHandler.Draw = OnAreaDraw;
    AreaHandler.MouseEvent = OnAreaMouseEvent;
    AreaHandler.MouseCrossed = OnAreaMouseCrossed;
    AreaHandler.DragBroken = OnAreaDragBroken;
    AreaHandler.KeyEvent = OnAreaKeyEvent;
    AreaHandler.Resize = OnAreaResize;

    WinBitmapInited = false;
    WinBitmapData = new u32[768*384];

    Window = uiNewWindow("melonDS - topnotch pixel checker", 768, 384, 0, 0, 0);
    Area = uiNewArea(&AreaHandler);
    uiWindowSetChild(Window, uiControl(Area));

    uiControlShow(uiControl(Window));
}

void DeInit()
{
    uiControlDestroy(uiControl(Window));
    uiDrawFreeBitmap(WinBitmap);
    WinBitmapInited = false;
    delete[] WinBitmapData;

    WinUsb_Free(CapUSBHandle);
    CloseHandle(CapHandle);
}


u32 VendorIn(u8 req, u16 len, u8* buf)
{
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
}

u32 VendorOut(u8 req, u16 val, u16 len, u8* buf)
{
    WINUSB_SETUP_PACKET pkt;
    pkt.RequestType = 0x40; // device to host
    pkt.Request = req;
    pkt.Value = val;
    pkt.Index = 0;
    pkt.Length = len;

    ULONG ret = 0;
    BOOL res = WinUsb_ControlTransfer(CapUSBHandle, pkt, buf, len, &ret, NULL);
    if (!res) return -1;
    return ret;
}

u32 BulkIn(u8* buf, u32 len)
{
    ULONG ret = 0;
    BOOL res = WinUsb_ReadPipe(CapUSBHandle, 0x82, buf, len, &ret, NULL);
    if (!res) return -1;
    return ret;
}


u32 ConvertColor(u16 col)
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

void CaptureFrame()
{
    u32 ret;
    u8 derp;
    u32 framelen = 256*384*2;
    u16 frame[framelen/2];
    u32 framepos = 0;
    u8 frameinfo[64];

    ret = VendorOut(0x30, 0, 0, &derp);
    if (ret < 0) return;

    while (framepos < framelen)
    {
        ret = BulkIn((u8*)&frame[framepos/2], framelen-framepos);
        if (ret < 0) break;
        if (ret > 0) framepos += ret;
    }

    ret = VendorIn(0x30, 64, frameinfo);
    if (ret < 0) return;
    if ((frameinfo[0] & 0x03) != 0x03) return;
    if (!frameinfo[52]) return;

    u16* in = &frame[0];
    u32* out = &WinBitmapData[256];

    for (int y = 0; y < 384; y++)
    {
        u32* out = &WinBitmapData[((y/2)*768) + ((y&1)*128) + 256];

        if (!(frameinfo[y>>3] & (1<<(y&7))))
        {
            continue;
        }

        for (int x = 0; x < 256/2; x++)
        {
            out[0] = ConvertColor(in[1]);
            out[768*192] = ConvertColor(in[0]);
            out++;
            in += 2;
        }
    }
}

void Update()
{
    // melonDS output

    int frontbuf = GPU::FrontBuffer;

    u32* topbuf = GPU::Framebuffer[frontbuf][0];
    if (topbuf)
    {
        for (int y = 0; y < 192; y++)
        {
            memcpy(&WinBitmapData[y*768], &topbuf[y*256], 256*4);
        }
    }

    u32* botbuf = GPU::Framebuffer[frontbuf][1];
    if (botbuf)
    {
        for (int y = 0; y < 192; y++)
        {
            memcpy(&WinBitmapData[(y+192)*768], &botbuf[y*256], 256*4);
        }
    }

    // DS capture

    CaptureFrame();

    // compare

    for (int y = 0; y < 384; y++)
    {
        for (int x = 0; x < 256; x++)
        {
            u32 colA = WinBitmapData[(y*768) + x + 0];
            u32 colB = WinBitmapData[(y*768) + x + 256];

            // best we get from the capture card is RGB565
            // so we'll ignore the lower bits
            const u32 mask = 0x00F8FCF8;
            colA &= mask;
            colB &= mask;

            if (colA == colB) WinBitmapData[(y*768) + x + 512] = 0xFF00FF00;
            else              WinBitmapData[(y*768) + x + 512] = 0xFFFF0000;
        }
    }

    uiAreaQueueRedrawAll(Area);
}

}
