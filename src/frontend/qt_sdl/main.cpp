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

#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include <optional>
#include <vector>
#include <string>
#include <algorithm>

#include <QProcess>
#include <QApplication>
#include <QMessageBox>
#include <QMenuBar>
#include <QMimeDatabase>
#include <QFileDialog>
#include <QInputDialog>
#include <QPaintEvent>
#include <QPainter>
#include <QKeyEvent>
#include <QMimeData>
#include <QVector>
#include <QCommandLineParser>
#ifndef _WIN32
#include <QGuiApplication>
#include <QSocketNotifier>
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#ifndef APPLE
#include <qpa/qplatformnativeinterface.h>
#endif
#endif

#include <SDL2/SDL.h>

#include "OpenGLSupport.h"
#include "duckstation/gl/context.h"

#include "main.h"
#include "Input.h"
#include "CheatsDialog.h"
#include "EmuSettingsDialog.h"
#include "InputConfig/InputConfigDialog.h"
#include "VideoSettingsDialog.h"
#include "CameraSettingsDialog.h"
#include "AudioSettingsDialog.h"
#include "FirmwareSettingsDialog.h"
#include "PathSettingsDialog.h"
#include "MPSettingsDialog.h"
#include "WifiSettingsDialog.h"
#include "InterfaceSettingsDialog.h"
#include "ROMInfoDialog.h"
#include "RAMInfoDialog.h"
#include "TitleManagerDialog.h"
#include "PowerManagement/PowerManagementDialog.h"
#include "AudioInOut.h"

#include "types.h"
#include "version.h"

#include "FrontendUtil.h"
#include "OSD.h"

#include "NDS.h"
#include "NDSCart.h"
#include "GBACart.h"
#include "GPU.h"
#include "SPU.h"
#include "Wifi.h"
#include "Platform.h"
#include "LocalMP.h"
#include "Config.h"
#include "DSi_I2C.h"

#include "Savestate.h"

#include "main_shaders.h"

#include "ROMManager.h"
#include "ArchiveUtil.h"
#include "CameraManager.h"

#include "CLI.h"

// TODO: uniform variable spelling

const QString NdsRomMimeType = "application/x-nintendo-ds-rom";
const QStringList NdsRomExtensions { ".nds", ".srl", ".dsi", ".ids" };

const QString GbaRomMimeType = "application/x-gba-rom";
const QStringList GbaRomExtensions { ".gba", ".agb" };

// This list of supported archive formats is based on libarchive(3) version 3.6.2 (2022-12-09).
const QStringList ArchiveMimeTypes
{
#ifdef ARCHIVE_SUPPORT_ENABLED
    "application/zip",
    "application/x-7z-compressed",
    "application/vnd.rar", // *.rar
    "application/x-tar",

    "application/x-compressed-tar", // *.tar.gz
    "application/x-xz-compressed-tar",
    "application/x-bzip-compressed-tar",
    "application/x-lz4-compressed-tar",
    "application/x-zstd-compressed-tar",

    "application/x-tarz", // *.tar.Z
    "application/x-lzip-compressed-tar",
    "application/x-lzma-compressed-tar",
    "application/x-lrzip-compressed-tar",
    "application/x-tzo", // *.tar.lzo
#endif
};

const QStringList ArchiveExtensions
{
#ifdef ARCHIVE_SUPPORT_ENABLED
    ".zip", ".7z", ".rar", ".tar",

    ".tar.gz", ".tgz",
    ".tar.xz", ".txz",
    ".tar.bz2", ".tbz2",
    ".tar.lz4", ".tlz4",
    ".tar.zst", ".tzst",

    ".tar.Z", ".taz",
    ".tar.lz",
    ".tar.lzma", ".tlz",
    ".tar.lrz", ".tlrz",
    ".tar.lzo", ".tzo"
#endif
};


bool RunningSomething;

MainWindow* mainWindow;
EmuThread* emuThread;

int autoScreenSizing = 0;

int videoRenderer;
GPU::RenderSettings videoSettings;
bool videoSettingsDirty;

CameraManager* camManager[2];
bool camStarted[2];

const struct { int id; float ratio; const char* label; } aspectRatios[] =
{
    { 0, 1,                       "4:3 (native)" },
    { 4, (5.f  / 3) / (4.f / 3), "5:3 (3DS)"},
    { 1, (16.f / 9) / (4.f / 3),  "16:9" },
    { 2, (21.f / 9) / (4.f / 3),  "21:9" },
    { 3, 0,                       "window" }
};
constexpr int AspectRatiosNum = sizeof(aspectRatios) / sizeof(aspectRatios[0]);


EmuThread::EmuThread(QObject* parent) : QThread(parent)
{
    EmuStatus = emuStatus_Exit;
    EmuRunning = emuStatus_Paused;
    EmuPauseStack = EmuPauseStackRunning;
    RunningSomething = false;

    connect(this, SIGNAL(windowUpdate()), mainWindow->panelWidget, SLOT(repaint()));
    connect(this, SIGNAL(windowTitleChange(QString)), mainWindow, SLOT(onTitleUpdate(QString)));
    connect(this, SIGNAL(windowEmuStart()), mainWindow, SLOT(onEmuStart()));
    connect(this, SIGNAL(windowEmuStop()), mainWindow, SLOT(onEmuStop()));
    connect(this, SIGNAL(windowEmuPause()), mainWindow->actPause, SLOT(trigger()));
    connect(this, SIGNAL(windowEmuReset()), mainWindow->actReset, SLOT(trigger()));
    connect(this, SIGNAL(windowEmuFrameStep()), mainWindow->actFrameStep, SLOT(trigger()));
    connect(this, SIGNAL(windowLimitFPSChange()), mainWindow->actLimitFramerate, SLOT(trigger()));
    connect(this, SIGNAL(screenLayoutChange()), mainWindow->panelWidget, SLOT(onScreenLayoutChanged()));
    connect(this, SIGNAL(windowFullscreenToggle()), mainWindow, SLOT(onFullscreenToggled()));
    connect(this, SIGNAL(swapScreensToggle()), mainWindow->actScreenSwap, SLOT(trigger()));
    connect(this, SIGNAL(screenEmphasisToggle()), mainWindow, SLOT(onScreenEmphasisToggled()));

    static_cast<ScreenPanelGL*>(mainWindow->panel)->transferLayout(this);
}

void EmuThread::updateScreenSettings(bool filter, const WindowInfo& windowInfo, int numScreens, int* screenKind, float* screenMatrix)
{
    screenSettingsLock.lock();

    if (lastScreenWidth != windowInfo.surface_width || lastScreenHeight != windowInfo.surface_height)
    {
        if (oglContext)
            oglContext->ResizeSurface(windowInfo.surface_width, windowInfo.surface_height);
        lastScreenWidth = windowInfo.surface_width;
        lastScreenHeight = windowInfo.surface_height;
    }

    this->filter = filter;
    this->windowInfo = windowInfo;
    this->numScreens = numScreens;
    memcpy(this->screenKind, screenKind, sizeof(int)*numScreens);
    memcpy(this->screenMatrix, screenMatrix, sizeof(float)*numScreens*6);

    screenSettingsLock.unlock();
}

void EmuThread::initOpenGL()
{
    GL::Context* windowctx = mainWindow->getOGLContext();

    oglContext = windowctx;
    oglContext->MakeCurrent();

    OpenGL::BuildShaderProgram(kScreenVS, kScreenFS, screenShaderProgram, "ScreenShader");
    GLuint pid = screenShaderProgram[2];
    glBindAttribLocation(pid, 0, "vPosition");
    glBindAttribLocation(pid, 1, "vTexcoord");
    glBindFragDataLocation(pid, 0, "oColor");

    OpenGL::LinkShaderProgram(screenShaderProgram);

    glUseProgram(pid);
    glUniform1i(glGetUniformLocation(pid, "ScreenTex"), 0);

    screenShaderScreenSizeULoc = glGetUniformLocation(pid, "uScreenSize");
    screenShaderTransformULoc = glGetUniformLocation(pid, "uTransform");

    // to prevent bleeding between both parts of the screen
    // with bilinear filtering enabled
    const int paddedHeight = 192*2+2;
    const float padPixels = 1.f / paddedHeight;

    const float vertices[] =
    {
        0.f,   0.f,    0.f, 0.f,
        0.f,   192.f,  0.f, 0.5f - padPixels,
        256.f, 192.f,  1.f, 0.5f - padPixels,
        0.f,   0.f,    0.f, 0.f,
        256.f, 192.f,  1.f, 0.5f - padPixels,
        256.f, 0.f,    1.f, 0.f,

        0.f,   0.f,    0.f, 0.5f + padPixels,
        0.f,   192.f,  0.f, 1.f,
        256.f, 192.f,  1.f, 1.f,
        0.f,   0.f,    0.f, 0.5f + padPixels,
        256.f, 192.f,  1.f, 1.f,
        256.f, 0.f,    1.f, 0.5f + padPixels
    };

    glGenBuffers(1, &screenVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, screenVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &screenVertexArray);
    glBindVertexArray(screenVertexArray);
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*4, (void*)(0));
    glEnableVertexAttribArray(1); // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*4, (void*)(2*4));

    glGenTextures(1, &screenTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, paddedHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    // fill the padding
    u8 zeroData[256*4*4];
    memset(zeroData, 0, sizeof(zeroData));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192, 256, 2, GL_RGBA, GL_UNSIGNED_BYTE, zeroData);

    OSD::Init(true);

    oglContext->SetSwapInterval(Config::ScreenVSync ? Config::ScreenVSyncInterval : 0);
}

void EmuThread::deinitOpenGL()
{
    glDeleteTextures(1, &screenTexture);

    glDeleteVertexArrays(1, &screenVertexArray);
    glDeleteBuffers(1, &screenVertexBuffer);

    OpenGL::DeleteShaderProgram(screenShaderProgram);

    OSD::DeInit();

    oglContext->DoneCurrent();
    oglContext = nullptr;

    lastScreenWidth = lastScreenHeight = -1;
}

