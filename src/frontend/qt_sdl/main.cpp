/*
    Copyright 2016-2020 Arisotura

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

#include <QApplication>
#include <QMessageBox>
#include <QMenuBar>
#include <QFileDialog>
#include <QPaintEvent>
#include <QPainter>
#include <QKeyEvent>
#include <QMimeData>

#include <SDL2/SDL.h>

#include "main.h"
#include "Input.h"
#include "CheatsDialog.h"
#include "EmuSettingsDialog.h"
#include "InputConfigDialog.h"
#include "VideoSettingsDialog.h"
#include "AudioSettingsDialog.h"
#include "WifiSettingsDialog.h"

#include "types.h"
#include "version.h"

#include "FrontendUtil.h"
#include "OSD.h"

#include "NDS.h"
#include "GBACart.h"
#ifdef OGLRENDERER_ENABLED
#include "OpenGLSupport.h"
#endif
#include "GPU.h"
#include "SPU.h"
#include "Wifi.h"
#include "Platform.h"
#include "Config.h"
#include "PlatformConfig.h"

#include "Savestate.h"

#include "main_shaders.h"


// TODO: uniform variable spelling

bool RunningSomething;

MainWindow* mainWindow;
EmuThread* emuThread;

int autoScreenSizing = 0;

int videoRenderer;
GPU::RenderSettings videoSettings;
bool videoSettingsDirty;

SDL_AudioDeviceID audioDevice;
int audioFreq;
SDL_cond* audioSync;
SDL_mutex* audioSyncLock;

SDL_AudioDeviceID micDevice;
s16 micExtBuffer[2048];
u32 micExtBufferWritePos;

u32 micWavLength;
s16* micWavBuffer;


void audioCallback(void* data, Uint8* stream, int len)
{
    len /= (sizeof(s16) * 2);

    // resample incoming audio to match the output sample rate

    int len_in = Frontend::AudioOut_GetNumSamples(len);
    s16 buf_in[1024*2];
    int num_in;

    SDL_LockMutex(audioSyncLock);
    num_in = SPU::ReadOutput(buf_in, len_in);
    SDL_CondSignal(audioSync);
    SDL_UnlockMutex(audioSyncLock);

    if (num_in < 1)
    {
        memset(stream, 0, len*sizeof(s16)*2);
        return;
    }

    int margin = 6;
    if (num_in < len_in-margin)
    {
        int last = num_in-1;

        for (int i = num_in; i < len_in-margin; i++)
            ((u32*)buf_in)[i] = ((u32*)buf_in)[last];

        num_in = len_in-margin;
    }

    Frontend::AudioOut_Resample(buf_in, num_in, (s16*)stream, len, Config::AudioVolume);
}


void micLoadWav(const char* name)
{
    SDL_AudioSpec format;
    memset(&format, 0, sizeof(SDL_AudioSpec));

    if (micWavBuffer) delete[] micWavBuffer;
    micWavBuffer = nullptr;
    micWavLength = 0;

    u8* buf;
    u32 len;
    if (!SDL_LoadWAV(name, &format, &buf, &len))
        return;

    const u64 dstfreq = 44100;

    if (format.format == AUDIO_S16 || format.format == AUDIO_U16)
    {
        int srcinc = format.channels;
        len /= (2 * srcinc);

        micWavLength = (len * dstfreq) / format.freq;
        if (micWavLength < 735) micWavLength = 735;
        micWavBuffer = new s16[micWavLength];

        float res_incr = len / (float)micWavLength;
        float res_timer = 0;
        int res_pos = 0;

        for (int i = 0; i < micWavLength; i++)
        {
            u16 val = ((u16*)buf)[res_pos];
            if (SDL_AUDIO_ISUNSIGNED(format.format)) val ^= 0x8000;

            micWavBuffer[i] = val;

            res_timer += res_incr;
            while (res_timer >= 1.0)
            {
                res_timer -= 1.0;
                res_pos += srcinc;
            }
        }
    }
    else if (format.format == AUDIO_S8 || format.format == AUDIO_U8)
    {
        int srcinc = format.channels;
        len /= srcinc;

        micWavLength = (len * dstfreq) / format.freq;
        if (micWavLength < 735) micWavLength = 735;
        micWavBuffer = new s16[micWavLength];

        float res_incr = len / (float)micWavLength;
        float res_timer = 0;
        int res_pos = 0;

        for (int i = 0; i < micWavLength; i++)
        {
            u16 val = buf[res_pos] << 8;
            if (SDL_AUDIO_ISUNSIGNED(format.format)) val ^= 0x8000;

            micWavBuffer[i] = val;

            res_timer += res_incr;
            while (res_timer >= 1.0)
            {
                res_timer -= 1.0;
                res_pos += srcinc;
            }
        }
    }
    else
        printf("bad WAV format %08X\n", format.format);

    SDL_FreeWAV(buf);
}

void micCallback(void* data, Uint8* stream, int len)
{
    s16* input = (s16*)stream;
    len /= sizeof(s16);

    int maxlen = sizeof(micExtBuffer) / sizeof(s16);

    if ((micExtBufferWritePos + len) > maxlen)
    {
        u32 len1 = maxlen - micExtBufferWritePos;
        memcpy(&micExtBuffer[micExtBufferWritePos], &input[0], len1*sizeof(s16));
        memcpy(&micExtBuffer[0], &input[len1], (len - len1)*sizeof(s16));
        micExtBufferWritePos = len - len1;
    }
    else
    {
        memcpy(&micExtBuffer[micExtBufferWritePos], input, len*sizeof(s16));
        micExtBufferWritePos += len;
    }
}

void micProcess()
{
    int type = Config::MicInputType;
    bool cmd = Input::HotkeyDown(HK_Mic);

    if (type != 1 && !cmd)
    {
        type = 0;
    }

    switch (type)
    {
    case 0: // no mic
        Frontend::Mic_FeedSilence();
        break;

    case 1: // host mic
    case 3: // WAV
        Frontend::Mic_FeedExternalBuffer();
        break;

    case 2: // white noise
        Frontend::Mic_FeedNoise();
        break;
    }
}


EmuThread::EmuThread(QObject* parent) : QThread(parent)
{
    EmuStatus = 0;
    EmuRunning = 2;
    EmuPause = 0;
    RunningSomething = false;

    connect(this, SIGNAL(windowUpdate()), mainWindow->panel, SLOT(update()));
    connect(this, SIGNAL(windowTitleChange(QString)), mainWindow, SLOT(onTitleUpdate(QString)));
    connect(this, SIGNAL(windowEmuStart()), mainWindow, SLOT(onEmuStart()));
    connect(this, SIGNAL(windowEmuStop()), mainWindow, SLOT(onEmuStop()));
    connect(this, SIGNAL(windowEmuPause()), mainWindow->actPause, SLOT(trigger()));
    connect(this, SIGNAL(windowEmuReset()), mainWindow->actReset, SLOT(trigger()));
    connect(this, SIGNAL(screenLayoutChange()), mainWindow->panel, SLOT(onScreenLayoutChanged()));
    connect(this, SIGNAL(windowFullscreenToggle()), mainWindow, SLOT(onFullscreenToggled()));

    if (mainWindow->hasOGL) initOpenGL();
}

void EmuThread::initOpenGL()
{
    QOpenGLContext* windowctx = mainWindow->getOGLContext();
    QSurfaceFormat format = windowctx->format();

    format.setSwapInterval(0);

    oglSurface = new QOffscreenSurface();
    oglSurface->setFormat(format);
    oglSurface->create();
    if (!oglSurface->isValid())
    {
        // TODO handle this!
        printf("oglSurface shat itself :(\n");
        delete oglSurface;
        return;
    }

    oglContext = new QOpenGLContext();
    oglContext->setFormat(oglSurface->format());
    oglContext->setShareContext(windowctx);
    if (!oglContext->create())
    {
        // TODO handle this!
        printf("oglContext shat itself :(\n");
        delete oglContext;
        delete oglSurface;
        return;
    }

    oglContext->moveToThread(this);
}

void EmuThread::deinitOpenGL()
{
    delete oglContext;
    delete oglSurface;
}

void* oglGetProcAddress(const char* proc)
{
    return emuThread->oglGetProcAddress(proc);
}

void* EmuThread::oglGetProcAddress(const char* proc)
{
    return (void*)oglContext->getProcAddress(proc);
}

void EmuThread::run()
{
    bool hasOGL = mainWindow->hasOGL;
    u32 mainScreenPos[3];

    NDS::Init();

    mainScreenPos[0] = 0;
    mainScreenPos[1] = 0;
    mainScreenPos[2] = 0;
    autoScreenSizing = 0;

    videoSettingsDirty = false;
    videoSettings.Soft_Threaded = Config::Threaded3D != 0;
    videoSettings.GL_ScaleFactor = Config::GL_ScaleFactor;

#ifdef OGLRENDERER_ENABLED
    if (hasOGL)
    {
        oglContext->makeCurrent(oglSurface);
        videoRenderer = OpenGL::Init() ? Config::_3DRenderer : 0;
    }
    else
#endif
    {
        videoRenderer = 0;
    }

    GPU::InitRenderer(videoRenderer);
    GPU::SetRenderSettings(videoRenderer, videoSettings);

    Input::Init();

    u32 nframes = 0;
    u32 starttick = SDL_GetTicks();
    u32 lasttick = starttick;
    u32 lastmeasuretick = lasttick;
    u32 fpslimitcount = 0;

    char melontitle[100];

    while (EmuRunning != 0)
    {
        Input::Process();

        if (Input::HotkeyPressed(HK_FastForwardToggle)) emit windowLimitFPSChange();

        if (Input::HotkeyPressed(HK_Pause)) emit windowEmuPause();
        if (Input::HotkeyPressed(HK_Reset)) emit windowEmuReset();
        
        if (Input::HotkeyPressed(HK_FullscreenToggle)) emit windowFullscreenToggle();

        if (GBACart::CartInserted && GBACart::HasSolarSensor)
        {
            if (Input::HotkeyPressed(HK_SolarSensorDecrease))
            {
                if (GBACart_SolarSensor::LightLevel > 0) GBACart_SolarSensor::LightLevel--;
                char msg[64];
                sprintf(msg, "Solar sensor level set to %d", GBACart_SolarSensor::LightLevel);
                OSD::AddMessage(0, msg);
            }
            if (Input::HotkeyPressed(HK_SolarSensorIncrease))
            {
                if (GBACart_SolarSensor::LightLevel < 10) GBACart_SolarSensor::LightLevel++;
                char msg[64];
                sprintf(msg, "Solar sensor level set to %d", GBACart_SolarSensor::LightLevel);
                OSD::AddMessage(0, msg);
            }
        }

        if (EmuRunning == 1)
        {
            EmuStatus = 1;

            // update render settings if needed
            if (videoSettingsDirty)
            {
                if (hasOGL != mainWindow->hasOGL)
                {
                    hasOGL = mainWindow->hasOGL;
#ifdef OGLRENDERER_ENABLED
                    if (hasOGL)
                    {
                        oglContext->makeCurrent(oglSurface);
                        videoRenderer = OpenGL::Init() ? Config::_3DRenderer : 0;
                    }
                    else
#endif
                    {
                        videoRenderer = 0;
                    }
                }
                else
                    videoRenderer = hasOGL ? Config::_3DRenderer : 0;

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
            micProcess();

            // auto screen layout
            if (Config::ScreenSizing == 3)
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
                    guess = 0;
                }
                else
                {
                    if (mainScreenPos[0] == 1)
                        guess = 1;
                    else
                        guess = 2;
                }

                if (guess != autoScreenSizing)
                {
                    autoScreenSizing = guess;
                    emit screenLayoutChange();
                }
            }

            // emulate
            u32 nlines = NDS::RunFrame();

#ifdef MELONCAP
            MelonCap::Update();
#endif // MELONCAP

            if (EmuRunning == 0) break;

            emit windowUpdate();

            bool fastforward = Input::HotkeyDown(HK_FastForward);

            if (Config::AudioSync && (!fastforward) && audioDevice)
            {
                SDL_LockMutex(audioSyncLock);
                while (SPU::GetOutputSize() > 1024)
                {
                    int ret = SDL_CondWaitTimeout(audioSync, audioSyncLock, 500);
                    if (ret == SDL_MUTEX_TIMEDOUT) break;
                }
                SDL_UnlockMutex(audioSyncLock);
            }

            float framerate = (1000.0f * nlines) / (60.0f * 263.0f);

            {
                u32 curtick = SDL_GetTicks();
                u32 delay = curtick - lasttick;

                bool limitfps = Config::LimitFPS && !fastforward;
                if (limitfps)
                {
                    float wantedtickF = starttick + (framerate * (fpslimitcount+1));
                    u32 wantedtick = (u32)ceil(wantedtickF);
                    if (curtick < wantedtick) SDL_Delay(wantedtick - curtick);

                    lasttick = SDL_GetTicks();
                    fpslimitcount++;
                    if ((abs(wantedtickF - (float)wantedtick) < 0.001312) || (fpslimitcount > 60))
                    {
                        fpslimitcount = 0;
                        starttick = lasttick;
                    }
                }
                else
                {
                    if (delay < 1) SDL_Delay(1);
                    lasttick = SDL_GetTicks();
                }
            }

            nframes++;
            if (nframes >= 30)
            {
                u32 tick = SDL_GetTicks();
                u32 diff = tick - lastmeasuretick;
                lastmeasuretick = tick;

                u32 fps;
                if (diff < 1) fps = 77777;
                else fps = (nframes * 1000) / diff;
                nframes = 0;

                float fpstarget;
                if (framerate < 1) fpstarget = 999;
                else fpstarget = 1000.0f/framerate;

                sprintf(melontitle, "[%d/%.0f] melonDS " MELONDS_VERSION, fps, fpstarget);
                changeWindowTitle(melontitle);
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

            emit windowUpdate();

            EmuStatus = EmuRunning;

            sprintf(melontitle, "melonDS " MELONDS_VERSION);
            changeWindowTitle(melontitle);

            SDL_Delay(75);
        }
    }

    EmuStatus = 0;

    GPU::DeInitRenderer();
    NDS::DeInit();
    //Platform::LAN_DeInit();

    if (hasOGL)
    {
        oglContext->doneCurrent();
        deinitOpenGL();
    }
}

void EmuThread::changeWindowTitle(char* title)
{
    emit windowTitleChange(QString(title));
}

void EmuThread::emuRun()
{
    EmuRunning = 1;
    EmuPause = 0;
    RunningSomething = true;

    // checkme
    emit windowEmuStart();
    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 0);
    if (micDevice)   SDL_PauseAudioDevice(micDevice, 0);
}

void EmuThread::emuPause()
{
    EmuPause++;
    if (EmuPause > 1) return;

    PrevEmuStatus = EmuRunning;
    EmuRunning = 2;
    while (EmuStatus != 2);

    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 1);
    if (micDevice)   SDL_PauseAudioDevice(micDevice, 1);
}

void EmuThread::emuUnpause()
{
    if (EmuPause < 1) return;

    EmuPause--;
    if (EmuPause > 0) return;

    EmuRunning = PrevEmuStatus;

    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 0);
    if (micDevice)   SDL_PauseAudioDevice(micDevice, 0);
}

void EmuThread::emuStop()
{
    EmuRunning = 0;
    EmuPause = 0;

    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 1);
    if (micDevice)   SDL_PauseAudioDevice(micDevice, 1);
}

bool EmuThread::emuIsRunning()
{
    return (EmuRunning == 1);
}


void ScreenHandler::screenSetupLayout(int w, int h)
{
    int sizing = Config::ScreenSizing;
    if (sizing == 3) sizing = autoScreenSizing;

    Frontend::SetupScreenLayout(w, h,
                                Config::ScreenLayout,
                                Config::ScreenRotation,
                                sizing,
                                Config::ScreenGap,
                                Config::IntegerScaling != 0);

    Frontend::GetScreenTransforms(screenMatrix[0], screenMatrix[1]);
}

QSize ScreenHandler::screenGetMinSize()
{
    bool isHori = (Config::ScreenRotation == 1 || Config::ScreenRotation == 3);
    int gap = Config::ScreenGap;

    int w = 256;
    int h = 192;

    if (Config::ScreenLayout == 0) // natural
    {
        if (isHori)
            return QSize(h+gap+h, w);
        else
            return QSize(w, h+gap+h);
    }
    else if (Config::ScreenLayout == 1) // vertical
    {
        if (isHori)
            return QSize(h, w+gap+w);
        else
            return QSize(w, h+gap+h);
    }
    else // horizontal
    {
        if (isHori)
            return QSize(h+gap+h, w);
        else
            return QSize(w+gap+w, h);
    }
}

void ScreenHandler::screenOnMousePress(QMouseEvent* event)
{
    event->accept();
    if (event->button() != Qt::LeftButton) return;

    int x = event->pos().x();
    int y = event->pos().y();

    Frontend::GetTouchCoords(x, y);

    if (x >= 0 && x < 256 && y >= 0 && y < 192)
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
    if (!(event->buttons() & Qt::LeftButton)) return;
    if (!touching) return;

    int x = event->pos().x();
    int y = event->pos().y();

    Frontend::GetTouchCoords(x, y);

    // clamp to screen range
    if (x < 0) x = 0;
    else if (x > 255) x = 255;
    if (y < 0) y = 0;
    else if (y > 191) y = 191;

    NDS::TouchScreen(x, y);
}


ScreenPanelNative::ScreenPanelNative(QWidget* parent) : QWidget(parent)
{
    screen[0] = QImage(256, 192, QImage::Format_RGB32);
    screen[1] = QImage(256, 192, QImage::Format_RGB32);

    screenTrans[0].reset();
    screenTrans[1].reset();

    touching = false;

    OSD::Init(nullptr);
}

ScreenPanelNative::~ScreenPanelNative()
{
    OSD::DeInit(nullptr);
}

void ScreenPanelNative::setupScreenLayout()
{
    int w = width();
    int h = height();
    float* mtx;

    screenSetupLayout(w, h);

    mtx = screenMatrix[0];
    screenTrans[0].setMatrix(mtx[0], mtx[1], 0.f,
                             mtx[2], mtx[3], 0.f,
                             mtx[4], mtx[5], 1.f);

    mtx = screenMatrix[1];
    screenTrans[1].setMatrix(mtx[0], mtx[1], 0.f,
                             mtx[2], mtx[3], 0.f,
                             mtx[4], mtx[5], 1.f);
}

void ScreenPanelNative::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    // fill background
    painter.fillRect(event->rect(), QColor::fromRgb(0, 0, 0));

    int frontbuf = GPU::FrontBuffer;
    if (!GPU::Framebuffer[frontbuf][0] || !GPU::Framebuffer[frontbuf][1]) return;

    memcpy(screen[0].scanLine(0), GPU::Framebuffer[frontbuf][0], 256*192*4);
    memcpy(screen[1].scanLine(0), GPU::Framebuffer[frontbuf][1], 256*192*4);

    painter.setRenderHint(QPainter::SmoothPixmapTransform, Config::ScreenFilter!=0);

    QRect screenrc(0, 0, 256, 192);

    painter.setTransform(screenTrans[0]);
    painter.drawImage(screenrc, screen[0]);

    painter.setTransform(screenTrans[1]);
    painter.drawImage(screenrc, screen[1]);

    OSD::Update(nullptr);
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

void ScreenPanelNative::onScreenLayoutChanged()
{
    setMinimumSize(screenGetMinSize());
    setupScreenLayout();
}


ScreenPanelGL::ScreenPanelGL(QWidget* parent) : QOpenGLWidget(parent)
{
    touching = false;

}

ScreenPanelGL::~ScreenPanelGL()
{
    makeCurrent();

    OSD::DeInit(this);

    glDeleteTextures(1, &screenTexture);

    glDeleteVertexArrays(1, &screenVertexArray);
    glDeleteBuffers(1, &screenVertexBuffer);

    delete screenShader;

    doneCurrent();
}

void ScreenPanelGL::setupScreenLayout()
{
    int w = width();
    int h = height();

    screenSetupLayout(w, h);
}

void ScreenPanelGL::initializeGL()
{
    initializeOpenGLFunctions();

    const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
    const GLubyte* version = glGetString(GL_VERSION); // version as a string
    printf("OpenGL: renderer: %s\n", renderer);
    printf("OpenGL: version: %s\n", version);

    glClearColor(0, 0, 0, 1);

    screenShader = new QOpenGLShaderProgram(this);
    screenShader->addShaderFromSourceCode(QOpenGLShader::Vertex, kScreenVS);
    screenShader->addShaderFromSourceCode(QOpenGLShader::Fragment, kScreenFS);

    GLuint pid = screenShader->programId();
    glBindAttribLocation(pid, 0, "vPosition");
    glBindAttribLocation(pid, 1, "vTexcoord");
    glBindFragDataLocation(pid, 0, "oColor");

    screenShader->link();

    screenShader->bind();
    screenShader->setUniformValue("ScreenTex", (GLint)0);
    screenShader->release();


    float vertices[] =
    {
        0,   0,    0, 0,
        0,   192,  0, 0.5,
        256, 192,  1, 0.5,
        0,   0,    0, 0,
        256, 192,  1, 0.5,
        256, 0,    1, 0,

        0,   0,    0, 0.5,
        0,   192,  0, 1,
        256, 192,  1, 1,
        0,   0,    0, 0.5,
        256, 192,  1, 1,
        256, 0,    1, 0.5
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192*2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    OSD::Init(this);
}

void ScreenPanelGL::paintGL()
{
    int w = width();
    int h = height();
    float factor = devicePixelRatioF();

    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(0, 0, w*factor, h*factor);

    screenShader->bind();

    screenShader->setUniformValue("uScreenSize", (float)w*factor, (float)h*factor);

    int frontbuf = GPU::FrontBuffer;
    glActiveTexture(GL_TEXTURE0);

#ifdef OGLRENDERER_ENABLED
    if (GPU::Renderer != 0)
    {
        // hardware-accelerated render
        GPU::GLCompositor::BindOutputTexture();
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
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 192, 256, 192, GL_RGBA,
                            GL_UNSIGNED_BYTE, GPU::Framebuffer[frontbuf][1]);
        }
    }

    GLint filter = Config::ScreenFilter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    glBindBuffer(GL_ARRAY_BUFFER, screenVertexBuffer);
    glBindVertexArray(screenVertexArray);

    GLint transloc = screenShader->uniformLocation("uTransform");

    glUniformMatrix2x3fv(transloc, 1, GL_TRUE, screenMatrix[0]);
    glDrawArrays(GL_TRIANGLES, 0, 2*3);

    glUniformMatrix2x3fv(transloc, 1, GL_TRUE, screenMatrix[1]);
    glDrawArrays(GL_TRIANGLES, 2*3, 2*3);

    screenShader->release();

    OSD::Update(this);
    OSD::DrawGL(this, w*factor, h*factor);
}

void ScreenPanelGL::resizeEvent(QResizeEvent* event)
{
    setupScreenLayout();

    QOpenGLWidget::resizeEvent(event);
}

void ScreenPanelGL::resizeGL(int w, int h)
{
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

void ScreenPanelGL::onScreenLayoutChanged()
{
    setMinimumSize(screenGetMinSize());
    setupScreenLayout();
}


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("melonDS " MELONDS_VERSION);
    setAttribute(Qt::WA_DeleteOnClose);
    setAcceptDrops(true);

    QMenuBar* menubar = new QMenuBar();
    {
        QMenu* menu = menubar->addMenu("File");

        actOpenROM = menu->addAction("Open ROM...");
        connect(actOpenROM, &QAction::triggered, this, &MainWindow::onOpenFile);

        //actBootFirmware = menu->addAction("Launch DS menu");
        actBootFirmware = menu->addAction("Boot firmware");
        connect(actBootFirmware, &QAction::triggered, this, &MainWindow::onBootFirmware);

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

        actImportSavefile = menu->addAction("Import savefile");
        connect(actImportSavefile, &QAction::triggered, this, &MainWindow::onImportSavefile);

        menu->addSeparator();

        actQuit = menu->addAction("Quit");
        connect(actQuit, &QAction::triggered, this, &MainWindow::onQuit);
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

        menu->addSeparator();

        actEnableCheats = menu->addAction("Enable cheats");
        actEnableCheats->setCheckable(true);
        connect(actEnableCheats, &QAction::triggered, this, &MainWindow::onEnableCheats);

        actSetupCheats = menu->addAction("Setup cheat codes");
        connect(actSetupCheats, &QAction::triggered, this, &MainWindow::onSetupCheats);
    }
    {
        QMenu* menu = menubar->addMenu("Config");

        actEmuSettings = menu->addAction("Emu settings");
        connect(actEmuSettings, &QAction::triggered, this, &MainWindow::onOpenEmuSettings);

        actInputConfig = menu->addAction("Input and hotkeys");
        connect(actInputConfig, &QAction::triggered, this, &MainWindow::onOpenInputConfig);

        actVideoSettings = menu->addAction("Video settings");
        connect(actVideoSettings, &QAction::triggered, this, &MainWindow::onOpenVideoSettings);

        actAudioSettings = menu->addAction("Audio settings");
        connect(actAudioSettings, &QAction::triggered, this, &MainWindow::onOpenAudioSettings);

        actWifiSettings = menu->addAction("Wifi settings");
        connect(actWifiSettings, &QAction::triggered, this, &MainWindow::onOpenWifiSettings);

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

            for (int i = 0; i < 4; i++)
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

            const char* screenlayout[] = {"Natural", "Vertical", "Horizontal"};

            for (int i = 0; i < 3; i++)
            {
                actScreenLayout[i] = submenu->addAction(QString(screenlayout[i]));
                actScreenLayout[i]->setActionGroup(grpScreenLayout);
                actScreenLayout[i]->setData(QVariant(i));
                actScreenLayout[i]->setCheckable(true);
            }

            connect(grpScreenLayout, &QActionGroup::triggered, this, &MainWindow::onChangeScreenLayout);
        }
        {
            QMenu* submenu = menu->addMenu("Screen sizing");
            grpScreenSizing = new QActionGroup(submenu);

            const char* screensizing[] = {"Even", "Emphasize top", "Emphasize bottom", "Auto"};

            for (int i = 0; i < 4; i++)
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

    show();
    createScreenPanel();

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

    actSetupCheats->setEnabled(false);


    actEnableCheats->setChecked(Config::EnableCheats != 0);

    actSavestateSRAMReloc->setChecked(Config::SavestateRelocSRAM != 0);

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
    actIntegerScaling->setChecked(Config::IntegerScaling != 0);

    actScreenFiltering->setChecked(Config::ScreenFilter != 0);
    actShowOSD->setChecked(Config::ShowOSD != 0);

    actLimitFramerate->setChecked(Config::LimitFPS != 0);
    actAudioSync->setChecked(Config::AudioSync != 0);
}

MainWindow::~MainWindow()
{
}

void MainWindow::createScreenPanel()
{
    hasOGL = (Config::ScreenUseGL != 0) || (Config::_3DRenderer != 0);

    if (hasOGL)
    {
        ScreenPanelGL* panelGL = new ScreenPanelGL(this);
        panelGL->show();

        if (!panelGL->isValid())
            hasOGL = false;
        else
        {
            QSurfaceFormat fmt = panelGL->format();
            if (fmt.majorVersion() < 3 || (fmt.majorVersion() == 3 && fmt.minorVersion() < 2))
                hasOGL = false;
        }

        if (!hasOGL)
            delete panelGL;
        else
            panel = panelGL;
    }

    if (!hasOGL)
    {
        panel = new ScreenPanelNative(this);
        panel->show();
    }

    setCentralWidget(panel);
    connect(this, SIGNAL(screenLayoutChange()), panel, SLOT(onScreenLayoutChanged()));
    emit screenLayoutChange();
}

QOpenGLContext* MainWindow::getOGLContext()
{
    if (!hasOGL) return nullptr;

    QOpenGLWidget* glpanel = (QOpenGLWidget*)panel;
    return glpanel->context();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    int w = event->size().width();
    int h = event->size().height();

    Config::WindowWidth = w;
    Config::WindowHeight = h;

    // TODO: detect when the window gets maximized!
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    if (event->key() == Qt::Key_F11) NDS::debug(0);

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
    QString ext = filename.right(3);

    if (ext == "nds" || ext == "srl" || ext == "dsi" || (ext == "gba" && RunningSomething))
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasUrls()) return;

    QList<QUrl> urls = event->mimeData()->urls();
    if (urls.count() > 1) return; // not handling more than one file at once

    emuThread->emuPause();

    QString filename = urls.at(0).toLocalFile();
    QString ext = filename.right(3);

    char _filename[1024];
    strncpy(_filename, filename.toStdString().c_str(), 1023); _filename[1023] = '\0';

    int slot; int res;
    if (ext == "gba")
    {
        slot = 1;
        res = Frontend::LoadROM(_filename, Frontend::ROMSlot_GBA);
    }
    else
    {
        slot = 0;
        res = Frontend::LoadROM(_filename, Frontend::ROMSlot_NDS);
    }

    if (res != Frontend::Load_OK)
    {
        QMessageBox::critical(this,
                              "melonDS",
                              loadErrorStr(res));
        emuThread->emuUnpause();
    }
    else if (slot == 1)
    {
        // checkme
        emuThread->emuUnpause();
    }
    else
    {
        emuThread->emuRun();
    }
}


QString MainWindow::loadErrorStr(int error)
{
    switch (error)
    {
    case Frontend::Load_BIOS9Missing:
        return "DS ARM9 BIOS was not found or could not be accessed. Check your emu settings.";
    case Frontend::Load_BIOS9Bad:
        return "DS ARM9 BIOS is not a valid BIOS dump.";

    case Frontend::Load_BIOS7Missing:
        return "DS ARM7 BIOS was not found or could not be accessed. Check your emu settings.";
    case Frontend::Load_BIOS7Bad:
        return "DS ARM7 BIOS is not a valid BIOS dump.";

    case Frontend::Load_FirmwareMissing:
        return "DS firmware was not found or could not be accessed. Check your emu settings.";
    case Frontend::Load_FirmwareBad:
        return "DS firmware is not a valid firmware dump.";
    case Frontend::Load_FirmwareNotBootable:
        return "DS firmware is not bootable.";

    case Frontend::Load_DSiBIOS9Missing:
        return "DSi ARM9 BIOS was not found or could not be accessed. Check your emu settings.";
    case Frontend::Load_DSiBIOS9Bad:
        return "DSi ARM9 BIOS is not a valid BIOS dump.";

    case Frontend::Load_DSiBIOS7Missing:
        return "DSi ARM7 BIOS was not found or could not be accessed. Check your emu settings.";
    case Frontend::Load_DSiBIOS7Bad:
        return "DSi ARM7 BIOS is not a valid BIOS dump.";

    case Frontend::Load_DSiNANDMissing:
        return "DSi NAND was not found or could not be accessed. Check your emu settings.";
    case Frontend::Load_DSiNANDBad:
        return "DSi NAND is not a valid NAND dump.";

    case Frontend::Load_ROMLoadError:
        return "Failed to load the ROM. Make sure the file is accessible and isn't used by another application.";

    default: return "Unknown error during launch; smack Arisotura.";
    }
}


void MainWindow::onOpenFile()
{
    emuThread->emuPause();

    QString filename = QFileDialog::getOpenFileName(this,
                                                    "Open ROM",
                                                    Config::LastROMFolder,
                                                    "DS ROMs (*.nds *.dsi *.srl);;GBA ROMs (*.gba);;Any file (*.*)");
    if (filename.isEmpty())
    {
        emuThread->emuUnpause();
        return;
    }

    // TODO: validate the input file!!
    // * check that it is a proper ROM
    // * ensure the binary offsets are sane
    // * etc

    // this shit is stupid
    char file[1024];
    strncpy(file, filename.toStdString().c_str(), 1023); file[1023] = '\0';

    int pos = strlen(file)-1;
    while (file[pos] != '/' && file[pos] != '\\' && pos > 0) pos--;
    strncpy(Config::LastROMFolder, file, pos);
    Config::LastROMFolder[pos] = '\0';
    char* ext = &file[strlen(file)-3];

    int slot; int res;
    if (!strcasecmp(ext, "gba"))
    {
        slot = 1;
        res = Frontend::LoadROM(file, Frontend::ROMSlot_GBA);
    }
    else
    {
        slot = 0;
        res = Frontend::LoadROM(file, Frontend::ROMSlot_NDS);
    }

    if (res != Frontend::Load_OK)
    {
        QMessageBox::critical(this,
                              "melonDS",
                              loadErrorStr(res));
        emuThread->emuUnpause();
    }
    else if (slot == 1)
    {
        // checkme
        emuThread->emuUnpause();
    }
    else
    {
        emuThread->emuRun();
    }
}

void MainWindow::onBootFirmware()
{
    // TODO: check the whole GBA cart shito

    emuThread->emuPause();

    int res = Frontend::LoadBIOS();
    if (res != Frontend::Load_OK)
    {
        QMessageBox::critical(this,
                              "melonDS",
                              loadErrorStr(res));
        emuThread->emuUnpause();
    }
    else
    {
        emuThread->emuRun();
    }
}

void MainWindow::onSaveState()
{
    int slot = ((QAction*)sender())->data().toInt();

    emuThread->emuPause();

    char filename[1024];
    if (slot > 0)
    {
        Frontend::GetSavestateName(slot, filename, 1024);
    }
    else
    {
        // TODO: specific 'last directory' for savestate files?
        QString qfilename = QFileDialog::getSaveFileName(this,
                                                         "Save state",
                                                         Config::LastROMFolder,
                                                         "melonDS savestates (*.mln);;Any file (*.*)");
        if (qfilename.isEmpty())
        {
            emuThread->emuUnpause();
            return;
        }

        strncpy(filename, qfilename.toStdString().c_str(), 1023); filename[1023] = '\0';
    }

    if (Frontend::SaveState(filename))
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

    char filename[1024];
    if (slot > 0)
    {
        Frontend::GetSavestateName(slot, filename, 1024);
    }
    else
    {
        // TODO: specific 'last directory' for savestate files?
        QString qfilename = QFileDialog::getOpenFileName(this,
                                                         "Load state",
                                                         Config::LastROMFolder,
                                                         "melonDS savestates (*.ml*);;Any file (*.*)");
        if (qfilename.isEmpty())
        {
            emuThread->emuUnpause();
            return;
        }

        strncpy(filename, qfilename.toStdString().c_str(), 1023); filename[1023] = '\0';
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

    if (Frontend::LoadState(filename))
    {
        char msg[64];
        if (slot > 0) sprintf(msg, "State loaded from slot %d", slot);
        else          sprintf(msg, "State loaded from file");
        OSD::AddMessage(0, msg);
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
    Frontend::UndoStateLoad();
    emuThread->emuUnpause();

    OSD::AddMessage(0, "State load undone");
}

void MainWindow::onImportSavefile()
{
    if (!RunningSomething) return;

    emuThread->emuPause();
    QString path = QFileDialog::getOpenFileName(this,
                                            "Select savefile",
                                            Config::LastROMFolder,
                                            "Savefiles (*.sav *.bin *.dsv);;Any file (*.*)");

    if (!path.isEmpty())
    {
        if (QMessageBox::warning(this,
                        "Emulation will be reset and data overwritten",
                        "The emulation will be reset and the current savefile overwritten.",
                        QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok)
        {
            int res = Frontend::Reset();
            if (res != Frontend::Load_OK)
            {
                QMessageBox::critical(this, "melonDS", "Reset failed\n" + loadErrorStr(res));
            }
            else
            {
                int diff = Frontend::ImportSRAM(path.toStdString().c_str());
                if (diff > 0)
                    OSD::AddMessage(0, "Trimmed savefile");
                else if (diff < 0)
                    OSD::AddMessage(0, "Savefile shorter than SRAM");
            }
        }
    }
    emuThread->emuUnpause();
}

void MainWindow::onQuit()
{
    QApplication::quit();
}


void MainWindow::onPause(bool checked)
{
    if (!RunningSomething) return;

    if (checked)
    {
        emuThread->emuPause();
        OSD::AddMessage(0, "Paused");
    }
    else
    {
        emuThread->emuUnpause();
        OSD::AddMessage(0, "Resumed");
    }
}

void MainWindow::onReset()
{
    if (!RunningSomething) return;

    emuThread->emuPause();

    actUndoStateLoad->setEnabled(false);

    int res = Frontend::Reset();
    if (res != Frontend::Load_OK)
    {
        QMessageBox::critical(this,
                              "melonDS",
                              loadErrorStr(res));
        emuThread->emuUnpause();
    }
    else
    {
        OSD::AddMessage(0, "Reset");
        emuThread->emuRun();
    }
}

void MainWindow::onStop()
{
    if (!RunningSomething) return;

    emuThread->emuPause();
    NDS::Stop();
}

void MainWindow::onEnableCheats(bool checked)
{
    Config::EnableCheats = checked?1:0;
    Frontend::EnableCheats(Config::EnableCheats != 0);
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


void MainWindow::onOpenEmuSettings()
{
    emuThread->emuPause();

    EmuSettingsDialog* dlg = EmuSettingsDialog::openDlg(this);
    connect(dlg, &EmuSettingsDialog::finished, this, &MainWindow::onEmuSettingsDialogFinished);
}

void MainWindow::onEmuSettingsDialogFinished(int res)
{
    emuThread->emuUnpause();

    if (EmuSettingsDialog::needsReset)
        onReset();
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

void MainWindow::onOpenAudioSettings()
{
    AudioSettingsDialog* dlg = AudioSettingsDialog::openDlg(this);
    connect(dlg, &AudioSettingsDialog::finished, this, &MainWindow::onAudioSettingsFinished);
}

void MainWindow::onAudioSettingsFinished(int res)
{
    if (Config::MicInputType == 3)
    {
        micLoadWav(Config::MicWavPath);
        Frontend::Mic_SetExternalBuffer(micWavBuffer, micWavLength);
    }
    else
    {
        delete[] micWavBuffer;
        micWavBuffer = nullptr;

        if (Config::MicInputType == 1)
            Frontend::Mic_SetExternalBuffer(micExtBuffer, sizeof(micExtBuffer)/sizeof(s16));
        else
            Frontend::Mic_SetExternalBuffer(NULL, 0);
    }
}

void MainWindow::onOpenWifiSettings()
{
    emuThread->emuPause();

    WifiSettingsDialog* dlg = WifiSettingsDialog::openDlg(this);
    connect(dlg, &WifiSettingsDialog::finished, this, &MainWindow::onWifiSettingsFinished);
}

void MainWindow::onWifiSettingsFinished(int res)
{
    if (Wifi::MPInited)
    {
        Platform::MP_DeInit();
        Platform::MP_Init();
    }

    Platform::LAN_DeInit();
    Platform::LAN_Init();

    if (WifiSettingsDialog::needsReset)
        onReset();

    emuThread->emuUnpause();
}

void MainWindow::onChangeSavestateSRAMReloc(bool checked)
{
    Config::SavestateRelocSRAM = checked?1:0;
}

void MainWindow::onChangeScreenSize()
{
    int factor = ((QAction*)sender())->data().toInt();

    bool isHori = (Config::ScreenRotation == 1 || Config::ScreenRotation == 3);
    int gap = Config::ScreenGap;

    int w = 256*factor;
    int h = 192*factor;

    QSize diff = size() - panel->size();

    if (Config::ScreenLayout == 0) // natural
    {
        if (isHori)
            resize(QSize(h+gap+h, w) + diff);
        else
            resize(QSize(w, h+gap+h) + diff);
    }
    else if (Config::ScreenLayout == 1) // vertical
    {
        if (isHori)
            resize(QSize(h, w+gap+w) + diff);
        else
            resize(QSize(w, h+gap+h) + diff);
    }
    else // horizontal
    {
        if (isHori)
            resize(QSize(h+gap+h, w) + diff);
        else
            resize(QSize(w+gap+w, h) + diff);
    }
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

void MainWindow::onChangeScreenSizing(QAction* act)
{
    int sizing = act->data().toInt();
    Config::ScreenSizing = sizing;

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

void MainWindow::onFullscreenToggled()
{
    if (!mainWindow->isFullScreen()) 
    {
        mainWindow->showFullScreen(); 
        mainWindow->menuBar()->hide();
    }
    else
    {
        mainWindow->showNormal();
        mainWindow->menuBar()->show();
    }
}

void MainWindow::onEmuStart()
{
    // TODO: make savestates work in DSi mode!!
    if (Config::ConsoleType == 1)
    {
        for (int i = 0; i < 9; i++)
        {
            actSaveState[i]->setEnabled(false);
            actLoadState[i]->setEnabled(false);
        }
        actUndoStateLoad->setEnabled(false);
    }
    else
    {
        for (int i = 1; i < 9; i++)
        {
            actSaveState[i]->setEnabled(true);
            actLoadState[i]->setEnabled(Frontend::SavestateExists(i));
        }
        actSaveState[0]->setEnabled(true);
        actLoadState[0]->setEnabled(true);
        actUndoStateLoad->setEnabled(false);
    }

    actPause->setEnabled(true);
    actPause->setChecked(false);
    actReset->setEnabled(true);
    actStop->setEnabled(true);
    actImportSavefile->setEnabled(true);

    actSetupCheats->setEnabled(true);
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
    actImportSavefile->setEnabled(false);

    actPause->setEnabled(false);
    actReset->setEnabled(false);
    actStop->setEnabled(false);

    actSetupCheats->setEnabled(false);
}

void MainWindow::onUpdateVideoSettings(bool glchange)
{
    if (glchange)
    {
        emuThread->emuPause();

        if (hasOGL) emuThread->deinitOpenGL();
        delete panel;
        createScreenPanel();
        connect(emuThread, SIGNAL(windowUpdate()), panel, SLOT(update()));
        if (hasOGL) emuThread->initOpenGL();
    }

    videoSettingsDirty = true;

    if (glchange)
        emuThread->emuUnpause();
}


void emuStop()
{
    RunningSomething = false;

    Frontend::UnloadROM(Frontend::ROMSlot_NDS);
    Frontend::UnloadROM(Frontend::ROMSlot_GBA);

    emit emuThread->windowEmuStop();

    OSD::AddMessage(0xFFC040, "Shutdown");
}



int main(int argc, char** argv)
{
    srand(time(NULL));

    printf("melonDS " MELONDS_VERSION "\n");
    printf(MELONDS_URL "\n");

    Platform::Init(argc, argv);

    QApplication melon(argc, argv);
    melon.setWindowIcon(QIcon(":/melon-icon"));

    // http://stackoverflow.com/questions/14543333/joystick-wont-work-using-sdl
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_HAPTIC) < 0)
    {
        printf("SDL couldn't init rumble\n");
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
    {
        QMessageBox::critical(NULL, "melonDS", "SDL shat itself :(");
        return 1;
    }

    SDL_JoystickEventState(SDL_ENABLE);

    Config::Load();

#define SANITIZE(var, min, max)  { if (var < min) var = min; else if (var > max) var = max; }
    SANITIZE(Config::ConsoleType, 0, 1);
    SANITIZE(Config::_3DRenderer, 0, 1);
    SANITIZE(Config::ScreenVSyncInterval, 1, 20);
    SANITIZE(Config::GL_ScaleFactor, 1, 16);
    SANITIZE(Config::AudioVolume, 0, 256);
    SANITIZE(Config::MicInputType, 0, 3);
    SANITIZE(Config::ScreenRotation, 0, 3);
    SANITIZE(Config::ScreenGap, 0, 500);
    SANITIZE(Config::ScreenLayout, 0, 2);
    SANITIZE(Config::ScreenSizing, 0, 3);
#undef SANITIZE

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(format);

    audioSync = SDL_CreateCond();
    audioSyncLock = SDL_CreateMutex();

    audioFreq = 48000; // TODO: make configurable?
    SDL_AudioSpec whatIwant, whatIget;
    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = audioFreq;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 2;
    whatIwant.samples = 1024;
    whatIwant.callback = audioCallback;
    audioDevice = SDL_OpenAudioDevice(NULL, 0, &whatIwant, &whatIget, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (!audioDevice)
    {
        printf("Audio init failed: %s\n", SDL_GetError());
    }
    else
    {
        audioFreq = whatIget.freq;
        printf("Audio output frequency: %d Hz\n", audioFreq);
        SDL_PauseAudioDevice(audioDevice, 1);
    }

    memset(&whatIwant, 0, sizeof(SDL_AudioSpec));
    whatIwant.freq = 44100;
    whatIwant.format = AUDIO_S16LSB;
    whatIwant.channels = 1;
    whatIwant.samples = 1024;
    whatIwant.callback = micCallback;
    micDevice = SDL_OpenAudioDevice(NULL, 1, &whatIwant, &whatIget, 0);
    if (!micDevice)
    {
        printf("Mic init failed: %s\n", SDL_GetError());
    }
    else
    {
        SDL_PauseAudioDevice(micDevice, 1);
    }


    memset(micExtBuffer, 0, sizeof(micExtBuffer));
    micExtBufferWritePos = 0;
    micWavBuffer = nullptr;

    Frontend::Init_ROM();
    Frontend::EnableCheats(Config::EnableCheats != 0);

    Frontend::Init_Audio(audioFreq);

    if (Config::MicInputType == 1)
    {
        Frontend::Mic_SetExternalBuffer(micExtBuffer, sizeof(micExtBuffer)/sizeof(s16));
    }
    else if (Config::MicInputType == 3)
    {
        micLoadWav(Config::MicWavPath);
        Frontend::Mic_SetExternalBuffer(micWavBuffer, micWavLength);
    }

    Input::JoystickID = Config::JoystickID;
    Input::OpenJoystick();

    mainWindow = new MainWindow();

    emuThread = new EmuThread();
    emuThread->start();
    emuThread->emuPause();

    if (argc > 1)
    {
        char* file = argv[1];
        char* ext = &file[strlen(file)-3];

        if (!strcasecmp(ext, "nds") || !strcasecmp(ext, "srl") || !strcasecmp(ext, "dsi"))
        {
            int res = Frontend::LoadROM(file, Frontend::ROMSlot_NDS);

            if (res == Frontend::Load_OK)
            {
                if (argc > 2)
                {
                    file = argv[2];
                    ext = &file[strlen(file)-3];

                    if (!strcasecmp(ext, "gba"))
                    {
                        Frontend::LoadROM(file, Frontend::ROMSlot_GBA);
                    }
                }

                emuThread->emuRun();
            }
        }
    }

    int ret = melon.exec();

    emuThread->emuStop();
    emuThread->wait();
    delete emuThread;

    Input::CloseJoystick();

    Frontend::DeInit_ROM();

    if (audioDevice) SDL_CloseAudioDevice(audioDevice);
    if (micDevice)   SDL_CloseAudioDevice(micDevice);

    SDL_DestroyCond(audioSync);
    SDL_DestroyMutex(audioSyncLock);

    if (micWavBuffer) delete[] micWavBuffer;

    Config::Save();

    SDL_Quit();
    Platform::DeInit();
    return ret;
}

#ifdef __WIN32__

#include <windows.h>

int CALLBACK WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int cmdshow)
{
    int argc = 0;
    wchar_t** argv_w = CommandLineToArgvW(GetCommandLineW(), &argc);
    char* nullarg = "";

    char** argv = new char*[argc];
    for (int i = 0; i < argc; i++)
    {
        if (!argv_w) { argv[i] = nullarg; continue; }
        int len = WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, NULL, 0, NULL, NULL);
        if (len < 1) { argv[i] = nullarg; continue; }
        argv[i] = new char[len];
        int res = WideCharToMultiByte(CP_UTF8, 0, argv_w[i], -1, argv[i], len, NULL, NULL);
        if (res != len) { delete[] argv[i]; argv[i] = nullarg; }
    }

    if (argv_w) LocalFree(argv_w);

    /*if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        printf("\n");
    }*/

    int ret = main(argc, argv);

    printf("\n\n>");

    for (int i = 0; i < argc; i++) if (argv[i] != nullarg) delete[] argv[i];
    delete[] argv;

    return ret;
}

#endif
