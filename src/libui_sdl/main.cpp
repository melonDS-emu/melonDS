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

#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include "libui/ui.h"

#include "../types.h"
#include "../version.h"
#include "../Config.h"

#include "DlgEmuSettings.h"
#include "DlgInputConfig.h"

#include "../NDS.h"
#include "../GPU.h"
#include "../SPU.h"
#include "../Wifi.h"
#include "../Platform.h"


const int kScreenRot[4] = {0, 1, 2, 3};
const int kScreenGap[6] = {0, 1, 8, 64, 90, 128};
const int kScreenLayout[3] = {0, 1, 2};
const int kScreenSizing[4] = {0, 1, 2, 3};


uiWindow* MainWindow;
uiArea* MainDrawArea;

uiMenuItem* MenuItem_Pause;
uiMenuItem* MenuItem_Reset;
uiMenuItem* MenuItem_Stop;

uiMenuItem* MenuItem_ScreenRot[4];
uiMenuItem* MenuItem_ScreenGap[6];
uiMenuItem* MenuItem_ScreenLayout[3];
uiMenuItem* MenuItem_ScreenSizing[4];

SDL_Thread* EmuThread;
int EmuRunning;
volatile int EmuStatus;

bool RunningSomething;
char ROMPath[1024];

bool ScreenDrawInited = false;
uiDrawBitmap* ScreenBitmap = NULL;
u32 ScreenBuffer[256*384];

int ScreenGap = 0;
int ScreenLayout = 0;
int ScreenSizing = 0;
int ScreenRotation = 0;

int MainScreenPos[3];
int AutoScreenSizing;

uiRect TopScreenRect;
uiRect BottomScreenRect;
uiDrawMatrix TopScreenTrans;
uiDrawMatrix BottomScreenTrans;

bool Touching = false;

u32 KeyInputMask;
SDL_Joystick* Joystick;


void SetupScreenRects(int width, int height);



void UpdateWindowTitle(void* data)
{
    uiWindowSetTitle(MainWindow, (const char*)data);
}

void AudioCallback(void* data, Uint8* stream, int len)
{
    SPU::ReadOutput((s16*)stream, len>>2);
}