void EmuThread::run()
{
    u32 mainScreenPos[3];

    NDS::Init();

    mainScreenPos[0] = 0;
    mainScreenPos[1] = 0;
    mainScreenPos[2] = 0;
    autoScreenSizing = 0;

    videoSettingsDirty = false;
    videoSettings.Soft_Threaded = Config::Threaded3D != 0;
    videoSettings.GL_ScaleFactor = Config::GL_ScaleFactor;
    videoSettings.GL_BetterPolygons = Config::GL_BetterPolygons;

    if (mainWindow->hasOGL)
    {
        initOpenGL();
        videoRenderer = Config::_3DRenderer;
    }
    else
    {
        videoRenderer = 0;
    }

    GPU::InitRenderer(videoRenderer);
    GPU::SetRenderSettings(videoRenderer, videoSettings);

    SPU::SetInterpolation(Config::AudioInterp);

    Input::Init();

    u32 nframes = 0;
    double perfCountsSec = 1.0 / SDL_GetPerformanceFrequency();
    double lastTime = SDL_GetPerformanceCounter() * perfCountsSec;
    double frameLimitError = 0.0;
    double lastMeasureTime = lastTime;

    u32 winUpdateCount = 0, winUpdateFreq = 1;
    u8 dsiVolumeLevel = 0x1F;

    char melontitle[100];

    while (EmuRunning != emuStatus_Exit)
    {
        Input::Process();

        if (Input::HotkeyPressed(HK_FastForwardToggle)) emit windowLimitFPSChange();

        if (Input::HotkeyPressed(HK_Pause)) emit windowEmuPause();
        if (Input::HotkeyPressed(HK_Reset)) emit windowEmuReset();
        if (Input::HotkeyPressed(HK_FrameStep)) emit windowEmuFrameStep();

        if (Input::HotkeyPressed(HK_FullscreenToggle)) emit windowFullscreenToggle();

        if (Input::HotkeyPressed(HK_SwapScreens)) emit swapScreensToggle();
        if (Input::HotkeyPressed(HK_SwapScreenEmphasis)) emit screenEmphasisToggle();

        if (Input::HotkeyPressed(HK_SolarSensorDecrease))
        {
            int level = GBACart::SetInput(GBACart::Input_SolarSensorDown, true);
            if (level != -1)
            {
                char msg[64];
                sprintf(msg, "Solar sensor level: %d", level);
                OSD::AddMessage(0, msg);
            }
        }
        if (Input::HotkeyPressed(HK_SolarSensorIncrease))
        {
            int level = GBACart::SetInput(GBACart::Input_SolarSensorUp, true);
            if (level != -1)
            {
                char msg[64];
                sprintf(msg, "Solar sensor level: %d", level);
                OSD::AddMessage(0, msg);
            }
        }

        if (NDS::ConsoleType == 1)
        {
            double currentTime = SDL_GetPerformanceCounter() * perfCountsSec;

            // Handle power button
            if (Input::HotkeyDown(HK_PowerButton))
            {
                DSi_BPTWL::SetPowerButtonHeld(currentTime);
            }
            else if (Input::HotkeyReleased(HK_PowerButton))
            {
                DSi_BPTWL::SetPowerButtonReleased(currentTime);
            }

            // Handle volume buttons
            if (Input::HotkeyDown(HK_VolumeUp))
            {
                DSi_BPTWL::SetVolumeSwitchHeld(DSi_BPTWL::volumeKey_Up);
            }
            else if (Input::HotkeyReleased(HK_VolumeUp))
            {
                DSi_BPTWL::SetVolumeSwitchReleased(DSi_BPTWL::volumeKey_Up);
            }

            if (Input::HotkeyDown(HK_VolumeDown))
            {
                DSi_BPTWL::SetVolumeSwitchHeld(DSi_BPTWL::volumeKey_Down);
            }
            else if (Input::HotkeyReleased(HK_VolumeDown))
            {
                DSi_BPTWL::SetVolumeSwitchReleased(DSi_BPTWL::volumeKey_Down);
            }

            DSi_BPTWL::ProcessVolumeSwitchInput(currentTime);
        }

        if (EmuRunning == emuStatus_Running || EmuRunning == emuStatus_FrameStep)
        {
            EmuStatus = emuStatus_Running;
            if (EmuRunning == emuStatus_FrameStep) EmuRunning = emuStatus_Paused;

            // update render settings if needed
            // HACK:
            // once the fast forward hotkey is released, we need to update vsync
            // to the old setting again
            if (videoSettingsDirty || Input::HotkeyReleased(HK_FastForward))
            {
                if (oglContext)
                {
                    oglContext->SetSwapInterval(Config::ScreenVSync ? Config::ScreenVSyncInterval : 0);
                    videoRenderer = Config::_3DRenderer;
                }
#ifdef OGLRENDERER_ENABLED
                else
#endif
                {
                    videoRenderer = 0;
                }

                videoRenderer = oglContext ? Config::_3DRenderer : 0;

                videoSettingsDirty = false;

                videoSettings.Soft_Threaded = Config::Threaded3D != 0;
                videoSettings.GL_ScaleFactor = Config::GL_ScaleFactor;
                videoSettings.GL_BetterPolygons = Config::GL_BetterPolygons;

                GPU::SetRenderSettings(videoRenderer, videoSettings);
            }

            // process input and hotkeys
            NDS::SetKeyMask(Input::InputMask);

            if (Input::HotkeyPressed(HK_Lid))
            {
                bool lid = !NDS::IsLidClosed();
                NDS::SetLidClosed(lid);
                OSD::AddMessage(0, lid ? "Lid closed" : "Lid opened");
            }

            // microphone input
            AudioInOut::MicProcess();

            // auto screen layout
            if (Config::ScreenSizing == Frontend::screenSizing_Auto)
            {
                mainScreenPos[2] = mainScreenPos[1];
                mainScreenPos[1] = mainScreenPos[0];
                mainScreenPos[0] = NDS::PowerControl9 >> 15;

                int guess;
                if (mainScreenPos[0] == mainScreenPos[2] &&
                    mainScreenPos[0] != mainScreenPos[1])
                {
                    // constant flickering, likely displaying 3D on both screens
                    // TODO: when both screens are used for 2D only...???
                    guess = Frontend::screenSizing_Even;
                }
                else
                {
                    if (mainScreenPos[0] == 1)
                        guess = Frontend::screenSizing_EmphTop;
                    else
                        guess = Frontend::screenSizing_EmphBot;
                }

                if (guess != autoScreenSizing)
                {
                    autoScreenSizing = guess;
                    emit screenLayoutChange();
                }
            }


            // emulate
            u32 nlines = NDS::RunFrame();

            if (ROMManager::NDSSave)
                ROMManager::NDSSave->CheckFlush();

            if (ROMManager::GBASave)
                ROMManager::GBASave->CheckFlush();

            if (ROMManager::FirmwareSave)
                ROMManager::FirmwareSave->CheckFlush();

            if (!oglContext)
            {
                FrontBufferLock.lock();
                FrontBuffer = GPU::FrontBuffer;
                FrontBufferLock.unlock();
            }
            else
            {
                FrontBuffer = GPU::FrontBuffer;
                drawScreenGL();
            }

#ifdef MELONCAP
            MelonCap::Update();
#endif // MELONCAP

            if (EmuRunning == emuStatus_Exit) break;

            winUpdateCount++;
            if (winUpdateCount >= winUpdateFreq && !oglContext)
            {
                emit windowUpdate();
                winUpdateCount = 0;
            }

            bool fastforward = Input::HotkeyDown(HK_FastForward);

            if (fastforward && oglContext && Config::ScreenVSync)
            {
                oglContext->SetSwapInterval(0);
            }

            if (Config::DSiVolumeSync && NDS::ConsoleType == 1)
            {
                u8 volumeLevel = DSi_BPTWL::GetVolumeLevel();
                if (volumeLevel != dsiVolumeLevel)
                {
                    dsiVolumeLevel = volumeLevel;
                    emit syncVolumeLevel();
                }

                Config::AudioVolume = volumeLevel * (256.0 / 31.0);
            }

            if (Config::AudioSync && !fastforward)
                AudioInOut::AudioSync();

            double frametimeStep = nlines / (60.0 * 263.0);

            {
                bool limitfps = Config::LimitFPS && !fastforward;

                double practicalFramelimit = limitfps ? frametimeStep : 1.0 / 1000.0;

                double curtime = SDL_GetPerformanceCounter() * perfCountsSec;

                frameLimitError += practicalFramelimit - (curtime - lastTime);
                if (frameLimitError < -practicalFramelimit)
                    frameLimitError = -practicalFramelimit;
                if (frameLimitError > practicalFramelimit)
                    frameLimitError = practicalFramelimit;

                if (round(frameLimitError * 1000.0) > 0.0)
                {
                    SDL_Delay(round(frameLimitError * 1000.0));
                    double timeBeforeSleep = curtime;
                    curtime = SDL_GetPerformanceCounter() * perfCountsSec;
                    frameLimitError -= curtime - timeBeforeSleep;
                }

                lastTime = curtime;
            }

            nframes++;
            if (nframes >= 30)
            {
                double time = SDL_GetPerformanceCounter() * perfCountsSec;
                double dt = time - lastMeasureTime;
                lastMeasureTime = time;

                u32 fps = round(nframes / dt);
                nframes = 0;

                float fpstarget = 1.0/frametimeStep;

                winUpdateFreq = fps / (u32)round(fpstarget);
                if (winUpdateFreq < 1)
                    winUpdateFreq = 1;

                int inst = Platform::InstanceID();
                if (inst == 0)
                    sprintf(melontitle, "[%d/%.0f] melonDS " MELONDS_VERSION, fps, fpstarget);
                else
                    sprintf(melontitle, "[%d/%.0f] melonDS (%d)", fps, fpstarget, inst+1);
                changeWindowTitle(melontitle);
            }
        }
        else
        {
            // paused
            nframes = 0;
            lastTime = SDL_GetPerformanceCounter() * perfCountsSec;
            lastMeasureTime = lastTime;

            emit windowUpdate();

            EmuStatus = EmuRunning;

            int inst = Platform::InstanceID();
            if (inst == 0)
                sprintf(melontitle, "melonDS " MELONDS_VERSION);
            else
                sprintf(melontitle, "melonDS (%d)", inst+1);
            changeWindowTitle(melontitle);

            SDL_Delay(75);

            if (oglContext)
                drawScreenGL();

            ContextRequestKind contextRequest = ContextRequest;
            if (contextRequest == contextRequest_InitGL)
            {
                initOpenGL();
                ContextRequest = contextRequest_None;
            }
            else if (contextRequest == contextRequest_DeInitGL)
            {
                deinitOpenGL();
                ContextRequest = contextRequest_None;
            }
        }
    }

    EmuStatus = emuStatus_Exit;

    GPU::DeInitRenderer();
    NDS::DeInit();
    //Platform::LAN_DeInit();
}

void EmuThread::changeWindowTitle(char* title)
{
    emit windowTitleChange(QString(title));
}

void EmuThread::emuRun()
{
    EmuRunning = emuStatus_Running;
    EmuPauseStack = EmuPauseStackRunning;
    RunningSomething = true;

    // checkme
    emit windowEmuStart();
    AudioInOut::Enable();
}

void EmuThread::initContext()
{
    ContextRequest = contextRequest_InitGL;
    while (ContextRequest != contextRequest_None);
}

void EmuThread::deinitContext()
{
    ContextRequest = contextRequest_DeInitGL;
    while (ContextRequest != contextRequest_None);
}

void EmuThread::emuPause()
{
    EmuPauseStack++;
    if (EmuPauseStack > EmuPauseStackPauseThreshold) return;

    PrevEmuStatus = EmuRunning;
    EmuRunning = emuStatus_Paused;
    while (EmuStatus != emuStatus_Paused);

    AudioInOut::Disable();
}

void EmuThread::emuUnpause()
{
    if (EmuPauseStack < EmuPauseStackPauseThreshold) return;

    EmuPauseStack--;
    if (EmuPauseStack >= EmuPauseStackPauseThreshold) return;

    EmuRunning = PrevEmuStatus;

    AudioInOut::Enable();
}

void EmuThread::emuStop()
{
    EmuRunning = emuStatus_Exit;
    EmuPauseStack = EmuPauseStackRunning;

    AudioInOut::Disable();
}

void EmuThread::emuFrameStep()
{
    if (EmuPauseStack < EmuPauseStackPauseThreshold) emit windowEmuPause();
    EmuRunning = emuStatus_FrameStep;
}

bool EmuThread::emuIsRunning()
{
    return EmuRunning == emuStatus_Running;
}

bool EmuThread::emuIsActive()
{
    return (RunningSomething == 1);
}

void EmuThread::drawScreenGL()
{
    int w = windowInfo.surface_width;
    int h = windowInfo.surface_height;
    float factor = windowInfo.surface_scale;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(false);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, w, h);

    glUseProgram(screenShaderProgram[2]);
    glUniform2f(screenShaderScreenSizeULoc, w / factor, h / factor);

    int frontbuf = FrontBuffer;
    glActiveTexture(GL_TEXTURE0);

#ifdef OGLRENDERER_ENABLED
    if (GPU::Renderer != 0)
    {
        // hardware-accelerated render
        GPU::CurGLCompositor->BindOutputTexture(frontbuf);
    }
    else
#endif
    {
        // regular render
        glBindTexture(GL_TEXTURE_2D, screenTexture);

        if (GPU::Framebuffer[frontbuf][0] && GPU::Framebuffer[frontbuf][1])
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 192, GL_RGBA,
                            GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][0]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192+2, 256, 192, GL_RGBA,
                            GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][1]);
        }
    }

    screenSettingsLock.lock();

    GLint filter = this->filter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    glBindBuffer(GL_ARRAY_BUFFER, screenVertexBuffer);
    glBindVertexArray(screenVertexArray);

    for (int i = 0; i < numScreens; i++)
    {
        glUniformMatrix2x3fv(screenShaderTransformULoc, 1, GL_TRUE, screenMatrix[i]);
        glDrawArrays(GL_TRIANGLES, screenKind[i] == 0 ? 0 : 2*3, 2*3);
    }

    screenSettingsLock.unlock();

    OSD::Update();
    OSD::DrawGL(w, h);

    oglContext->SwapBuffers();
}

ScreenHandler::ScreenHandler(QWidget* widget)
{
    widget->setMouseTracking(true);
    widget->setAttribute(Qt::WA_AcceptTouchEvents);
    QTimer* mouseTimer = setupMouseTimer();
    widget->connect(mouseTimer, &QTimer::timeout, [=] { if (Config::MouseHide) widget->setCursor(Qt::BlankCursor);});
}

