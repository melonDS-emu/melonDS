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

#include "../NDS.h"
#include "../GPU.h"
#include "../SPU.h"


uiWindow* MainWindow;
uiArea* MainDrawArea;

uiMenuItem* MenuItem_Pause;
uiMenuItem* MenuItem_Reset;
uiMenuItem* MenuItem_Stop;

SDL_Thread* EmuThread;
int EmuRunning;

bool RunningSomething;
char ROMPath[1024];

bool ScreenDrawInited = false;
uiDrawBitmap* ScreenBitmap = NULL;
u32 ScreenBuffer[256*384];

bool Touching = false;


void AudioCallback(void* data, Uint8* stream, int len)
{
    SPU::ReadOutput((s16*)stream, len>>2);
}

int EmuThreadFunc(void* burp)
{
    NDS::Init();

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

    u32 nframes = 0;
    u32 starttick = SDL_GetTicks();
    u32 lasttick = starttick;
    u32 lastmeasuretick = lasttick;
    u32 fpslimitcount = 0;
    bool limitfps = true;

    while (EmuRunning != 0)
    {
        if (EmuRunning == 1)
        {
            // emulate
            u32 nlines = NDS::RunFrame();

            if (EmuRunning == 0) break;

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

                char melontitle[100];
                sprintf(melontitle, "%d/%.0f FPS | melonDS " MELONDS_VERSION, fps, fpstarget);
                //uiWindowSetTitle(MainWindow, melontitle);
            }
        }
        else
        {
            // paused
            nframes = 0;
            lasttick = SDL_GetTicks();
            starttick = lasttick;
            lastmeasuretick = lasttick;
            fpslimitcount = 0;

            uiAreaQueueRedrawAll(MainDrawArea);
            SDL_Delay(50);
        }
    }

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

    uiRect dorp = {0, 0, 256, 384};

    uiDrawBitmapUpdate(ScreenBitmap, ScreenBuffer);

    uiDrawBitmapDraw(params->Context, ScreenBitmap, &dorp, &dorp);
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
    else if (!Touching && (evt->Down == 1) && (y >= 192))
    {
        Touching = true;
        NDS::PressKey(16+6);
        // TODO: scaling/offset as needed
    }

    if (Touching)
    {
        // TODO: scaling, here too
        y -= 192;

        // clamp
        if (x < 0) x = 0;
        else if (x > 255) x = 255;
        if (y < 0) y = 0;
        else if (y > 191) y = 191;

        NDS::TouchScreen(x, y);
    }
}

void OnAreaMouseCrossed(uiAreaHandler* handler, uiArea* area, int left)
{
    //
}

void OnAreaDragBroken(uiAreaHandler* handler, uiArea* area)
{
    //
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
        for (int i = 0; i < 10; i++)
            if (evt->Scancode == Config::KeyMapping[i]) NDS::ReleaseKey(i);
        if (evt->Scancode == Config::KeyMapping[10]) NDS::ReleaseKey(16);
        if (evt->Scancode == Config::KeyMapping[11]) NDS::ReleaseKey(17);
    }
    else if (!evt->Repeat)
    {
        for (int i = 0; i < 10; i++)
            if (evt->Scancode == Config::KeyMapping[i]) NDS::PressKey(i);
        if (evt->Scancode == Config::KeyMapping[10]) NDS::PressKey(16);
        if (evt->Scancode == Config::KeyMapping[11]) NDS::PressKey(17);
    }

    return 1;
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

void Stop()
{
    EmuRunning = 2;
    RunningSomething = false;

    uiMenuItemDisable(MenuItem_Pause);
    uiMenuItemDisable(MenuItem_Reset);
    uiMenuItemDisable(MenuItem_Stop);
    uiMenuItemSetChecked(MenuItem_Pause, 0);

    memset(ScreenBuffer, 0, 256*384*4);
    uiAreaQueueRedrawAll(MainDrawArea);
}


int OnCloseWindow(uiWindow* window, void* blarg)
{
    uiQuit();
    return 1;
}

void OnCloseByMenu(uiMenuItem* item, uiWindow* window, void* blarg)
{
    // TODO????
    // uiQuit() crashes
}

void OnOpenFile(uiMenuItem* item, uiWindow* window, void* blarg)
{
    int prevstatus = EmuRunning;
    EmuRunning = 2;
    // TODO: ensure the emu thread has indeed stopped at this point

    char* file = uiOpenFile(window, "DS ROM (*.nds)|*.nds;*.srl|Any file|*.*", NULL);
    if (!file)
    {
        EmuRunning = prevstatus;
        return;
    }

    strncpy(ROMPath, file, 1023);
    ROMPath[1023] = '\0';
    uiFreeText(file);
    // TODO: change libui to store strings in stack-allocated buffers?
    // so we don't have to free it after use

    NDS::LoadROM(ROMPath, Config::DirectBoot);

    Run();
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

    if (ROMPath[0] == '\0')
        NDS::LoadBIOS();
    else
        NDS::LoadROM(ROMPath, Config::DirectBoot);

    Run();
}

void OnStop(uiMenuItem* item, uiWindow* window, void* blarg)
{
    if (!RunningSomething) return;

    Stop();
}


int main(int argc, char** argv)
{
    srand(time(NULL));

    printf("melonDS " MELONDS_VERSION "\n");
    printf(MELONDS_URL "\n");

    // http://stackoverflow.com/questions/14543333/joystick-wont-work-using-sdl
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        printf("SDL shat itself :(\n");
        return 1;
    }

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

    MainWindow = uiNewWindow("melonDS " MELONDS_VERSION, 256, 384, 1);
    uiWindowOnClosing(MainWindow, OnCloseWindow, NULL);

    uiMenuItemDisable(MenuItem_Pause);
    uiMenuItemDisable(MenuItem_Reset);
    uiMenuItemDisable(MenuItem_Stop);

    uiAreaHandler areahandler;

    areahandler.Draw = OnAreaDraw;
    areahandler.MouseEvent = OnAreaMouseEvent;
    areahandler.MouseCrossed = OnAreaMouseCrossed;
    areahandler.DragBroken = OnAreaDragBroken;
    areahandler.KeyEvent = OnAreaKeyEvent;

    MainDrawArea = uiNewArea(&areahandler);
    uiWindowSetChild(MainWindow, uiControl(MainDrawArea));

    EmuRunning = 2;
    RunningSomething = false;
    EmuThread = SDL_CreateThread(EmuThreadFunc, "melonDS magic", NULL);

    uiControlShow(uiControl(MainWindow));
    //uiControlSetFocus(uiControl(MainDrawArea)); // TODO: this needs to be done when the window regains focus
    uiMain();

    EmuRunning = 0;
    SDL_WaitThread(EmuThread, NULL);

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
    int arg = 0;
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

    return main(arg, (char**)cmdargs);
}

#endif