int EmuThreadFunc(void* burp)
{
    NDS::Init();

    MainScreenPos[0] = 0;
    MainScreenPos[1] = 0;
    MainScreenPos[2] = 0;
    AutoScreenSizing = 0;

    ScreenDrawInited = false;
    Touching = false;

    // DS:
    // 547.060546875 samples per frame
    // 32823.6328125 samples per second
    //
    // 48000 samples per second:
    // 800 samples per frame
    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = 32824; // 32823.6328125
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 2;
    whatIwant.samples = 1024;
    whatIwant.callback = AudioCallback;
    SDL_AudioDeviceID audio = SDL_OpenAudioDevice(NULL, 0, &whatIwant, &whatIget, 0);
    if (!audio)
    {
        printf("Audio init failed: %s\n", SDL_GetError());
    }
    else
    {
        SDL_PauseAudioDevice(audio, 0);
    }

    KeyInputMask = 0xFFF;

    // TODO: support more joysticks
    if (SDL_NumJoysticks() > 0)
        Joystick = SDL_JoystickOpen(0);
    else
        Joystick = NULL;

    u32 nframes = 0;
    u32 starttick = SDL_GetTicks();
    u32 lasttick = starttick;
    u32 lastmeasuretick = lasttick;
    u32 fpslimitcount = 0;
    bool limitfps = true;
    char melontitle[100];

    while (EmuRunning != 0)
    {
        if (EmuRunning == 1)
        {
            EmuStatus = 1;

            // poll input
            u32 keymask = KeyInputMask;
            u32 joymask = 0xFFF;
            if (Joystick)
            {
                SDL_JoystickUpdate();

                Uint32 hat = SDL_JoystickGetHat(Joystick, 0);
                Sint16 axisX = SDL_JoystickGetAxis(Joystick, 0);
                Sint16 axisY = SDL_JoystickGetAxis(Joystick, 1);

                for (int i = 0; i < 12; i++)
                {
                    int btnid = Config::JoyMapping[i];
                    if (btnid < 0) continue;

                    bool pressed;
                    if (btnid == 0x101) // up
                        pressed = (hat & SDL_HAT_UP) || (axisY <= -16384);
                    else if (btnid == 0x104) // down
                        pressed = (hat & SDL_HAT_DOWN) || (axisY >= 16384);
                    else if (btnid == 0x102) // right
                        pressed = (hat & SDL_HAT_RIGHT) || (axisX >= 16384);
                    else if (btnid == 0x108) // left
                        pressed = (hat & SDL_HAT_LEFT) || (axisX <= -16384);
                    else
                        pressed = SDL_JoystickGetButton(Joystick, btnid);

                    if (pressed) joymask &= ~(1<<i);
                }
            }
            NDS::SetKeyMask(keymask & joymask);

            // emulate
            u32 nlines = NDS::RunFrame();

            if (EmuRunning == 0) break;

            // auto screen layout
            {
                MainScreenPos[2] = MainScreenPos[1];
                MainScreenPos[1] = MainScreenPos[0];
                MainScreenPos[0] = NDS::PowerControl9 >> 15;

                int guess;
                if (MainScreenPos[0] == MainScreenPos[2] &&
                    MainScreenPos[0] != MainScreenPos[1])
                {
                    // constant flickering, likely displaying 3D on both screens
                    // TODO: when both screens are used for 2D only...???
                    guess = 0;
                }
                else
                {
                    if (MainScreenPos[0] == 1)
                        guess = 1;
                    else
                        guess = 2;
                }

                if (guess != AutoScreenSizing)
                {
                    AutoScreenSizing = guess;
                    SetupScreenRects(Config::WindowWidth, Config::WindowHeight);
                }
            }

            memcpy(ScreenBuffer, GPU::Framebuffer, 256*384*4);
            uiAreaQueueRedrawAll(MainDrawArea);

            // framerate limiter based off SDL2_gfx
            float framerate;
            if (nlines == 263) framerate = 1000.0f / 60.0f;
            else               framerate = ((1000.0f * nlines) / 263.0f) / 60.0f;

            fpslimitcount++;
            u32 curtick = SDL_GetTicks();
            u32 delay = curtick - lasttick;
            lasttick = curtick;

            u32 wantedtick = starttick + (u32)((float)fpslimitcount * framerate);
            if (curtick < wantedtick && limitfps)
            {
                SDL_Delay(wantedtick - curtick);
            }
            else
            {
                fpslimitcount = 0;
                starttick = curtick;
            }

            nframes++;
            if (nframes >= 30)
            {
                u32 tick = SDL_GetTicks();
                u32 diff = tick - lastmeasuretick;
                lastmeasuretick = tick;

                u32 fps = (nframes * 1000) / diff;
                nframes = 0;

                float fpstarget;
                if (framerate < 1) fpstarget = 999;
                else fpstarget = 1000.0f/framerate;

                sprintf(melontitle, "[%d/%.0f] melonDS " MELONDS_VERSION, fps, fpstarget);
                uiQueueMain(UpdateWindowTitle, melontitle);
            }
        }
        else
        {
            EmuStatus = 2;

            // paused
            nframes = 0;
            lasttick = SDL_GetTicks();
            starttick = lasttick;
            lastmeasuretick = lasttick;
            fpslimitcount = 0;

            uiAreaQueueRedrawAll(MainDrawArea);
            SDL_Delay(100);
        }
    }

    EmuStatus = 0;

    if (Joystick) SDL_JoystickClose(Joystick);

    if (audio) SDL_CloseAudioDevice(audio);

    NDS::DeInit();

    return 44203;
}


void OnAreaDraw(uiAreaHandler* handler, uiArea* area, uiAreaDrawParams* params)
{
    if (!ScreenDrawInited)
    {
        ScreenBitmap = uiDrawNewBitmap(params->Context, 256, 384);
        ScreenDrawInited = true;
    }

    if (!ScreenBitmap) return;

    uiRect top = {0, 0, 256, 192};
    uiRect bot = {0, 192, 256, 192};

    uiDrawBitmapUpdate(ScreenBitmap, ScreenBuffer);

    uiDrawSave(params->Context);
    uiDrawTransform(params->Context, &TopScreenTrans);
    uiDrawBitmapDraw(params->Context, ScreenBitmap, &top, &TopScreenRect);
    uiDrawRestore(params->Context);

    /*uiDrawMatrix blirg;
    uiDrawMatrixSetIdentity(&blirg);
    uiDrawMatrixTranslate(&blirg, -BottomScreenRect.X, -BottomScreenRect.Y);
    uiDrawMatrixRotate(&blirg, 0, 0, -M_PI/2.0f);//3.14);
    uiDrawMatrixTranslate(&blirg, BottomScreenRect.X, BottomScreenRect.Y+BottomScreenRect.Height);
    //uiDrawMatrixScale(&blirg, 128, 192, 2, 2);
    uiDrawTransform(params->Context, &blirg);*/
    uiDrawSave(params->Context);
    uiDrawTransform(params->Context, &BottomScreenTrans);
    uiDrawBitmapDraw(params->Context, ScreenBitmap, &bot, &BottomScreenRect);
    uiDrawRestore(params->Context);
}