ScreenHandler::~ScreenHandler()
{
    mouseTimer->stop();
}

void ScreenHandler::screenSetupLayout(int w, int h)
{
    int sizing = Config::ScreenSizing;
    if (sizing == 3) sizing = autoScreenSizing;

    float aspectTop, aspectBot;

    for (auto ratio : aspectRatios)
    {
        if (ratio.id == Config::ScreenAspectTop)
            aspectTop = ratio.ratio;
        if (ratio.id == Config::ScreenAspectBot)
            aspectBot = ratio.ratio;
    }

    if (aspectTop == 0)
        aspectTop = ((float) w / h) / (4.f / 3.f);

    if (aspectBot == 0)
        aspectBot = ((float) w / h) / (4.f / 3.f);

    Frontend::SetupScreenLayout(w, h,
                                static_cast<Frontend::ScreenLayout>(Config::ScreenLayout),
                                static_cast<Frontend::ScreenRotation>(Config::ScreenRotation),
                                static_cast<Frontend::ScreenSizing>(sizing),
                                Config::ScreenGap,
                                Config::IntegerScaling != 0,
                                Config::ScreenSwap != 0,
                                aspectTop,
                                aspectBot);

    numScreens = Frontend::GetScreenTransforms(screenMatrix[0], screenKind);
}

QSize ScreenHandler::screenGetMinSize(int factor = 1)
{
    bool isHori = (Config::ScreenRotation == Frontend::screenRot_90Deg
        || Config::ScreenRotation == Frontend::screenRot_270Deg);
    int gap = Config::ScreenGap * factor;

    int w = 256 * factor;
    int h = 192 * factor;

    if (Config::ScreenSizing == Frontend::screenSizing_TopOnly
        || Config::ScreenSizing == Frontend::screenSizing_BotOnly)
    {
        return QSize(w, h);
    }

    if (Config::ScreenLayout == Frontend::screenLayout_Natural)
    {
        if (isHori)
            return QSize(h+gap+h, w);
        else
            return QSize(w, h+gap+h);
    }
    else if (Config::ScreenLayout == Frontend::screenLayout_Vertical)
    {
        if (isHori)
            return QSize(h, w+gap+w);
        else
            return QSize(w, h+gap+h);
    }
    else if (Config::ScreenLayout == Frontend::screenLayout_Horizontal)
    {
        if (isHori)
            return QSize(h+gap+h, w);
        else
            return QSize(w+gap+w, h);
    }
    else // hybrid
    {
        if (isHori)
            return QSize(h+gap+h, 3*w + (int)ceil((4*gap) / 3.0));
        else
            return QSize(3*w + (int)ceil((4*gap) / 3.0), h+gap+h);
    }
}

void ScreenHandler::screenOnMousePress(QMouseEvent* event)
{
    event->accept();
    if (event->button() != Qt::LeftButton) return;

    int x = event->pos().x();
    int y = event->pos().y();

    if (Frontend::GetTouchCoords(x, y, false))
    {
        touching = true;
        NDS::TouchScreen(x, y);
    }
}

void ScreenHandler::screenOnMouseRelease(QMouseEvent* event)
{
    event->accept();
    if (event->button() != Qt::LeftButton) return;

    if (touching)
    {
        touching = false;
        NDS::ReleaseScreen();
    }
}

void ScreenHandler::screenOnMouseMove(QMouseEvent* event)
{
    event->accept();

    showCursor();

    if (!(event->buttons() & Qt::LeftButton)) return;
    if (!touching) return;

    int x = event->pos().x();
    int y = event->pos().y();

    if (Frontend::GetTouchCoords(x, y, true))
        NDS::TouchScreen(x, y);
}

void ScreenHandler::screenHandleTablet(QTabletEvent* event)
{
    event->accept();

    switch(event->type())
    {
    case QEvent::TabletPress:
    case QEvent::TabletMove:
        {
            int x = event->x();
            int y = event->y();

            if (Frontend::GetTouchCoords(x, y, event->type()==QEvent::TabletMove))
            {
                touching = true;
                NDS::TouchScreen(x, y);
            }
        }
        break;
    case QEvent::TabletRelease:
        if (touching)
        {
            NDS::ReleaseScreen();
            touching = false;
        }
        break;
    default:
        break;
    }
}

void ScreenHandler::screenHandleTouch(QTouchEvent* event)
{
    event->accept();

    switch(event->type())
    {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
        if (event->touchPoints().length() > 0)
        {
            QPointF lastPosition = event->touchPoints().first().lastPos();
            int x = (int)lastPosition.x();
            int y = (int)lastPosition.y();

            if (Frontend::GetTouchCoords(x, y, event->type()==QEvent::TouchUpdate))
            {
                touching = true;
                NDS::TouchScreen(x, y);
            }
        }
        break;
    case QEvent::TouchEnd:
        if (touching)
        {
            NDS::ReleaseScreen();
            touching = false;
        }
        break;
    default:
        break;
    }
}

void ScreenHandler::showCursor()
{
    mainWindow->panelWidget->setCursor(Qt::ArrowCursor);
    mouseTimer->start();
}

QTimer* ScreenHandler::setupMouseTimer()
{
    mouseTimer = new QTimer();
    mouseTimer->setSingleShot(true);
    mouseTimer->setInterval(Config::MouseHideSeconds*1000);
    mouseTimer->start();

    return mouseTimer;
}

ScreenPanelNative::ScreenPanelNative(QWidget* parent) : QWidget(parent), ScreenHandler(this)
{
    screen[0] = QImage(256, 192, QImage::Format_RGB32);
    screen[1] = QImage(256, 192, QImage::Format_RGB32);

    screenTrans[0].reset();
    screenTrans[1].reset();

    OSD::Init(false);
}

ScreenPanelNative::~ScreenPanelNative()
{
    OSD::DeInit();
}

void ScreenPanelNative::setupScreenLayout()
{
    int w = width();
    int h = height();

    screenSetupLayout(w, h);

    for (int i = 0; i < numScreens; i++)
    {
        float* mtx = screenMatrix[i];
        screenTrans[i].setMatrix(mtx[0], mtx[1], 0.f,
                                mtx[2], mtx[3], 0.f,
                                mtx[4], mtx[5], 1.f);
    }
}

void ScreenPanelNative::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    // fill background
    painter.fillRect(event->rect(), QColor::fromRgb(0, 0, 0));

    if (emuThread->emuIsActive())
    {
        emuThread->FrontBufferLock.lock();
        int frontbuf = emuThread->FrontBuffer;
        if (!GPU::Framebuffer[frontbuf][0] || !GPU::Framebuffer[frontbuf][1])
        {
            emuThread->FrontBufferLock.unlock();
            return;
        }

        memcpy(screen[0].scanLine(0), GPU::Framebuffer[frontbuf][0], 256 * 192 * 4);
        memcpy(screen[1].scanLine(0), GPU::Framebuffer[frontbuf][1], 256 * 192 * 4);
        emuThread->FrontBufferLock.unlock();

        QRect screenrc(0, 0, 256, 192);

        for (int i = 0; i < numScreens; i++)
        {
            painter.setTransform(screenTrans[i]);
            painter.drawImage(screenrc, screen[screenKind[i]]);
        }
    }

    OSD::Update();
    OSD::DrawNative(painter);
}

void ScreenPanelNative::resizeEvent(QResizeEvent* event)
{
    setupScreenLayout();
}

void ScreenPanelNative::mousePressEvent(QMouseEvent* event)
{
    screenOnMousePress(event);
}

void ScreenPanelNative::mouseReleaseEvent(QMouseEvent* event)
{
    screenOnMouseRelease(event);
}

void ScreenPanelNative::mouseMoveEvent(QMouseEvent* event)
{
    screenOnMouseMove(event);
}

void ScreenPanelNative::tabletEvent(QTabletEvent* event)
{
    screenHandleTablet(event);
}

bool ScreenPanelNative::event(QEvent* event)
{
    if (event->type() == QEvent::TouchBegin
        || event->type() == QEvent::TouchEnd
        || event->type() == QEvent::TouchUpdate)
    {
        screenHandleTouch((QTouchEvent*)event);
        return true;
    }
    return QWidget::event(event);
}

void ScreenPanelNative::onScreenLayoutChanged()
{
    setMinimumSize(screenGetMinSize());
    setupScreenLayout();
}


ScreenPanelGL::ScreenPanelGL(QWidget* parent) : QWidget(parent), ScreenHandler(this)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_KeyCompression, false);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(screenGetMinSize());
}

ScreenPanelGL::~ScreenPanelGL()
{}

bool ScreenPanelGL::createContext()
{
    std::optional<WindowInfo> windowInfo = getWindowInfo();
    std::array<GL::Context::Version, 2> versionsToTry = {
        GL::Context::Version{GL::Context::Profile::Core, 4, 3},
        GL::Context::Version{GL::Context::Profile::Core, 3, 2}};
    if (windowInfo.has_value())
    {
        glContext = GL::Context::Create(*getWindowInfo(), versionsToTry);
        glContext->DoneCurrent();
    }

    return glContext != nullptr;
}

qreal ScreenPanelGL::devicePixelRatioFromScreen() const
{
    const QScreen* screen_for_ratio = window()->windowHandle()->screen();
    if (!screen_for_ratio)
        screen_for_ratio = QGuiApplication::primaryScreen();

    return screen_for_ratio ? screen_for_ratio->devicePixelRatio() : static_cast<qreal>(1);
}

int ScreenPanelGL::scaledWindowWidth() const
{
  return std::max(static_cast<int>(std::ceil(static_cast<qreal>(width()) * devicePixelRatioFromScreen())), 1);
}

int ScreenPanelGL::scaledWindowHeight() const
{
  return std::max(static_cast<int>(std::ceil(static_cast<qreal>(height()) * devicePixelRatioFromScreen())), 1);
}

std::optional<WindowInfo> ScreenPanelGL::getWindowInfo()
{
    WindowInfo wi;

    // Windows and Apple are easy here since there's no display connection.
    #if defined(_WIN32)
    wi.type = WindowInfo::Type::Win32;
    wi.window_handle = reinterpret_cast<void*>(winId());
    #elif defined(__APPLE__)
    wi.type = WindowInfo::Type::MacOS;
    wi.window_handle = reinterpret_cast<void*>(winId());
    #else
    QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
    const QString platform_name = QGuiApplication::platformName();
    if (platform_name == QStringLiteral("xcb"))
    {
        wi.type = WindowInfo::Type::X11;
        wi.display_connection = pni->nativeResourceForWindow("display", windowHandle());
        wi.window_handle = reinterpret_cast<void*>(winId());
    }
    else if (platform_name == QStringLiteral("wayland"))
    {
        wi.type = WindowInfo::Type::Wayland;
        QWindow* handle = windowHandle();
        if (handle == nullptr)
            return std::nullopt;

        wi.display_connection = pni->nativeResourceForWindow("display", handle);
        wi.window_handle = pni->nativeResourceForWindow("surface", handle);
    }
    else
    {
        qCritical() << "Unknown PNI platform " << platform_name;
        return std::nullopt;
    }
    #endif

    wi.surface_width = static_cast<u32>(scaledWindowWidth());
    wi.surface_height = static_cast<u32>(scaledWindowHeight());
    wi.surface_scale = static_cast<float>(devicePixelRatioFromScreen());

    return wi;
}


QPaintEngine* ScreenPanelGL::paintEngine() const
{
  return nullptr;
}

void ScreenPanelGL::setupScreenLayout()
{
    int w = width();
    int h = height();

    screenSetupLayout(w, h);
    if (emuThread)
        transferLayout(emuThread);
}

void ScreenPanelGL::resizeEvent(QResizeEvent* event)
{
    setupScreenLayout();

    QWidget::resizeEvent(event);
}

void ScreenPanelGL::mousePressEvent(QMouseEvent* event)
{
    screenOnMousePress(event);
}

void ScreenPanelGL::mouseReleaseEvent(QMouseEvent* event)
{
    screenOnMouseRelease(event);
}

void ScreenPanelGL::mouseMoveEvent(QMouseEvent* event)
{
    screenOnMouseMove(event);
}

void ScreenPanelGL::tabletEvent(QTabletEvent* event)
{
    screenHandleTablet(event);
}