void OnAreaMouseEvent(uiAreaHandler* handler, uiArea* area, uiAreaMouseEvent* evt)
{
    int x = (int)evt->X;
    int y = (int)evt->Y;

    if (Touching && (evt->Up == 1))
    {
        Touching = false;
        NDS::ReleaseKey(16+6);
        NDS::ReleaseScreen();
    }
    else if (!Touching && (evt->Down == 1) &&
             (x >= BottomScreenRect.X) && (y >= BottomScreenRect.Y) &&
             (x < (BottomScreenRect.X+BottomScreenRect.Width)) && (y < (BottomScreenRect.Y+BottomScreenRect.Height)))
    {
        Touching = true;
        NDS::PressKey(16+6);
    }

    if (Touching)
    {
        x -= BottomScreenRect.X;
        y -= BottomScreenRect.Y;

        if (ScreenRotation == 0 || ScreenRotation == 2)
        {
            if (BottomScreenRect.Width != 256)
                x = (x * 256) / BottomScreenRect.Width;
            if (BottomScreenRect.Height != 192)
                y = (y * 192) / BottomScreenRect.Height;

            if (ScreenRotation == 2)
            {
                x = 255 - x;
                y = 191 - y;
            }
        }
        else
        {
            if (BottomScreenRect.Width != 192)
                x = (x * 192) / BottomScreenRect.Width;
            if (BottomScreenRect.Height != 256)
                y = (y * 256) / BottomScreenRect.Height;

            if (ScreenRotation == 1)
            {
                int tmp = x;
                x = y;
                y = 191 - tmp;
            }
            else
            {
                int tmp = x;
                x = 255 - y;
                y = tmp;
            }
        }

        // clamp
        if (x < 0) x = 0;
        else if (x > 255) x = 255;
        if (y < 0) y = 0;
        else if (y > 191) y = 191;

        // TODO: take advantage of possible extra precision when possible? (scaled window for example)
        NDS::TouchScreen(x, y);
    }
}

void OnAreaMouseCrossed(uiAreaHandler* handler, uiArea* area, int left)
{
}

void OnAreaDragBroken(uiAreaHandler* handler, uiArea* area)
{
}

int OnAreaKeyEvent(uiAreaHandler* handler, uiArea* area, uiAreaKeyEvent* evt)
{
    // TODO: release all keys if the window loses focus? or somehow global key input?
    if (evt->Scancode == 0x38) // ALT
        return 0;
    if (evt->Modifiers == 0x2) // ALT+key
        return 0;

    if (evt->Up)
    {
        for (int i = 0; i < 12; i++)
            if (evt->Scancode == Config::KeyMapping[i])
                KeyInputMask |= (1<<i);
    }
    else if (!evt->Repeat)
    {
        for (int i = 0; i < 12; i++)
            if (evt->Scancode == Config::KeyMapping[i])
                KeyInputMask &= ~(1<<i);
    }

    return 1;
}