bool ScreenPanelGL::event(QEvent* event)
{
    if (event->type() == QEvent::TouchBegin
        || event->type() == QEvent::TouchEnd
        || event->type() == QEvent::TouchUpdate)
    {
        screenHandleTouch((QTouchEvent*)event);
        return true;
    }
    return QWidget::event(event);
}

void ScreenPanelGL::transferLayout(EmuThread* thread)
{
    std::optional<WindowInfo> windowInfo = getWindowInfo();
    if (windowInfo.has_value())
        thread->updateScreenSettings(Config::ScreenFilter, *windowInfo, numScreens, screenKind, &screenMatrix[0][0]);
}

void ScreenPanelGL::onScreenLayoutChanged()
{
    setMinimumSize(screenGetMinSize());
    setupScreenLayout();
}


static bool FileExtensionInList(const QString& filename, const QStringList& extensions, Qt::CaseSensitivity cs = Qt::CaseInsensitive)
{
    return std::any_of(extensions.cbegin(), extensions.cend(), [&](const auto& ext) {
        return filename.endsWith(ext, cs);
    });
}

static bool MimeTypeInList(const QMimeType& mimetype, const QStringList& superTypeNames)
{
    return std::any_of(superTypeNames.cbegin(), superTypeNames.cend(), [&](const auto& superTypeName) {
        return mimetype.inherits(superTypeName);
    });
}


static bool NdsRomByExtension(const QString& filename)
{
    return FileExtensionInList(filename, NdsRomExtensions);
}

static bool GbaRomByExtension(const QString& filename)
{
    return FileExtensionInList(filename, GbaRomExtensions);
}

static bool SupportedArchiveByExtension(const QString& filename)
{
    return FileExtensionInList(filename, ArchiveExtensions);
}


static bool NdsRomByMimetype(const QMimeType& mimetype)
{
    return mimetype.inherits(NdsRomMimeType);
}

static bool GbaRomByMimetype(const QMimeType& mimetype)
{
    return mimetype.inherits(GbaRomMimeType);
}

static bool SupportedArchiveByMimetype(const QMimeType& mimetype)
{
    return MimeTypeInList(mimetype, ArchiveMimeTypes);
}

static bool ZstdNdsRomByExtension(const QString& filename)
{
    return filename.endsWith(".zst", Qt::CaseInsensitive) &&
        NdsRomByExtension(filename.left(filename.size() - 4));
}

static bool ZstdGbaRomByExtension(const QString& filename)
{
    return filename.endsWith(".zst", Qt::CaseInsensitive) &&
        GbaRomByExtension(filename.left(filename.size() - 4));
}

static bool FileIsSupportedFiletype(const QString& filename, bool insideArchive = false)
{
    if (ZstdNdsRomByExtension(filename) || ZstdGbaRomByExtension(filename))
        return true;

    if (NdsRomByExtension(filename) || GbaRomByExtension(filename) || SupportedArchiveByExtension(filename))
        return true;

    const auto matchmode = insideArchive ? QMimeDatabase::MatchExtension : QMimeDatabase::MatchDefault;
    const QMimeType mimetype = QMimeDatabase().mimeTypeForFile(filename, matchmode);
    return NdsRomByMimetype(mimetype) || GbaRomByMimetype(mimetype) || SupportedArchiveByMimetype(mimetype);
}


#ifndef _WIN32
static int signalFd[2];
QSocketNotifier *signalSn;

static void signalHandler(int)
{
    char a = 1;
    write(signalFd[0], &a, sizeof(a));
}
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
#ifndef _WIN32
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, signalFd))
    {
       qFatal("Couldn't create socketpair");
    }

    signalSn = new QSocketNotifier(signalFd[1], QSocketNotifier::Read, this);
    connect(signalSn, SIGNAL(activated(int)), this, SLOT(onQuit()));

    struct sigaction sa;

    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_flags |= SA_RESTART;
    sigaction(SIGINT, &sa, 0);
#endif

    oldW = Config::WindowWidth;
    oldH = Config::WindowHeight;
    oldMax = Config::WindowMaximized;

    setWindowTitle("melonDS " MELONDS_VERSION);
    setAttribute(Qt::WA_DeleteOnClose);
    setAcceptDrops(true);
    setFocusPolicy(Qt::ClickFocus);

    int inst = Platform::InstanceID();

    QMenuBar* menubar = new QMenuBar();
    {
        QMenu* menu = menubar->addMenu("File");

        actOpenROM = menu->addAction("Open ROM...");
        connect(actOpenROM, &QAction::triggered, this, &MainWindow::onOpenFile);
        actOpenROM->setShortcut(QKeySequence(QKeySequence::StandardKey::Open));

        /*actOpenROMArchive = menu->addAction("Open ROM inside archive...");
        connect(actOpenROMArchive, &QAction::triggered, this, &MainWindow::onOpenFileArchive);
        actOpenROMArchive->setShortcut(QKeySequence(Qt::Key_O | Qt::CTRL | Qt::SHIFT));*/

        recentMenu = menu->addMenu("Open recent");
        for (int i = 0; i < 10; ++i)
        {
            std::string item = Config::RecentROMList[i];
            if (!item.empty())
                recentFileList.push_back(QString::fromStdString(item));
        }
        updateRecentFilesMenu();

        //actBootFirmware = menu->addAction("Launch DS menu");
        actBootFirmware = menu->addAction("Boot firmware");
        connect(actBootFirmware, &QAction::triggered, this, &MainWindow::onBootFirmware);

        menu->addSeparator();

        actCurrentCart = menu->addAction("DS slot: " + ROMManager::CartLabel());
        actCurrentCart->setEnabled(false);

        actInsertCart = menu->addAction("Insert cart...");
        connect(actInsertCart, &QAction::triggered, this, &MainWindow::onInsertCart);

        actEjectCart = menu->addAction("Eject cart");
        connect(actEjectCart, &QAction::triggered, this, &MainWindow::onEjectCart);

        menu->addSeparator();

        actCurrentGBACart = menu->addAction("GBA slot: " + ROMManager::GBACartLabel());
        actCurrentGBACart->setEnabled(false);

        actInsertGBACart = menu->addAction("Insert ROM cart...");
        connect(actInsertGBACart, &QAction::triggered, this, &MainWindow::onInsertGBACart);

        {
            QMenu* submenu = menu->addMenu("Insert add-on cart");

            actInsertGBAAddon[0] = submenu->addAction("Memory expansion");
            actInsertGBAAddon[0]->setData(QVariant(NDS::GBAAddon_RAMExpansion));
            connect(actInsertGBAAddon[0], &QAction::triggered, this, &MainWindow::onInsertGBAAddon);
        }

        actEjectGBACart = menu->addAction("Eject cart");
        connect(actEjectGBACart, &QAction::triggered, this, &MainWindow::onEjectGBACart);

        menu->addSeparator();

        actImportSavefile = menu->addAction("Import savefile");
        connect(actImportSavefile, &QAction::triggered, this, &MainWindow::onImportSavefile);

        menu->addSeparator();

        {
            QMenu* submenu = menu->addMenu("Save state");

            for (int i = 1; i < 9; i++)
            {
                actSaveState[i] = submenu->addAction(QString("%1").arg(i));
                actSaveState[i]->setShortcut(QKeySequence(Qt::ShiftModifier | (Qt::Key_F1+i-1)));
                actSaveState[i]->setData(QVariant(i));
                connect(actSaveState[i], &QAction::triggered, this, &MainWindow::onSaveState);
            }

            actSaveState[0] = submenu->addAction("File...");
            actSaveState[0]->setShortcut(QKeySequence(Qt::ShiftModifier | Qt::Key_F9));
            actSaveState[0]->setData(QVariant(0));
            connect(actSaveState[0], &QAction::triggered, this, &MainWindow::onSaveState);
        }
        {
            QMenu* submenu = menu->addMenu("Load state");

            for (int i = 1; i < 9; i++)
            {
                actLoadState[i] = submenu->addAction(QString("%1").arg(i));
                actLoadState[i]->setShortcut(QKeySequence(Qt::Key_F1+i-1));
                actLoadState[i]->setData(QVariant(i));
                connect(actLoadState[i], &QAction::triggered, this, &MainWindow::onLoadState);
            }

            actLoadState[0] = submenu->addAction("File...");
            actLoadState[0]->setShortcut(QKeySequence(Qt::Key_F9));
            actLoadState[0]->setData(QVariant(0));
            connect(actLoadState[0], &QAction::triggered, this, &MainWindow::onLoadState);
        }

        actUndoStateLoad = menu->addAction("Undo state load");
        actUndoStateLoad->setShortcut(QKeySequence(Qt::Key_F12));
        connect(actUndoStateLoad, &QAction::triggered, this, &MainWindow::onUndoStateLoad);

        menu->addSeparator();

        actQuit = menu->addAction("Quit");
        connect(actQuit, &QAction::triggered, this, &MainWindow::onQuit);
        actQuit->setShortcut(QKeySequence(QKeySequence::StandardKey::Quit));
    }
    {
        QMenu* menu = menubar->addMenu("System");

        actPause = menu->addAction("Pause");
        actPause->setCheckable(true);
        connect(actPause, &QAction::triggered, this, &MainWindow::onPause);

        actReset = menu->addAction("Reset");
        connect(actReset, &QAction::triggered, this, &MainWindow::onReset);

        actStop = menu->addAction("Stop");
        connect(actStop, &QAction::triggered, this, &MainWindow::onStop);

        actFrameStep = menu->addAction("Frame step");
        connect(actFrameStep, &QAction::triggered, this, &MainWindow::onFrameStep);

        menu->addSeparator();

        actPowerManagement = menu->addAction("Power management");
        connect(actPowerManagement, &QAction::triggered, this, &MainWindow::onOpenPowerManagement);

        menu->addSeparator();

        actEnableCheats = menu->addAction("Enable cheats");
        actEnableCheats->setCheckable(true);
        connect(actEnableCheats, &QAction::triggered, this, &MainWindow::onEnableCheats);

        //if (inst == 0)
        {
            actSetupCheats = menu->addAction("Setup cheat codes");
            actSetupCheats->setMenuRole(QAction::NoRole);
            connect(actSetupCheats, &QAction::triggered, this, &MainWindow::onSetupCheats);

            menu->addSeparator();
            actROMInfo = menu->addAction("ROM info");
            connect(actROMInfo, &QAction::triggered, this, &MainWindow::onROMInfo);

            actRAMInfo = menu->addAction("RAM search");
            connect(actRAMInfo, &QAction::triggered, this, &MainWindow::onRAMInfo);

            actTitleManager = menu->addAction("Manage DSi titles");
            connect(actTitleManager, &QAction::triggered, this, &MainWindow::onOpenTitleManager);
        }

        {
            menu->addSeparator();
            QMenu* submenu = menu->addMenu("Multiplayer");

            actMPNewInstance = submenu->addAction("Launch new instance");
            connect(actMPNewInstance, &QAction::triggered, this, &MainWindow::onMPNewInstance);
        }
    }
    {
        QMenu* menu = menubar->addMenu("Config");

        actEmuSettings = menu->addAction("Emu settings");
        connect(actEmuSettings, &QAction::triggered, this, &MainWindow::onOpenEmuSettings);

#ifdef __APPLE__
        actPreferences = menu->addAction("Preferences...");
        connect(actPreferences, &QAction::triggered, this, &MainWindow::onOpenEmuSettings);
        actPreferences->setMenuRole(QAction::PreferencesRole);
#endif

        actInputConfig = menu->addAction("Input and hotkeys");
        connect(actInputConfig, &QAction::triggered, this, &MainWindow::onOpenInputConfig);

        actVideoSettings = menu->addAction("Video settings");
        connect(actVideoSettings, &QAction::triggered, this, &MainWindow::onOpenVideoSettings);

        actCameraSettings = menu->addAction("Camera settings");
        connect(actCameraSettings, &QAction::triggered, this, &MainWindow::onOpenCameraSettings);

        actAudioSettings = menu->addAction("Audio settings");
        connect(actAudioSettings, &QAction::triggered, this, &MainWindow::onOpenAudioSettings);

        actMPSettings = menu->addAction("Multiplayer settings");
        connect(actMPSettings, &QAction::triggered, this, &MainWindow::onOpenMPSettings);

        actWifiSettings = menu->addAction("Wifi settings");
        connect(actWifiSettings, &QAction::triggered, this, &MainWindow::onOpenWifiSettings);

        actFirmwareSettings = menu->addAction("Firmware settings");
        connect(actFirmwareSettings, &QAction::triggered, this, &MainWindow::onOpenFirmwareSettings);

        actInterfaceSettings = menu->addAction("Interface settings");
        connect(actInterfaceSettings, &QAction::triggered, this, &MainWindow::onOpenInterfaceSettings);

        actPathSettings = menu->addAction("Path settings");
        connect(actPathSettings, &QAction::triggered, this, &MainWindow::onOpenPathSettings);

        {
            QMenu* submenu = menu->addMenu("Savestate settings");

            actSavestateSRAMReloc = submenu->addAction("Separate savefiles");
            actSavestateSRAMReloc->setCheckable(true);
            connect(actSavestateSRAMReloc, &QAction::triggered, this, &MainWindow::onChangeSavestateSRAMReloc);
        }

        menu->addSeparator();

        {
            QMenu* submenu = menu->addMenu("Screen size");

            for (int i = 0; i < 4; i++)
            {
                int data = i+1;
                actScreenSize[i] = submenu->addAction(QString("%1x").arg(data));
                actScreenSize[i]->setData(QVariant(data));
                connect(actScreenSize[i], &QAction::triggered, this, &MainWindow::onChangeScreenSize);
            }
        }
        {
            QMenu* submenu = menu->addMenu("Screen rotation");
            grpScreenRotation = new QActionGroup(submenu);

            for (int i = 0; i < Frontend::screenRot_MAX; i++)
            {
                int data = i*90;
                actScreenRotation[i] = submenu->addAction(QString("%1°").arg(data));
                actScreenRotation[i]->setActionGroup(grpScreenRotation);
                actScreenRotation[i]->setData(QVariant(i));
                actScreenRotation[i]->setCheckable(true);
            }

            connect(grpScreenRotation, &QActionGroup::triggered, this, &MainWindow::onChangeScreenRotation);
        }
        {
            QMenu* submenu = menu->addMenu("Screen gap");
            grpScreenGap = new QActionGroup(submenu);

            const int screengap[] = {0, 1, 8, 64, 90, 128};

            for (int i = 0; i < 6; i++)
            {
                int data = screengap[i];
                actScreenGap[i] = submenu->addAction(QString("%1 px").arg(data));
                actScreenGap[i]->setActionGroup(grpScreenGap);
                actScreenGap[i]->setData(QVariant(data));
                actScreenGap[i]->setCheckable(true);
            }

            connect(grpScreenGap, &QActionGroup::triggered, this, &MainWindow::onChangeScreenGap);
        }
        {
            QMenu* submenu = menu->addMenu("Screen layout");
            grpScreenLayout = new QActionGroup(submenu);

            const char* screenlayout[] = {"Natural", "Vertical", "Horizontal", "Hybrid"};

            for (int i = 0; i < Frontend::screenLayout_MAX; i++)
            {
                actScreenLayout[i] = submenu->addAction(QString(screenlayout[i]));
                actScreenLayout[i]->setActionGroup(grpScreenLayout);
                actScreenLayout[i]->setData(QVariant(i));
                actScreenLayout[i]->setCheckable(true);
            }

            connect(grpScreenLayout, &QActionGroup::triggered, this, &MainWindow::onChangeScreenLayout);

            submenu->addSeparator();

            actScreenSwap = submenu->addAction("Swap screens");
            actScreenSwap->setCheckable(true);
            connect(actScreenSwap, &QAction::triggered, this, &MainWindow::onChangeScreenSwap);
        }
        {
            QMenu* submenu = menu->addMenu("Screen sizing");
            grpScreenSizing = new QActionGroup(submenu);

            const char* screensizing[] = {"Even", "Emphasize top", "Emphasize bottom", "Auto", "Top only", "Bottom only"};

            for (int i = 0; i < Frontend::screenSizing_MAX; i++)
            {
                actScreenSizing[i] = submenu->addAction(QString(screensizing[i]));
                actScreenSizing[i]->setActionGroup(grpScreenSizing);
                actScreenSizing[i]->setData(QVariant(i));
                actScreenSizing[i]->setCheckable(true);
            }

            connect(grpScreenSizing, &QActionGroup::triggered, this, &MainWindow::onChangeScreenSizing);

            submenu->addSeparator();

            actIntegerScaling = submenu->addAction("Force integer scaling");
            actIntegerScaling->setCheckable(true);
            connect(actIntegerScaling, &QAction::triggered, this, &MainWindow::onChangeIntegerScaling);
        }
        {
            QMenu* submenu = menu->addMenu("Aspect ratio");
            grpScreenAspectTop = new QActionGroup(submenu);
            grpScreenAspectBot = new QActionGroup(submenu);
            actScreenAspectTop = new QAction*[AspectRatiosNum];
            actScreenAspectBot = new QAction*[AspectRatiosNum];

            for (int i = 0; i < 2; i++)
            {
                QActionGroup* group = grpScreenAspectTop;
                QAction** actions = actScreenAspectTop;

                if (i == 1)
                {
                    group = grpScreenAspectBot;
                    submenu->addSeparator();
                    actions = actScreenAspectBot;
                }

                for (int j = 0; j < AspectRatiosNum; j++)
                {
                    auto ratio = aspectRatios[j];
                    QString label = QString("%1 %2").arg(i ? "Bottom" : "Top", ratio.label);
                    actions[j] = submenu->addAction(label);
                    actions[j]->setActionGroup(group);
                    actions[j]->setData(QVariant(ratio.id));
                    actions[j]->setCheckable(true);
                }

                connect(group, &QActionGroup::triggered, this, &MainWindow::onChangeScreenAspect);
            }
        }

        actScreenFiltering = menu->addAction("Screen filtering");
        actScreenFiltering->setCheckable(true);
        connect(actScreenFiltering, &QAction::triggered, this, &MainWindow::onChangeScreenFiltering);

        actShowOSD = menu->addAction("Show OSD");
        actShowOSD->setCheckable(true);
        connect(actShowOSD, &QAction::triggered, this, &MainWindow::onChangeShowOSD);

        menu->addSeparator();

        actLimitFramerate = menu->addAction("Limit framerate");
        actLimitFramerate->setCheckable(true);
        connect(actLimitFramerate, &QAction::triggered, this, &MainWindow::onChangeLimitFramerate);

        actAudioSync = menu->addAction("Audio sync");
        actAudioSync->setCheckable(true);
        connect(actAudioSync, &QAction::triggered, this, &MainWindow::onChangeAudioSync);
    }
    setMenuBar(menubar);

    resize(Config::WindowWidth, Config::WindowHeight);

    if (Config::FirmwareUsername == "Arisotura")
        actMPNewInstance->setText("Fart");

#ifdef Q_OS_MAC
    QPoint screenCenter = screen()->availableGeometry().center();
    QRect frameGeo = frameGeometry();
    frameGeo.moveCenter(screenCenter);
    move(frameGeo.topLeft());
#endif

    if (oldMax)
        showMaximized();
    else
        show();

    createScreenPanel();

    actEjectCart->setEnabled(false);
    actEjectGBACart->setEnabled(false);

    if (Config::ConsoleType == 1)
    {
        actInsertGBACart->setEnabled(false);
        for (int i = 0; i < 1; i++)
            actInsertGBAAddon[i]->setEnabled(false);
    }

    for (int i = 0; i < 9; i++)
    {
        actSaveState[i]->setEnabled(false);
        actLoadState[i]->setEnabled(false);
    }
    actUndoStateLoad->setEnabled(false);
    actImportSavefile->setEnabled(false);

    actPause->setEnabled(false);
    actReset->setEnabled(false);
    actStop->setEnabled(false);
    actFrameStep->setEnabled(false);

    actPowerManagement->setEnabled(false);

    actSetupCheats->setEnabled(false);
    actTitleManager->setEnabled(!Config::DSiNANDPath.empty());

    actEnableCheats->setChecked(Config::EnableCheats);

    actROMInfo->setEnabled(false);
    actRAMInfo->setEnabled(false);

    actSavestateSRAMReloc->setChecked(Config::SavestateRelocSRAM);

    actScreenRotation[Config::ScreenRotation]->setChecked(true);

    for (int i = 0; i < 6; i++)
    {
        if (actScreenGap[i]->data().toInt() == Config::ScreenGap)
        {
            actScreenGap[i]->setChecked(true);
            break;
        }
    }

    actScreenLayout[Config::ScreenLayout]->setChecked(true);
    actScreenSizing[Config::ScreenSizing]->setChecked(true);
    actIntegerScaling->setChecked(Config::IntegerScaling);

    actScreenSwap->setChecked(Config::ScreenSwap);

    for (int i = 0; i < AspectRatiosNum; i++)
    {
        if (Config::ScreenAspectTop == aspectRatios[i].id)
            actScreenAspectTop[i]->setChecked(true);
        if (Config::ScreenAspectBot == aspectRatios[i].id)
            actScreenAspectBot[i]->setChecked(true);
    }

    actScreenFiltering->setChecked(Config::ScreenFilter);
    actShowOSD->setChecked(Config::ShowOSD);

    actLimitFramerate->setChecked(Config::LimitFPS);
    actAudioSync->setChecked(Config::AudioSync);

    if (inst > 0)
    {
        actEmuSettings->setEnabled(false);
        actVideoSettings->setEnabled(false);
        actMPSettings->setEnabled(false);
        actWifiSettings->setEnabled(false);
        actInterfaceSettings->setEnabled(false);

#ifdef __APPLE__
        actPreferences->setEnabled(false);
#endif // __APPLE__
    }
}

MainWindow::~MainWindow()
{
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (hasOGL)
    {
        // we intentionally don't unpause here
        emuThread->emuPause();
        emuThread->deinitContext();
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::createScreenPanel()
{
    hasOGL = (Config::ScreenUseGL != 0) || (Config::_3DRenderer != 0);

    if (hasOGL)
    {
        ScreenPanelGL* panelGL = new ScreenPanelGL(this);
        panelGL->show();

        panel = panelGL;
        panelWidget = panelGL;

        panelGL->createContext();
    }

    if (!hasOGL)
    {
        ScreenPanelNative* panelNative = new ScreenPanelNative(this);
        panel = panelNative;
        panelWidget = panelNative;
        panelWidget->show();
    }
    setCentralWidget(panelWidget);

    actScreenFiltering->setEnabled(hasOGL);

    connect(this, SIGNAL(screenLayoutChange()), panelWidget, SLOT(onScreenLayoutChanged()));
    emit screenLayoutChange();
}

GL::Context* MainWindow::getOGLContext()
{
    if (!hasOGL) return nullptr;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->getContext();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    int w = event->size().width();
    int h = event->size().height();

    if (!isFullScreen())
    {
        // this is ugly
        // thing is, when maximizing the window, we first receive the resizeEvent
        // with a new size matching the screen, then the changeEvent telling us that
        // the maximized flag was updated
        oldW = Config::WindowWidth;
        oldH = Config::WindowHeight;
        oldMax = isMaximized();

        Config::WindowWidth = w;
        Config::WindowHeight = h;
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (isMaximized() && !oldMax)
    {
        Config::WindowWidth = oldW;
        Config::WindowHeight = oldH;
    }

    Config::WindowMaximized = isMaximized() ? 1:0;
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    // TODO!! REMOVE ME IN RELEASE BUILDS!!
    //if (event->key() == Qt::Key_F11) NDS::debug(0);

    Input::KeyPress(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    Input::KeyRelease(event);
}


void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event->mimeData()->hasUrls()) return;

    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.count() > 1) return; // not handling more than one file at once

    QString filename = urls.at(0).toLocalFile();

    if (FileIsSupportedFiletype(filename))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasUrls()) return;

    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.count() > 1) return; // not handling more than one file at once

    emuThread->emuPause();

    if (!verifySetup())
    {
        emuThread->emuUnpause();
        return;
    }

    const QStringList file = splitArchivePath(urls.at(0).toLocalFile(), false);
    if (file.isEmpty())
    {
        emuThread->emuUnpause();
        return;
    }

    const QString filename = file.last();
    const bool romInsideArchive = file.size() > 1;
    const auto matchMode = romInsideArchive ? QMimeDatabase::MatchExtension : QMimeDatabase::MatchDefault;
    const QMimeType mimetype = QMimeDatabase().mimeTypeForFile(filename, matchMode);

    bool isNdsRom = NdsRomByExtension(filename) || NdsRomByMimetype(mimetype);
    bool isGbaRom = GbaRomByExtension(filename) || GbaRomByMimetype(mimetype);
    isNdsRom |= ZstdNdsRomByExtension(filename);
    isGbaRom |= ZstdGbaRomByExtension(filename);

    if (isNdsRom)
    {
        if (!ROMManager::LoadROM(file, true))
        {
            // TODO: better error reporting?
            QMessageBox::critical(this, "melonDS", "Failed to load the DS ROM.");
            emuThread->emuUnpause();
            return;
        }

        const QString barredFilename = file.join('|');
        recentFileList.removeAll(barredFilename);
        recentFileList.prepend(barredFilename);
        updateRecentFilesMenu();

        NDS::Start();
        emuThread->emuRun();

        updateCartInserted(false);
    }
    else if (isGbaRom)
    {
        if (!ROMManager::LoadGBAROM(file))
        {
            // TODO: better error reporting?
            QMessageBox::critical(this, "melonDS", "Failed to load the GBA ROM.");
            emuThread->emuUnpause();
            return;
        }

        emuThread->emuUnpause();

        updateCartInserted(true);
    }
    else
    {
        QMessageBox::critical(this, "melonDS", "The file could not be recognized as a DS or GBA ROM.");
        emuThread->emuUnpause();
        return;
    }
}