void SetupScreenRects(int width, int height)
{
    bool horizontal = false;
    bool sideways = false;

    if (ScreenRotation == 1 || ScreenRotation == 3)
        sideways = true;

    if (ScreenLayout == 2) horizontal = true;
    else if (ScreenLayout == 0)
    {
        if (sideways)
            horizontal = true;
    }

    int sizemode;
    if (ScreenSizing == 3)
        sizemode = AutoScreenSizing;
    else
        sizemode = ScreenSizing;

    int screenW, screenH;
    if (sideways)
    {
        screenW = 192;
        screenH = 256;
    }
    else
    {
        screenW = 256;
        screenH = 192;
    }

    uiRect *topscreen, *bottomscreen;
    if (ScreenRotation == 1 || ScreenRotation == 2)
    {
        topscreen = &BottomScreenRect;
        bottomscreen = &TopScreenRect;
    }
    else
    {
        topscreen = &TopScreenRect;
        bottomscreen = &BottomScreenRect;
    }

    if (horizontal)
    {
        // side-by-side

        int heightreq;
        int startX = 0;

        width -= ScreenGap;

        if (sizemode == 0) // even
        {
            heightreq = (width * screenH) / (screenW*2);
            if (heightreq > height)
            {
                int newwidth = (height * width) / heightreq;
                startX = (width - newwidth) / 2;
                heightreq = height;
                width = newwidth;
            }
        }
        else // emph. top/bottom
        {
            heightreq = ((width - screenW) * screenH) / screenW;
            if (heightreq > height)
            {
                int newwidth = ((height * (width - screenW)) / heightreq) + screenW;
                startX = (width - newwidth) / 2;
                heightreq = height;
                width = newwidth;
            }
        }

        if (sizemode == 2)
        {
            topscreen->Width = screenW;
            topscreen->Height = screenH;
        }
        else
        {
            topscreen->Width = (sizemode==0) ? (width / 2) : (width - screenW);
            topscreen->Height = heightreq;
        }
        topscreen->X = startX;
        topscreen->Y = ((height - heightreq) / 2) + (heightreq - topscreen->Height);

        bottomscreen->X = topscreen->X + topscreen->Width + ScreenGap;

        if (sizemode == 1)
        {
            bottomscreen->Width = screenW;
            bottomscreen->Height = screenH;
        }
        else
        {
            bottomscreen->Width = width - topscreen->Width;
            bottomscreen->Height = heightreq;
        }
        bottomscreen->Y = ((height - heightreq) / 2) + (heightreq - bottomscreen->Height);
    }
    else
    {
        // top then bottom

        int widthreq;
        int startY = 0;

        height -= ScreenGap;

        if (sizemode == 0) // even
        {
            widthreq = (height * screenW) / (screenH*2);
            if (widthreq > width)
            {
                int newheight = (width * height) / widthreq;
                startY = (height - newheight) / 2;
                widthreq = width;
                height = newheight;
            }
        }
        else // emph. top/bottom
        {
            widthreq = ((height - screenH) * screenW) / screenH;
            if (widthreq > width)
            {
                int newheight = ((width * (height - screenH)) / widthreq) + screenH;
                startY = (height - newheight) / 2;
                widthreq = width;
                height = newheight;
            }
        }

        if (sizemode == 2)
        {
            topscreen->Width = screenW;
            topscreen->Height = screenH;
        }
        else
        {
            topscreen->Width = widthreq;
            topscreen->Height = (sizemode==0) ? (height / 2) : (height - screenH);
        }
        topscreen->Y = startY;
        topscreen->X = (width - topscreen->Width) / 2;

        bottomscreen->Y = topscreen->Y + topscreen->Height + ScreenGap;

        if (sizemode == 1)
        {
            bottomscreen->Width = screenW;
            bottomscreen->Height = screenH;
        }
        else
        {
            bottomscreen->Width = widthreq;
            bottomscreen->Height = height - topscreen->Height;
        }
        bottomscreen->X = (width - bottomscreen->Width) / 2;
    }

    // setup matrices for potential rotation

    uiDrawMatrixSetIdentity(&TopScreenTrans);
    uiDrawMatrixSetIdentity(&BottomScreenTrans);

    switch (ScreenRotation)
    {
    case 1: // 90°
        {
            uiDrawMatrixTranslate(&TopScreenTrans, -TopScreenRect.X, -TopScreenRect.Y);
            uiDrawMatrixRotate(&TopScreenTrans, 0, 0, M_PI/2.0f);
            uiDrawMatrixScale(&TopScreenTrans, 0, 0,
                              TopScreenRect.Width/(double)TopScreenRect.Height,
                              TopScreenRect.Height/(double)TopScreenRect.Width);
            uiDrawMatrixTranslate(&TopScreenTrans, TopScreenRect.X+TopScreenRect.Width, TopScreenRect.Y);

            uiDrawMatrixTranslate(&BottomScreenTrans, -BottomScreenRect.X, -BottomScreenRect.Y);
            uiDrawMatrixRotate(&BottomScreenTrans, 0, 0, M_PI/2.0f);
            uiDrawMatrixScale(&BottomScreenTrans, 0, 0,
                              BottomScreenRect.Width/(double)BottomScreenRect.Height,
                              BottomScreenRect.Height/(double)BottomScreenRect.Width);
            uiDrawMatrixTranslate(&BottomScreenTrans, BottomScreenRect.X+BottomScreenRect.Width, BottomScreenRect.Y);
        }
        break;

    case 2: // 180°
        {
            uiDrawMatrixTranslate(&TopScreenTrans, -TopScreenRect.X, -TopScreenRect.Y);
            uiDrawMatrixRotate(&TopScreenTrans, 0, 0, M_PI);
            uiDrawMatrixTranslate(&TopScreenTrans, TopScreenRect.X+TopScreenRect.Width, TopScreenRect.Y+TopScreenRect.Height);

            uiDrawMatrixTranslate(&BottomScreenTrans, -BottomScreenRect.X, -BottomScreenRect.Y);
            uiDrawMatrixRotate(&BottomScreenTrans, 0, 0, M_PI);
            uiDrawMatrixTranslate(&BottomScreenTrans, BottomScreenRect.X+BottomScreenRect.Width, BottomScreenRect.Y+BottomScreenRect.Height);
        }
        break;

    case 3: // 270°
        {
            uiDrawMatrixTranslate(&TopScreenTrans, -TopScreenRect.X, -TopScreenRect.Y);
            uiDrawMatrixRotate(&TopScreenTrans, 0, 0, -M_PI/2.0f);
            uiDrawMatrixScale(&TopScreenTrans, 0, 0,
                              TopScreenRect.Width/(double)TopScreenRect.Height,
                              TopScreenRect.Height/(double)TopScreenRect.Width);
            uiDrawMatrixTranslate(&TopScreenTrans, TopScreenRect.X, TopScreenRect.Y+TopScreenRect.Height);

            uiDrawMatrixTranslate(&BottomScreenTrans, -BottomScreenRect.X, -BottomScreenRect.Y);
            uiDrawMatrixRotate(&BottomScreenTrans, 0, 0, -M_PI/2.0f);
            uiDrawMatrixScale(&BottomScreenTrans, 0, 0,
                              BottomScreenRect.Width/(double)BottomScreenRect.Height,
                              BottomScreenRect.Height/(double)BottomScreenRect.Width);
            uiDrawMatrixTranslate(&BottomScreenTrans, BottomScreenRect.X, BottomScreenRect.Y+BottomScreenRect.Height);
        }
        break;
    }
}