void MainWindow::focusInEvent(QFocusEvent* event)
{
    AudioInOut::AudioMute(mainWindow);
}

void MainWindow::focusOutEvent(QFocusEvent* event)
{
    AudioInOut::AudioMute(mainWindow);
}

void MainWindow::onAppStateChanged(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationInactive)
    {
        if (Config::PauseLostFocus && emuThread->emuIsRunning())
            emuThread->emuPause();
    }
    else if (state == Qt::ApplicationActive)
    {
        if (Config::PauseLostFocus && !pausedManually)
            emuThread->emuUnpause();
    }
}

bool MainWindow::verifySetup()
{
    QString res = ROMManager::VerifySetup();
    if (!res.isEmpty())
    {
         QMessageBox::critical(this, "melonDS", res);
         return false;
    }

    return true;
}

bool MainWindow::preloadROMs(QStringList file, QStringList gbafile, bool boot)
{
    if (!verifySetup())
    {
        return false;
    }

    bool gbaloaded = false;
    if (!gbafile.isEmpty())
    {
        if (!ROMManager::LoadGBAROM(gbafile))
        {
            // TODO: better error reporting?
            QMessageBox::critical(this, "melonDS", "Failed to load the GBA ROM.");
            return false;
        }

        gbaloaded = true;
    }

    bool ndsloaded = false;
    if (!file.isEmpty())
    {
        if (!ROMManager::LoadROM(file, true))
        {
            // TODO: better error reporting?
            QMessageBox::critical(this, "melonDS", "Failed to load the ROM.");
            return false;
        }
        recentFileList.removeAll(file.join("|"));
        recentFileList.prepend(file.join("|"));
        updateRecentFilesMenu();
        ndsloaded = true;
    }

    if (boot)
    {
        if (ndsloaded)
        {
            NDS::Start();
            emuThread->emuRun();
        }
        else
        {
            onBootFirmware();
        }
    }

    updateCartInserted(false);

    if (gbaloaded)
    {
        updateCartInserted(true);
    }

    return true;
}

QStringList MainWindow::splitArchivePath(const QString& filename, bool useMemberSyntax)
{
    if (filename.isEmpty()) return {};

#ifdef ARCHIVE_SUPPORT_ENABLED
    if (useMemberSyntax)
    {
        const QStringList filenameParts = filename.split('|');
        if (filenameParts.size() > 2)
        {
            QMessageBox::warning(this, "melonDS", "This path contains too many '|'.");
            return {};
        }

        if (filenameParts.size() == 2)
        {
            const QString archive = filenameParts.at(0);
            if (!QFileInfo(archive).exists())
            {
                QMessageBox::warning(this, "melonDS", "This archive does not exist.");
                return {};
            }

            const QString subfile = filenameParts.at(1);
            if (!Archive::ListArchive(archive).contains(subfile))
            {
                QMessageBox::warning(this, "melonDS", "This archive does not contain the desired file.");
                return {};
            }

            return filenameParts;
        }
    }
#endif

    if (!QFileInfo(filename).exists())
    {
        QMessageBox::warning(this, "melonDS", "This ROM file does not exist.");
        return {};
    }

#ifdef ARCHIVE_SUPPORT_ENABLED
    if (SupportedArchiveByExtension(filename)
        || SupportedArchiveByMimetype(QMimeDatabase().mimeTypeForFile(filename)))
    {
        const QString subfile = pickFileFromArchive(filename);
        if (subfile.isEmpty())
            return {};

        return { filename, subfile };
    }
#endif

    return { filename };
}

QString MainWindow::pickFileFromArchive(QString archiveFileName)
{
    QVector<QString> archiveROMList = Archive::ListArchive(archiveFileName);

    if (archiveROMList.size() <= 1)
    {
        if (!archiveROMList.isEmpty() && archiveROMList.at(0) == "OK")
            QMessageBox::warning(this, "melonDS", "This archive is empty.");
        else
            QMessageBox::critical(this, "melonDS", "This archive could not be read. It may be corrupt or you don't have the permissions.");
        return QString();
    }

    archiveROMList.removeFirst();

    const auto notSupportedRom = [&](const auto& filename){
        if (NdsRomByExtension(filename) || GbaRomByExtension(filename))
            return false;
        const QMimeType mimetype = QMimeDatabase().mimeTypeForFile(filename, QMimeDatabase::MatchExtension);
        return !(NdsRomByMimetype(mimetype) || GbaRomByMimetype(mimetype));
    };

    archiveROMList.erase(std::remove_if(archiveROMList.begin(), archiveROMList.end(), notSupportedRom),
                         archiveROMList.end());

    if (archiveROMList.isEmpty())
    {
        QMessageBox::warning(this, "melonDS", "This archive does not contain any supported ROMs.");
        return QString();
    }

    if (archiveROMList.size() == 1)
        return archiveROMList.first();

    bool ok;
    const QString toLoad = QInputDialog::getItem(
        this, "melonDS",
        "This archive contains multiple files. Select which ROM you want to load.",
        archiveROMList.toList(), 0, false, &ok
    );

    if (ok) return toLoad;

    // User clicked on cancel

    return QString();
}

QStringList MainWindow::pickROM(bool gba)
{
    const QString console = gba ? "GBA" : "DS";
    const QStringList& romexts = gba ? GbaRomExtensions : NdsRomExtensions;

    QString rawROMs = romexts.join(" *");
    QString extraFilters = ";;" + console + " ROMs (*" + rawROMs;
    QString allROMs = rawROMs;

    QString zstdROMs = "*" + romexts.join(".zst *") + ".zst";
    extraFilters += ");;Zstandard-compressed " + console + " ROMs (" + zstdROMs + ")";
    allROMs += " " + zstdROMs;

#ifdef ARCHIVE_SUPPORT_ENABLED
    QString archives = "*" + ArchiveExtensions.join(" *");
    extraFilters += ";;Archives (" + archives + ")";
    allROMs += " " + archives;
#endif
    extraFilters += ";;All files (*.*)";

    const QString filename = QFileDialog::getOpenFileName(
        this, "Open " + console + " ROM",
        QString::fromStdString(Config::LastROMFolder),
        "All supported files (*" + allROMs + ")" + extraFilters
    );

    if (filename.isEmpty()) return {};

    Config::LastROMFolder = QFileInfo(filename).dir().path().toStdString();
    return splitArchivePath(filename, false);
}

void MainWindow::updateCartInserted(bool gba)
{
    bool inserted;
    if (gba)
    {
        inserted = ROMManager::GBACartInserted() && (Config::ConsoleType == 0);
        actCurrentGBACart->setText("GBA slot: " + ROMManager::GBACartLabel());
        actEjectGBACart->setEnabled(inserted);
    }
    else
    {
        inserted = ROMManager::CartInserted();
        actCurrentCart->setText("DS slot: " + ROMManager::CartLabel());
        actEjectCart->setEnabled(inserted);
        actImportSavefile->setEnabled(inserted);
        actSetupCheats->setEnabled(inserted);
        actROMInfo->setEnabled(inserted);
        actRAMInfo->setEnabled(inserted);
    }
}

void MainWindow::onOpenFile()
{
    emuThread->emuPause();

    if (!verifySetup())
    {
        emuThread->emuUnpause();
        return;
    }

    QStringList file = pickROM(false);
    if (file.isEmpty())
    {
        emuThread->emuUnpause();
        return;
    }

    if (!ROMManager::LoadROM(file, true))
    {
        // TODO: better error reporting?
        QMessageBox::critical(this, "melonDS", "Failed to load the ROM.");
        emuThread->emuUnpause();
        return;
    }

    QString filename = file.join('|');
    recentFileList.removeAll(filename);
    recentFileList.prepend(filename);
    updateRecentFilesMenu();

    NDS::Start();
    emuThread->emuRun();

    updateCartInserted(false);
}

void MainWindow::onClearRecentFiles()
{
    recentFileList.clear();
    for (int i = 0; i < 10; i++)
        Config::RecentROMList[i] = "";
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    recentMenu->clear();

    for (int i = 0; i < recentFileList.size(); ++i)
    {
        if (i >= 10) break;

        QString item_full = recentFileList.at(i);
        QString item_display = item_full;
        int itemlen = item_full.length();
        const int maxlen = 100;
        if (itemlen > maxlen)
        {
            int cut_start = 0;
            while (item_full[cut_start] != '/' && item_full[cut_start] != '\\' &&
                   cut_start < itemlen)
                cut_start++;

            int cut_end = itemlen-1;
            while (((item_full[cut_end] != '/' && item_full[cut_end] != '\\') ||
                    (cut_start+4+(itemlen-cut_end) < maxlen)) &&
                   cut_end > 0)
                cut_end--;

            item_display.truncate(cut_start+1);
            item_display += "...";
            item_display += QString(item_full).remove(0, cut_end);
        }

        QAction *actRecentFile_i = recentMenu->addAction(QString("%1.  %2").arg(i+1).arg(item_display));
        actRecentFile_i->setData(item_full);
        connect(actRecentFile_i, &QAction::triggered, this, &MainWindow::onClickRecentFile);

        Config::RecentROMList[i] = recentFileList.at(i).toStdString();
    }

    while (recentFileList.size() > 10)
        recentFileList.removeLast();

    recentMenu->addSeparator();

    QAction *actClearRecentList = recentMenu->addAction("Clear");
    connect(actClearRecentList, &QAction::triggered, this, &MainWindow::onClearRecentFiles);

    if (recentFileList.empty())
        actClearRecentList->setEnabled(false);

    Config::Save();
}

void MainWindow::onClickRecentFile()
{
    QAction *act = (QAction *)sender();
    QString filename = act->data().toString();

    emuThread->emuPause();

    if (!verifySetup())
    {
        emuThread->emuUnpause();
        return;
    }

    const QStringList file = splitArchivePath(filename, true);
    if (file.isEmpty())
    {
        emuThread->emuUnpause();
        return;
    }

    if (!ROMManager::LoadROM(file, true))
    {
        // TODO: better error reporting?
        QMessageBox::critical(this, "melonDS", "Failed to load the ROM.");
        emuThread->emuUnpause();
        return;
    }

    recentFileList.removeAll(filename);
    recentFileList.prepend(filename);
    updateRecentFilesMenu();

    NDS::Start();
    emuThread->emuRun();

    updateCartInserted(false);
}

void MainWindow::onBootFirmware()
{
    emuThread->emuPause();

    if (!verifySetup())
    {
        emuThread->emuUnpause();
        return;
    }

    if (!ROMManager::LoadBIOS())
    {
        // TODO: better error reporting?
        QMessageBox::critical(this, "melonDS", "This firmware is not bootable.");
        emuThread->emuUnpause();
        return;
    }

    NDS::Start();
    emuThread->emuRun();
}

void MainWindow::onInsertCart()
{
    emuThread->emuPause();

    QStringList file = pickROM(false);
    if (file.isEmpty())
    {
        emuThread->emuUnpause();
        return;
    }

    if (!ROMManager::LoadROM(file, false))
    {
        // TODO: better error reporting?
        QMessageBox::critical(this, "melonDS", "Failed to load the ROM.");
        emuThread->emuUnpause();
        return;
    }

    emuThread->emuUnpause();

    updateCartInserted(false);
}