void OnAreaResize(uiAreaHandler* handler, uiArea* area, int width, int height)
{
    SetupScreenRects(width, height);

    // TODO:
    // should those be the size of the uiArea, or the size of the window client area?
    // for now the uiArea fills the whole window anyway
    // but... we never know, I guess
    Config::WindowWidth = width;
    Config::WindowHeight = height;
}


void Run()
{
    EmuRunning = 1;
    RunningSomething = true;

    uiMenuItemEnable(MenuItem_Pause);
    uiMenuItemEnable(MenuItem_Reset);
    uiMenuItemEnable(MenuItem_Stop);
    uiMenuItemSetChecked(MenuItem_Pause, 0);
}

void Stop(bool internal)
{
    EmuRunning = 2;
    if (!internal) // if shutting down from the UI thread, wait till the emu thread has stopped
        while (EmuStatus != 2);
    RunningSomething = false;

    uiMenuItemDisable(MenuItem_Pause);
    uiMenuItemDisable(MenuItem_Reset);
    uiMenuItemDisable(MenuItem_Stop);
    uiMenuItemSetChecked(MenuItem_Pause, 0);

    memset(ScreenBuffer, 0, 256*384*4);
    uiAreaQueueRedrawAll(MainDrawArea);
}

void TryLoadROM(char* file, int prevstatus)
{
    char oldpath[1024];
    strncpy(oldpath, ROMPath, 1024);

    strncpy(ROMPath, file, 1023);
    ROMPath[1023] = '\0';

    if (NDS::LoadROM(ROMPath, Config::DirectBoot))
        Run();
    else
    {
        uiMsgBoxError(MainWindow,
                      "Failed to load the ROM",
                      "Make sure the file can be accessed and isn't opened in another application.");

        strncpy(ROMPath, oldpath, 1024);
        EmuRunning = prevstatus;
    }
}


int OnCloseWindow(uiWindow* window, void* blarg)
{
    uiQuit();
    return 1;
}

void OnDropFile(uiWindow* window, char* file, void* blarg)
{
    char* ext = &file[strlen(file)-3];
    int prevstatus = EmuRunning;

    if (!strcasecmp(ext, "nds") || !strcasecmp(ext, "srl"))
    {
        if (RunningSomething)
        {
            EmuRunning = 2;
            while (EmuStatus != 2);
        }

        TryLoadROM(file, prevstatus);
    }
}

void OnGetFocus(uiWindow* window, void* blarg)
{
    uiControlSetFocus(uiControl(MainDrawArea));
}

void OnLoseFocus(uiWindow* window, void* blarg)
{
    // TODO: shit here?
}

void OnCloseByMenu(uiMenuItem* item, uiWindow* window, void* blarg)
{
    uiControlDestroy(uiControl(window));
    uiQuit();
}

void OnOpenFile(uiMenuItem* item, uiWindow* window, void* blarg)
{
    int prevstatus = EmuRunning;
    EmuRunning = 2;
    while (EmuStatus != 2);

    char* file = uiOpenFile(window, "DS ROM (*.nds)|*.nds;*.srl|Any file|*.*", NULL);
    if (!file)
    {
        EmuRunning = prevstatus;
        return;
    }

    TryLoadROM(file, prevstatus);
    uiFreeText(file);
}

void OnRun(uiMenuItem* item, uiWindow* window, void* blarg)
{
    if (!RunningSomething)
    {
        ROMPath[0] = '\0';
        NDS::LoadBIOS();
    }

    Run();
}

void OnPause(uiMenuItem* item, uiWindow* window, void* blarg)
{
    if (!RunningSomething) return;

    if (EmuRunning == 1)
    {
        // enable pause
        EmuRunning = 2;
        uiMenuItemSetChecked(MenuItem_Pause, 1);
    }
    else
    {
        // disable pause
        EmuRunning = 1;
        uiMenuItemSetChecked(MenuItem_Pause, 0);
    }
}

void OnReset(uiMenuItem* item, uiWindow* window, void* blarg)
{
    if (!RunningSomething) return;

    EmuRunning = 2;
    while (EmuStatus != 2);

    if (ROMPath[0] == '\0')
        NDS::LoadBIOS();
    else
        NDS::LoadROM(ROMPath, Config::DirectBoot);

    Run();
}

void OnStop(uiMenuItem* item, uiWindow* window, void* blarg)
{
    if (!RunningSomething) return;

    Stop(false);
}

void OnOpenEmuSettings(uiMenuItem* item, uiWindow* window, void* blarg)
{
    DlgEmuSettings::Open();
}

void OnOpenInputConfig(uiMenuItem* item, uiWindow* window, void* blarg)
{
    DlgInputConfig::Open();
}


void OnSetScreenRotation(uiMenuItem* item, uiWindow* window, void* param)
{
    int rot = *(int*)param;

    int oldrot = ScreenRotation;
    ScreenRotation = rot;

    // TODO: adjust window size
    if (rot == 1 || rot == 3)
        uiControlSetMinSize(uiControl(MainDrawArea), 384, 256);
    else
        uiControlSetMinSize(uiControl(MainDrawArea), 256, 384);

    SetupScreenRects(Config::WindowWidth, Config::WindowHeight);

    for (int i = 0; i < 4; i++)
        uiMenuItemSetChecked(MenuItem_ScreenRot[i], i==ScreenRotation);
}

void OnSetScreenGap(uiMenuItem* item, uiWindow* window, void* param)
{
    int gap = *(int*)param;

    int oldgap = ScreenGap;
    ScreenGap = gap;

    // resize window as needed
    // TODO: adapt to horizontal modes
    // TODO: always resize window? except if it's maximized
    int w, h;
    uiWindowContentSize(window, &w, &h);
    {
        h -= gap;
        if (h < 384)
        {
            h = 384 + gap;
            uiWindowSetContentSize(window, w, h);
        }
    }

    SetupScreenRects(Config::WindowWidth, Config::WindowHeight);
}

void OnSetScreenLayout(uiMenuItem* item, uiWindow* window, void* param)
{
    int layout = *(int*)param;
    ScreenLayout = layout;
    // TODO trigger resize
}

void OnSetScreenSizing(uiMenuItem* item, uiWindow* window, void* param)
{
    int sizing = *(int*)param;
    ScreenSizing = sizing;

    SetupScreenRects(Config::WindowWidth, Config::WindowHeight);
}


void ApplyNewSettings()
{
    if (!RunningSomething) return;

    int prevstatus = EmuRunning;
    EmuRunning = 2;
    while (EmuStatus != 2);

    GPU3D::SoftRenderer::SetupRenderThread();

    if (Wifi::MPInited)
    {
        Platform::MP_DeInit();
        Platform::MP_Init();
    }

    EmuRunning = prevstatus;
}