void MainWindow::onEjectCart()
{
    emuThread->emuPause();

    ROMManager::EjectCart();

    emuThread->emuUnpause();

    updateCartInserted(false);
}

void MainWindow::onInsertGBACart()
{
    emuThread->emuPause();

    QStringList file = pickROM(true);
    if (file.isEmpty())
    {
        emuThread->emuUnpause();
        return;
    }

    if (!ROMManager::LoadGBAROM(file))
    {
        // TODO: better error reporting?
        QMessageBox::critical(this, "melonDS", "Failed to load the ROM.");
        emuThread->emuUnpause();
        return;
    }

    emuThread->emuUnpause();

    updateCartInserted(true);
}

void MainWindow::onInsertGBAAddon()
{
    QAction* act = (QAction*)sender();
    int type = act->data().toInt();

    emuThread->emuPause();

    ROMManager::LoadGBAAddon(type);

    emuThread->emuUnpause();

    updateCartInserted(true);
}

void MainWindow::onEjectGBACart()
{
    emuThread->emuPause();

    ROMManager::EjectGBACart();

    emuThread->emuUnpause();

    updateCartInserted(true);
}

void MainWindow::onSaveState()
{
    int slot = ((QAction*)sender())->data().toInt();

    emuThread->emuPause();

    std::string filename;
    if (slot > 0)
    {
        filename = ROMManager::GetSavestateName(slot);
    }
    else
    {
        // TODO: specific 'last directory' for savestate files?
        QString qfilename = QFileDialog::getSaveFileName(this,
                                                         "Save state",
                                                         QString::fromStdString(Config::LastROMFolder),
                                                         "melonDS savestates (*.mln);;Any file (*.*)");
        if (qfilename.isEmpty())
        {
            emuThread->emuUnpause();
            return;
        }

        filename = qfilename.toStdString();
    }

    if (ROMManager::SaveState(filename))
    {
        char msg[64];
        if (slot > 0) sprintf(msg, "State saved to slot %d", slot);
        else          sprintf(msg, "State saved to file");
        OSD::AddMessage(0, msg);

        actLoadState[slot]->setEnabled(true);
    }
    else
    {
        OSD::AddMessage(0xFFA0A0, "State save failed");
    }

    emuThread->emuUnpause();
}

void MainWindow::onLoadState()
{
    int slot = ((QAction*)sender())->data().toInt();

    emuThread->emuPause();

    std::string filename;
    if (slot > 0)
    {
        filename = ROMManager::GetSavestateName(slot);
    }
    else
    {
        // TODO: specific 'last directory' for savestate files?
        QString qfilename = QFileDialog::getOpenFileName(this,
                                                         "Load state",
                                                         QString::fromStdString(Config::LastROMFolder),
                                                         "melonDS savestates (*.ml*);;Any file (*.*)");
        if (qfilename.isEmpty())
        {
            emuThread->emuUnpause();
            return;
        }

        filename = qfilename.toStdString();
    }

    if (!Platform::FileExists(filename))
    {
        char msg[64];
        if (slot > 0) sprintf(msg, "State slot %d is empty", slot);
        else          sprintf(msg, "State file does not exist");
        OSD::AddMessage(0xFFA0A0, msg);

        emuThread->emuUnpause();
        return;
    }

    if (ROMManager::LoadState(filename))
    {
        char msg[64];
        if (slot > 0) sprintf(msg, "State loaded from slot %d", slot);
        else          sprintf(msg, "State loaded from file");
        OSD::AddMessage(0, msg);

        actUndoStateLoad->setEnabled(true);
    }
    else
    {
        OSD::AddMessage(0xFFA0A0, "State load failed");
    }

    emuThread->emuUnpause();
}

void MainWindow::onUndoStateLoad()
{
    emuThread->emuPause();
    ROMManager::UndoStateLoad();
    emuThread->emuUnpause();

    OSD::AddMessage(0, "State load undone");
}