bool _fileexists(char* name)
{
    FILE* f = fopen(name, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}


int main(int argc, char** argv)
{
    srand(time(NULL));

    printf("melonDS " MELONDS_VERSION "\n");
    printf(MELONDS_URL "\n");

    // http://stackoverflow.com/questions/14543333/joystick-wont-work-using-sdl
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_HAPTIC) < 0)
    {
        printf("SDL couldn't init rumble\n");
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
    {
        printf("SDL shat itself :(\n");
        return 1;
    }

    SDL_JoystickEventState(SDL_ENABLE);

    uiInitOptions ui_opt;
    memset(&ui_opt, 0, sizeof(uiInitOptions));
    const char* ui_err = uiInit(&ui_opt);
    if (ui_err != NULL)
    {
        printf("libui shat itself :( %s\n", ui_err);
        uiFreeInitError(ui_err);
        return 1;
    }

    Config::Load();

    if (!_fileexists("bios7.bin") || !_fileexists("bios9.bin") || !_fileexists("firmware.bin"))
    {
        uiMsgBoxError(
            NULL,
            "BIOS/Firmware not found",
            "One or more of the following required files don't exist or couldn't be accessed:\n\n"
            "bios7.bin -- ARM7 BIOS\n"
            "bios9.bin -- ARM9 BIOS\n"
            "firmware.bin -- firmware image\n\n"
            "Dump the files from your DS and place them in the directory you run melonDS from.\n"
            "Make sure that the files can be accessed.");

        uiUninit();
        SDL_Quit();
        return 0;
    }

    uiMenu* menu;
    uiMenuItem* menuitem;

    menu = uiNewMenu("File");
    menuitem = uiMenuAppendItem(menu, "Open ROM...");
    uiMenuItemOnClicked(menuitem, OnOpenFile, NULL);
    uiMenuAppendSeparator(menu);
    menuitem = uiMenuAppendItem(menu, "Quit");
    uiMenuItemOnClicked(menuitem, OnCloseByMenu, NULL);

    menu = uiNewMenu("System");
    menuitem = uiMenuAppendItem(menu, "Run");
    uiMenuItemOnClicked(menuitem, OnRun, NULL);
    menuitem = uiMenuAppendCheckItem(menu, "Pause");
    uiMenuItemOnClicked(menuitem, OnPause, NULL);
    MenuItem_Pause = menuitem;
    uiMenuAppendSeparator(menu);
    menuitem = uiMenuAppendItem(menu, "Reset");
    uiMenuItemOnClicked(menuitem, OnReset, NULL);
    MenuItem_Reset = menuitem;
    menuitem = uiMenuAppendItem(menu, "Stop");
    uiMenuItemOnClicked(menuitem, OnStop, NULL);
    MenuItem_Stop = menuitem;

    menu = uiNewMenu("Config");
    menuitem = uiMenuAppendItem(menu, "Emu settings");
    uiMenuItemOnClicked(menuitem, OnOpenEmuSettings, NULL);
    menuitem = uiMenuAppendItem(menu, "Input config");
    uiMenuItemOnClicked(menuitem, OnOpenInputConfig, NULL);
    uiMenuAppendSeparator(menu);
    {
        uiMenu* submenu = uiNewMenu("Screen rotation");

        for (int i = 0; i < 4; i++)
        {
            char name[32];
            sprintf(name, "%d", kScreenRot[i]*90);
            MenuItem_ScreenRot[i] = uiMenuAppendItem(submenu, name);
            uiMenuItemOnClicked(MenuItem_ScreenRot[i], OnSetScreenRotation, (void*)&kScreenRot[i]);
        }

        uiMenuAppendSubmenu(menu, submenu);
    }
    {
        uiMenu* submenu = uiNewMenu("Mid-screen gap");

        //for (int i = 0; kScreenGap[i] != -1; i++)
        for (int i = 0; i < 6; i++)
        {
            char name[32];
            sprintf(name, "%d pixels", kScreenGap[i]);
            MenuItem_ScreenGap[i] = uiMenuAppendItem(submenu, name);
            uiMenuItemOnClicked(MenuItem_ScreenGap[i], OnSetScreenGap, (void*)&kScreenGap[i]);
        }

        uiMenuAppendSubmenu(menu, submenu);
    }
    {
        uiMenu* submenu = uiNewMenu("Screen layout");

        MenuItem_ScreenLayout[0] = uiMenuAppendItem(submenu, "Natural");
        uiMenuItemOnClicked(MenuItem_ScreenLayout[0], OnSetScreenLayout, (void*)&kScreenLayout[0]);
        MenuItem_ScreenLayout[1] = uiMenuAppendItem(submenu, "Vertical");
        uiMenuItemOnClicked(MenuItem_ScreenLayout[1], OnSetScreenLayout, (void*)&kScreenLayout[1]);
        MenuItem_ScreenLayout[2] = uiMenuAppendItem(submenu, "Horizontal");
        uiMenuItemOnClicked(MenuItem_ScreenLayout[2], OnSetScreenLayout, (void*)&kScreenLayout[2]);

        uiMenuAppendSubmenu(menu, submenu);
    }
    {
        uiMenu* submenu = uiNewMenu("Screen sizing");

        MenuItem_ScreenSizing[0] = uiMenuAppendItem(submenu, "Even");
        uiMenuItemOnClicked(MenuItem_ScreenSizing[0], OnSetScreenSizing, (void*)&kScreenSizing[0]);
        MenuItem_ScreenSizing[1] = uiMenuAppendItem(submenu, "Emphasize top");
        uiMenuItemOnClicked(MenuItem_ScreenSizing[1], OnSetScreenSizing, (void*)&kScreenSizing[1]);
        MenuItem_ScreenSizing[2] = uiMenuAppendItem(submenu, "Emphasize bottom");
        uiMenuItemOnClicked(MenuItem_ScreenSizing[2], OnSetScreenSizing, (void*)&kScreenSizing[2]);
        MenuItem_ScreenSizing[3] = uiMenuAppendItem(submenu, "Auto");
        uiMenuItemOnClicked(MenuItem_ScreenSizing[3], OnSetScreenSizing, (void*)&kScreenSizing[3]);

        uiMenuAppendSubmenu(menu, submenu);
    }


    int w = Config::WindowWidth;
    int h = Config::WindowHeight;
    if (w < 256) w = 256;
    if (h < 384) h = 384;

    MainWindow = uiNewWindow("melonDS " MELONDS_VERSION, w, h, 1, 1);
    uiWindowOnClosing(MainWindow, OnCloseWindow, NULL);

    uiWindowSetDropTarget(MainWindow, 1);
    uiWindowOnDropFile(MainWindow, OnDropFile, NULL);

    uiWindowOnGetFocus(MainWindow, OnGetFocus, NULL);
    uiWindowOnLoseFocus(MainWindow, OnLoseFocus, NULL);

    uiMenuItemDisable(MenuItem_Pause);
    uiMenuItemDisable(MenuItem_Reset);
    uiMenuItemDisable(MenuItem_Stop);

    uiAreaHandler areahandler;
    areahandler.Draw = OnAreaDraw;
    areahandler.MouseEvent = OnAreaMouseEvent;
    areahandler.MouseCrossed = OnAreaMouseCrossed;
    areahandler.DragBroken = OnAreaDragBroken;
    areahandler.KeyEvent = OnAreaKeyEvent;
    areahandler.Resize = OnAreaResize;

    MainDrawArea = uiNewArea(&areahandler);
    uiWindowSetChild(MainWindow, uiControl(MainDrawArea));
    uiControlSetMinSize(uiControl(MainDrawArea), 256, 384);

    EmuRunning = 2;
    RunningSomething = false;
    EmuThread = SDL_CreateThread(EmuThreadFunc, "melonDS magic", NULL);

    if (argc > 1)
    {
        char* file = argv[1];
        char* ext = &file[strlen(file)-3];

        if (!strcasecmp(ext, "nds") || !strcasecmp(ext, "srl"))
        {
            strncpy(ROMPath, file, 1023);
            ROMPath[1023] = '\0';

            if (NDS::LoadROM(ROMPath, Config::DirectBoot))
                Run();
        }
    }

    uiControlShow(uiControl(MainWindow));
    uiControlSetFocus(uiControl(MainDrawArea));
    uiMain();

    EmuRunning = 0;
    SDL_WaitThread(EmuThread, NULL);

    Config::Save();

    if (ScreenBitmap) uiDrawFreeBitmap(ScreenBitmap);

    uiUninit();
    SDL_Quit();
    return 0;
}

#ifdef __WIN32__

#include <windows.h>

int CALLBACK WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int cmdshow)
{
    char cmdargs[16][256];
    int arg = 1;
    int j = 0;
    bool inquote = false;
    int len = strlen(cmdline);
    for (int i = 0; i < len; i++)
    {
        char c = cmdline[i];
        if (c == '\0') break;
        if (c == '"') inquote = !inquote;
        if (!inquote && c==' ')
        {
            if (j > 255) j = 255;
            if (arg < 16) cmdargs[arg][j] = '\0';
            arg++;
            j = 0;
        }
        else
        {
            if (arg < 16 && j < 255) cmdargs[arg][j] = c;
            j++;
        }
    }
    if (j > 255) j = 255;
    if (arg < 16) cmdargs[arg][j] = '\0';
    if (len > 0) arg++;

    // FIXME!!
    strncpy(cmdargs[0], "melonDS.exe", 256);

    char* cmdargptr[16];
    for (int i = 0; i < 16; i++)
        cmdargptr[i] = &cmdargs[i][0];

    return main(arg, cmdargptr);
}

#endif