void MainWindow::onImportSavefile()
{
    emuThread->emuPause();
    QString path = QFileDialog::getOpenFileName(this,
                                            "Select savefile",
                                            QString::fromStdString(Config::LastROMFolder),
                                            "Savefiles (*.sav *.bin *.dsv);;Any file (*.*)");

    if (path.isEmpty())
    {
        emuThread->emuUnpause();
        return;
    }

    Platform::FileHandle* f = Platform::OpenFile(path.toStdString(), Platform::FileMode::Read);
    if (!f)
    {
        QMessageBox::critical(this, "melonDS", "Could not open the given savefile.");
        emuThread->emuUnpause();
        return;
    }

    if (RunningSomething)
    {
        if (QMessageBox::warning(this,
                        "melonDS",
                        "The emulation will be reset and the current savefile overwritten.",
                        QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
        {
            emuThread->emuUnpause();
            return;
        }

        ROMManager::Reset();
    }

    u32 len = FileLength(f);

    u8* data = new u8[len];
    Platform::FileRewind(f);
    Platform::FileRead(data, len, 1, f);

    NDS::LoadSave(data, len);
    delete[] data;

    CloseFile(f);
    emuThread->emuUnpause();
}

void MainWindow::onQuit()
{
#ifndef _WIN32
    signalSn->setEnabled(false);
#endif
    QApplication::quit();
}


void MainWindow::onPause(bool checked)
{
    if (!RunningSomething) return;

    if (checked)
    {
        emuThread->emuPause();
        OSD::AddMessage(0, "Paused");
        pausedManually = true;
    }
    else
    {
        emuThread->emuUnpause();
        OSD::AddMessage(0, "Resumed");
        pausedManually = false;
    }
}

void MainWindow::onReset()
{
    if (!RunningSomething) return;

    emuThread->emuPause();

    actUndoStateLoad->setEnabled(false);

    ROMManager::Reset();

    OSD::AddMessage(0, "Reset");
    emuThread->emuRun();
}

void MainWindow::onStop()
{
    if (!RunningSomething) return;

    emuThread->emuPause();
    NDS::Stop();
}

void MainWindow::onFrameStep()
{
    if (!RunningSomething) return;

    emuThread->emuFrameStep();
}

void MainWindow::onEnableCheats(bool checked)
{
    Config::EnableCheats = checked?1:0;
    ROMManager::EnableCheats(Config::EnableCheats != 0);
}

void MainWindow::onSetupCheats()
{
    emuThread->emuPause();

    CheatsDialog* dlg = CheatsDialog::openDlg(this);
    connect(dlg, &CheatsDialog::finished, this, &MainWindow::onCheatsDialogFinished);
}

void MainWindow::onCheatsDialogFinished(int res)
{
    emuThread->emuUnpause();
}

void MainWindow::onROMInfo()
{
    ROMInfoDialog* dlg = ROMInfoDialog::openDlg(this);
}

void MainWindow::onRAMInfo()
{
    RAMInfoDialog* dlg = RAMInfoDialog::openDlg(this);
}

void MainWindow::onOpenTitleManager()
{
    TitleManagerDialog* dlg = TitleManagerDialog::openDlg(this);
}

void MainWindow::onMPNewInstance()
{
    //QProcess::startDetached(QApplication::applicationFilePath());
    QProcess newinst;
    newinst.setProgram(QApplication::applicationFilePath());
    newinst.setArguments(QApplication::arguments().mid(1, QApplication::arguments().length()-1));

#ifdef __WIN32__
    newinst.setCreateProcessArgumentsModifier([] (QProcess::CreateProcessArguments *args)
    {
        args->flags |= CREATE_NEW_CONSOLE;
    });
#endif

    newinst.startDetached();
}

void MainWindow::onOpenEmuSettings()
{
    emuThread->emuPause();

    EmuSettingsDialog* dlg = EmuSettingsDialog::openDlg(this);
    connect(dlg, &EmuSettingsDialog::finished, this, &MainWindow::onEmuSettingsDialogFinished);
}

void MainWindow::onEmuSettingsDialogFinished(int res)
{
    emuThread->emuUnpause();

    if (Config::ConsoleType == 1)
    {
        actInsertGBACart->setEnabled(false);
        for (int i = 0; i < 1; i++)
            actInsertGBAAddon[i]->setEnabled(false);
        actEjectGBACart->setEnabled(false);
    }
    else
    {
        actInsertGBACart->setEnabled(true);
        for (int i = 0; i < 1; i++)
            actInsertGBAAddon[i]->setEnabled(true);
        actEjectGBACart->setEnabled(ROMManager::GBACartInserted());
    }

    if (EmuSettingsDialog::needsReset)
        onReset();

    actCurrentGBACart->setText("GBA slot: " + ROMManager::GBACartLabel());

    if (!RunningSomething)
        actTitleManager->setEnabled(!Config::DSiNANDPath.empty());
}

void MainWindow::onOpenPowerManagement()
{
    PowerManagementDialog* dlg = PowerManagementDialog::openDlg(this);
}

void MainWindow::onOpenInputConfig()
{
    emuThread->emuPause();

    InputConfigDialog* dlg = InputConfigDialog::openDlg(this);
    connect(dlg, &InputConfigDialog::finished, this, &MainWindow::onInputConfigFinished);
}

void MainWindow::onInputConfigFinished(int res)
{
    emuThread->emuUnpause();
}

void MainWindow::onOpenVideoSettings()
{
    VideoSettingsDialog* dlg = VideoSettingsDialog::openDlg(this);
    connect(dlg, &VideoSettingsDialog::updateVideoSettings, this, &MainWindow::onUpdateVideoSettings);
}

void MainWindow::onOpenCameraSettings()
{
    emuThread->emuPause();

    camStarted[0] = camManager[0]->isStarted();
    camStarted[1] = camManager[1]->isStarted();
    if (camStarted[0]) camManager[0]->stop();
    if (camStarted[1]) camManager[1]->stop();

    CameraSettingsDialog* dlg = CameraSettingsDialog::openDlg(this);
    connect(dlg, &CameraSettingsDialog::finished, this, &MainWindow::onCameraSettingsFinished);
}

void MainWindow::onCameraSettingsFinished(int res)
{
    if (camStarted[0]) camManager[0]->start();
    if (camStarted[1]) camManager[1]->start();

    emuThread->emuUnpause();
}

void MainWindow::onOpenAudioSettings()
{
    AudioSettingsDialog* dlg = AudioSettingsDialog::openDlg(this, emuThread->emuIsActive());
    connect(emuThread, &EmuThread::syncVolumeLevel, dlg, &AudioSettingsDialog::onSyncVolumeLevel);
    connect(emuThread, &EmuThread::windowEmuStart, dlg, &AudioSettingsDialog::onConsoleReset);
    connect(dlg, &AudioSettingsDialog::updateAudioSettings, this, &MainWindow::onUpdateAudioSettings);
    connect(dlg, &AudioSettingsDialog::finished, this, &MainWindow::onAudioSettingsFinished);
}

void MainWindow::onOpenFirmwareSettings()
{
    emuThread->emuPause();

    FirmwareSettingsDialog* dlg = FirmwareSettingsDialog::openDlg(this);
    connect(dlg, &FirmwareSettingsDialog::finished, this, &MainWindow::onFirmwareSettingsFinished);
}

void MainWindow::onFirmwareSettingsFinished(int res)
{
    if (FirmwareSettingsDialog::needsReset)
        onReset();

    emuThread->emuUnpause();
}

void MainWindow::onOpenPathSettings()
{
    emuThread->emuPause();

    PathSettingsDialog* dlg = PathSettingsDialog::openDlg(this);
    connect(dlg, &PathSettingsDialog::finished, this, &MainWindow::onPathSettingsFinished);
}

void MainWindow::onPathSettingsFinished(int res)
{
    if (PathSettingsDialog::needsReset)
        onReset();

    emuThread->emuUnpause();
}

void MainWindow::onUpdateAudioSettings()
{
    SPU::SetInterpolation(Config::AudioInterp);

    if (Config::AudioBitDepth == 0)
        SPU::SetDegrade10Bit(NDS::ConsoleType == 0);
    else
        SPU::SetDegrade10Bit(Config::AudioBitDepth == 1);
}

void MainWindow::onAudioSettingsFinished(int res)
{
    AudioInOut::UpdateSettings();
}

void MainWindow::onOpenMPSettings()
{
    emuThread->emuPause();

    MPSettingsDialog* dlg = MPSettingsDialog::openDlg(this);
    connect(dlg, &MPSettingsDialog::finished, this, &MainWindow::onMPSettingsFinished);
}

void MainWindow::onMPSettingsFinished(int res)
{
    AudioInOut::AudioMute(mainWindow);
    LocalMP::SetRecvTimeout(Config::MPRecvTimeout);

    emuThread->emuUnpause();
}

void MainWindow::onOpenWifiSettings()
{
    emuThread->emuPause();

    WifiSettingsDialog* dlg = WifiSettingsDialog::openDlg(this);
    connect(dlg, &WifiSettingsDialog::finished, this, &MainWindow::onWifiSettingsFinished);
}

void MainWindow::onWifiSettingsFinished(int res)
{
    Platform::LAN_DeInit();
    Platform::LAN_Init();

    if (WifiSettingsDialog::needsReset)
        onReset();

    emuThread->emuUnpause();
}

void MainWindow::onOpenInterfaceSettings()
{
    emuThread->emuPause();
    InterfaceSettingsDialog* dlg = InterfaceSettingsDialog::openDlg(this);
    connect(dlg, &InterfaceSettingsDialog::finished, this, &MainWindow::onInterfaceSettingsFinished);
    connect(dlg, &InterfaceSettingsDialog::updateMouseTimer, this, &MainWindow::onUpdateMouseTimer);
}

void MainWindow::onUpdateMouseTimer()
{
    panel->mouseTimer->setInterval(Config::MouseHideSeconds*1000);
}

void MainWindow::onInterfaceSettingsFinished(int res)
{
    emuThread->emuUnpause();
}

void MainWindow::onChangeSavestateSRAMReloc(bool checked)
{
    Config::SavestateRelocSRAM = checked?1:0;
}

void MainWindow::onChangeScreenSize()
{
    int factor = ((QAction*)sender())->data().toInt();
    QSize diff = size() - panelWidget->size();
    resize(panel->screenGetMinSize(factor) + diff);
}

void MainWindow::onChangeScreenRotation(QAction* act)
{
    int rot = act->data().toInt();
    Config::ScreenRotation = rot;

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenGap(QAction* act)
{
    int gap = act->data().toInt();
    Config::ScreenGap = gap;

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenLayout(QAction* act)
{
    int layout = act->data().toInt();
    Config::ScreenLayout = layout;

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenSwap(bool checked)
{
    Config::ScreenSwap = checked?1:0;

    // Swap between top and bottom screen when displaying one screen.
    if (Config::ScreenSizing == Frontend::screenSizing_TopOnly)
    {
        // Bottom Screen.
        Config::ScreenSizing = Frontend::screenSizing_BotOnly;
        actScreenSizing[Frontend::screenSizing_TopOnly]->setChecked(false);
        actScreenSizing[Config::ScreenSizing]->setChecked(true);
    }
    else if (Config::ScreenSizing == Frontend::screenSizing_BotOnly)
    {
        // Top Screen.
        Config::ScreenSizing = Frontend::screenSizing_TopOnly;
        actScreenSizing[Frontend::screenSizing_BotOnly]->setChecked(false);
        actScreenSizing[Config::ScreenSizing]->setChecked(true);
    }

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenSizing(QAction* act)
{
    int sizing = act->data().toInt();
    Config::ScreenSizing = sizing;

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenAspect(QAction* act)
{
    int aspect = act->data().toInt();
    QActionGroup* group = act->actionGroup();

    if (group == grpScreenAspectTop)
    {
        Config::ScreenAspectTop = aspect;
    }
    else
    {
        Config::ScreenAspectBot = aspect;
    }

    emit screenLayoutChange();
}

void MainWindow::onChangeIntegerScaling(bool checked)
{
    Config::IntegerScaling = checked?1:0;

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenFiltering(bool checked)
{
    Config::ScreenFilter = checked?1:0;

    emit screenLayoutChange();
}

void MainWindow::onChangeShowOSD(bool checked)
{
    Config::ShowOSD = checked?1:0;
}
void MainWindow::onChangeLimitFramerate(bool checked)
{
    Config::LimitFPS = checked?1:0;
}

void MainWindow::onChangeAudioSync(bool checked)
{
    Config::AudioSync = checked?1:0;
}


void MainWindow::onTitleUpdate(QString title)
{
    setWindowTitle(title);
}

void ToggleFullscreen(MainWindow* mainWindow)
{
    if (!mainWindow->isFullScreen())
    {
        mainWindow->showFullScreen();
        mainWindow->menuBar()->setFixedHeight(0); // Don't use hide() as menubar actions stop working
    }
    else
    {
        mainWindow->showNormal();
        int menuBarHeight = mainWindow->menuBar()->sizeHint().height();
        mainWindow->menuBar()->setFixedHeight(menuBarHeight);
    }
}

void MainWindow::onFullscreenToggled()
{
    ToggleFullscreen(this);
}

void MainWindow::onScreenEmphasisToggled()
{
    int currentSizing = Config::ScreenSizing;
    if (currentSizing == Frontend::screenSizing_EmphTop)
    {
        Config::ScreenSizing = Frontend::screenSizing_EmphBot;
    }
    else if (currentSizing == Frontend::screenSizing_EmphBot)
    {
        Config::ScreenSizing = Frontend::screenSizing_EmphTop;
    }

    emit screenLayoutChange();
}

void MainWindow::onEmuStart()
{
    for (int i = 1; i < 9; i++)
    {
        actSaveState[i]->setEnabled(true);
        actLoadState[i]->setEnabled(ROMManager::SavestateExists(i));
    }
    actSaveState[0]->setEnabled(true);
    actLoadState[0]->setEnabled(true);
    actUndoStateLoad->setEnabled(false);

    actPause->setEnabled(true);
    actPause->setChecked(false);
    actReset->setEnabled(true);
    actStop->setEnabled(true);
    actFrameStep->setEnabled(true);

    actPowerManagement->setEnabled(true);

    actTitleManager->setEnabled(false);
}

void MainWindow::onEmuStop()
{
    emuThread->emuPause();

    for (int i = 0; i < 9; i++)
    {
        actSaveState[i]->setEnabled(false);
        actLoadState[i]->setEnabled(false);
    }
    actUndoStateLoad->setEnabled(false);

    actPause->setEnabled(false);
    actReset->setEnabled(false);
    actStop->setEnabled(false);
    actFrameStep->setEnabled(false);

    actPowerManagement->setEnabled(false);

    actTitleManager->setEnabled(!Config::DSiNANDPath.empty());
}

void MainWindow::onUpdateVideoSettings(bool glchange)
{
    if (glchange)
    {
        emuThread->emuPause();
        if (hasOGL) emuThread->deinitContext();

        delete panel;
        createScreenPanel();
        connect(emuThread, SIGNAL(windowUpdate()), panelWidget, SLOT(repaint()));
    }

    videoSettingsDirty = true;

    if (glchange)
    {
        if (hasOGL) emuThread->initContext();
        emuThread->emuUnpause();
    }
}


void emuStop()
{
    RunningSomething = false;

    emit emuThread->windowEmuStop();
}

MelonApplication::MelonApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
#ifndef __APPLE__
    setWindowIcon(QIcon(":/melon-icon"));
#endif
}

bool MelonApplication::event(QEvent *event)
{
    if (event->type() == QEvent::FileOpen)
    {
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent*>(event);

        emuThread->emuPause();
        const QStringList file = mainWindow->splitArchivePath(openEvent->file(), true);
        if (!mainWindow->preloadROMs(file, {}, true))
            emuThread->emuUnpause();
    }

    return QApplication::event(event);
}

int main(int argc, char** argv)
{
    srand(time(nullptr));

    qputenv("QT_SCALE_FACTOR", "1");

    printf("melonDS " MELONDS_VERSION "\n");
    printf(MELONDS_URL "\n");

    // easter egg - not worth checking other cases for something so dumb
    if (argc != 0 && (!strcasecmp(argv[0], "derpDS") || !strcasecmp(argv[0], "./derpDS")))
        printf("did you just call me a derp???\n");

    Platform::Init(argc, argv);

    MelonApplication melon(argc, argv);

    CLI::CommandLineOptions* options = CLI::ManageArgs(melon);

    // http://stackoverflow.com/questions/14543333/joystick-wont-work-using-sdl
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_HAPTIC) < 0)
    {
        printf("SDL couldn't init rumble\n");
    }
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
    {
        printf("SDL couldn't init joystick\n");
    }
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        const char* err = SDL_GetError();
        QString errorStr = "Failed to initialize SDL. This could indicate an issue with your audio driver.\n\nThe error was: ";
        errorStr += err;

        QMessageBox::critical(NULL, "melonDS", errorStr);
        return 1;
    }

    SDL_JoystickEventState(SDL_ENABLE);

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_EnableScreenSaver(); SDL_DisableScreenSaver();

    Config::Load();

#define SANITIZE(var, min, max)  { var = std::clamp(var, min, max); }
    SANITIZE(Config::ConsoleType, 0, 1);
    SANITIZE(Config::_3DRenderer,
    0,
    0 // Minimum, Software renderer
    #ifdef OGLRENDERER_ENABLED
    + 1 // OpenGL Renderer
    #endif
    );
    SANITIZE(Config::ScreenVSyncInterval, 1, 20);
    SANITIZE(Config::GL_ScaleFactor, 1, 16);
    SANITIZE(Config::AudioInterp, 0, 3);
    SANITIZE(Config::AudioVolume, 0, 256);
    SANITIZE(Config::MicInputType, 0, (int)micInputType_MAX);
    SANITIZE(Config::ScreenRotation, 0, (int)Frontend::screenRot_MAX);
    SANITIZE(Config::ScreenGap, 0, 500);
    SANITIZE(Config::ScreenLayout, 0, (int)Frontend::screenLayout_MAX);
    SANITIZE(Config::ScreenSizing, 0, (int)Frontend::screenSizing_MAX);
    SANITIZE(Config::ScreenAspectTop, 0, AspectRatiosNum);
    SANITIZE(Config::ScreenAspectBot, 0, AspectRatiosNum);
#undef SANITIZE

    AudioInOut::Init();
    camStarted[0] = false;
    camStarted[1] = false;
    camManager[0] = new CameraManager(0, 640, 480, true);
    camManager[1] = new CameraManager(1, 640, 480, true);
    camManager[0]->setXFlip(Config::Camera[0].XFlip);
    camManager[1]->setXFlip(Config::Camera[1].XFlip);

    ROMManager::EnableCheats(Config::EnableCheats != 0);

    Input::JoystickID = Config::JoystickID;
    Input::OpenJoystick();

    mainWindow = new MainWindow();
    if (options->fullscreen)
        ToggleFullscreen(mainWindow);

    emuThread = new EmuThread();
    emuThread->start();
    emuThread->emuPause();

    AudioInOut::AudioMute(mainWindow);

    QObject::connect(&melon, &QApplication::applicationStateChanged, mainWindow, &MainWindow::onAppStateChanged);

    bool memberSyntaxUsed = false;
    const auto prepareRomPath = [&](const std::optional<QString>& romPath, const std::optional<QString>& romArchivePath) -> QStringList
    {
        if (!romPath.has_value())
            return {};

        if (romArchivePath.has_value())
            return { *romPath, *romArchivePath };

        const QStringList path = mainWindow->splitArchivePath(*romPath, true);
        if (path.size() > 1) memberSyntaxUsed = true;
        return path;
    };

    const QStringList dsfile = prepareRomPath(options->dsRomPath, options->dsRomArchivePath);
    const QStringList gbafile = prepareRomPath(options->gbaRomPath, options->gbaRomArchivePath);

    if (memberSyntaxUsed) printf("Warning: use the a.zip|b.nds format at your own risk!\n");

    mainWindow->preloadROMs(dsfile, gbafile, options->boot);

    int ret = melon.exec();

    emuThread->emuStop();
    emuThread->wait();
    delete emuThread;

    Input::CloseJoystick();

    AudioInOut::DeInit();
    delete camManager[0];
    delete camManager[1];

    Config::Save();

    SDL_Quit();
    Platform::DeInit();
    return ret;
}

#ifdef __WIN32__

#include <windows.h>

int CALLBACK WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int cmdshow)
{
    int ret = main(__argc, __argv);

    printf("\n\n>");

    return ret;
}

#endif
