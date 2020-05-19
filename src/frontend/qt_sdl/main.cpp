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

#include <SDL2/SDL.h>

#include "main.h"
#include "Input.h"
#include "EmuSettingsDialog.h"
#include "InputConfigDialog.h"

#include "types.h"
#include "version.h"

#include "FrontendUtil.h"

#include "NDS.h"
#include "GPU.h"
#include "SPU.h"
#include "Wifi.h"
#include "Platform.h"
#include "Config.h"
#include "PlatformConfig.h"

#include "Savestate.h"


// TODO: uniform variable spelling

char* EmuDirectory;

bool RunningSomething;

MainWindow* mainWindow;
EmuThread* emuThread;

SDL_AudioDeviceID audioDevice;
int audioFreq;
SDL_cond* audioSync;
SDL_mutex* audioSyncLock;


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
        if (last < 0) last = 0;

        for (int i = num_in; i < len_in-margin; i++)
            ((u32*)buf_in)[i] = ((u32*)buf_in)[last];

        num_in = len_in-margin;
    }

    Frontend::AudioOut_Resample(buf_in, num_in, (s16*)stream, len);
}


EmuThread::EmuThread(QObject* parent) : QThread(parent)
{
    EmuStatus = 0;
    EmuRunning = 2;
    RunningSomething = false;

    connect(this, SIGNAL(windowTitleChange(QString)), mainWindow, SLOT(onTitleUpdate(QString)));
    connect(this, SIGNAL(windowEmuStart()), mainWindow, SLOT(onEmuStart()));
    connect(this, SIGNAL(windowEmuStop()), mainWindow, SLOT(onEmuStop()));

    emit windowEmuStop();
}

void EmuThread::run()
{
    NDS::Init();

    /*MainScreenPos[0] = 0;
    MainScreenPos[1] = 0;
    MainScreenPos[2] = 0;
    AutoScreenSizing = 0;*/

    /*if (Screen_UseGL)
    {
        uiGLMakeContextCurrent(GLContext);
        GPU3D::InitRenderer(true);
        uiGLMakeContextCurrent(NULL);
    }
    else*/
    {
        GPU3D::InitRenderer(false);
    }

    /*Touching = false;
    LidStatus = false;*/

    u32 nframes = 0;
    u32 starttick = SDL_GetTicks();
    u32 lasttick = starttick;
    u32 lastmeasuretick = lasttick;
    u32 fpslimitcount = 0;

    char melontitle[100];

    while (EmuRunning != 0)
    {
        Input::Process();
        /*ProcessInput();

        if (HotkeyPressed(HK_FastForwardToggle))
        {
            Config::LimitFPS = !Config::LimitFPS;
            uiQueueMain(UpdateFPSLimit, NULL);
        }
        // TODO: similar hotkeys for video/audio sync?

        if (HotkeyPressed(HK_Pause)) uiQueueMain(TogglePause, NULL);
        if (HotkeyPressed(HK_Reset)) uiQueueMain(Reset, NULL);

        if (GBACart::CartInserted && GBACart::HasSolarSensor)
        {
            if (HotkeyPressed(HK_SolarSensorDecrease))
            {
                if (GBACart_SolarSensor::LightLevel > 0) GBACart_SolarSensor::LightLevel--;
                char msg[64];
                sprintf(msg, "Solar sensor level set to %d", GBACart_SolarSensor::LightLevel);
                OSD::AddMessage(0, msg);
            }
            if (HotkeyPressed(HK_SolarSensorIncrease))
            {
                if (GBACart_SolarSensor::LightLevel < 10) GBACart_SolarSensor::LightLevel++;
                char msg[64];
                sprintf(msg, "Solar sensor level set to %d", GBACart_SolarSensor::LightLevel);
                OSD::AddMessage(0, msg);
            }
        }*/

        if (EmuRunning == 1)
        {
            EmuStatus = 1;

            // process input and hotkeys
            NDS::SetKeyMask(0xFFF);
            /*NDS::SetKeyMask(KeyInputMask & JoyInputMask);

            if (HotkeyPressed(HK_Lid))
            {
                LidStatus = !LidStatus;
                NDS::SetLidClosed(LidStatus);
                OSD::AddMessage(0, LidStatus ? "Lid closed" : "Lid opened");
            }*/

            // microphone input
            /*FeedMicInput();

            if (Screen_UseGL)
            {
                uiGLBegin(GLContext);
                uiGLMakeContextCurrent(GLContext);
            }*/

            // auto screen layout
            /*{
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
                    SetupScreenRects(WindowWidth, WindowHeight);
                }
            }*/

            // emulate
            u32 nlines = NDS::RunFrame();

#ifdef MELONCAP
            MelonCap::Update();
#endif // MELONCAP

            if (EmuRunning == 0) break;

            /*if (Screen_UseGL)
            {
                GLScreen_DrawScreen();
                uiGLEnd(GLContext);
            }
            uiAreaQueueRedrawAll(MainDrawArea);*/
            mainWindow->update();

            bool fastforward = false;
            //bool fastforward = HotkeyDown(HK_FastForward);

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

            if (EmuRunning == 2)
            {
                /*if (Screen_UseGL)
                {
                    uiGLBegin(GLContext);
                    uiGLMakeContextCurrent(GLContext);
                    GLScreen_DrawScreen();
                    uiGLEnd(GLContext);
                }
                uiAreaQueueRedrawAll(MainDrawArea);*/
                mainWindow->update();
            }

            //if (Screen_UseGL) uiGLMakeContextCurrent(NULL);

            EmuStatus = EmuRunning;

            sprintf(melontitle, "melonDS " MELONDS_VERSION);
            changeWindowTitle(melontitle);

            SDL_Delay(100);
        }
    }

    EmuStatus = 0;

    //if (Screen_UseGL) uiGLMakeContextCurrent(GLContext);

    NDS::DeInit();
    //Platform::LAN_DeInit();

    /*if (Screen_UseGL)
    {
        OSD::DeInit(true);
        GLScreen_DeInit();
    }
    else
        OSD::DeInit(false);*/

    //if (Screen_UseGL) uiGLMakeContextCurrent(NULL);
}

void EmuThread::changeWindowTitle(char* title)
{
    emit windowTitleChange(QString(title));
}

void EmuThread::emuRun()
{
    EmuRunning = 1;
    RunningSomething = true;

    // checkme
    emit windowEmuStart();
    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 0);
}

void EmuThread::emuPause(bool refresh)
{
    int status = refresh ? 2:3;
    PrevEmuStatus = EmuRunning;
    EmuRunning = status;
    while (EmuStatus != status);

    //emit windowEmuPause();
    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 1);
}

void EmuThread::emuUnpause()
{
    EmuRunning = PrevEmuStatus;

    //emit windowEmuUnpause();
    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 0);
}

void EmuThread::emuStop()
{
    EmuRunning = 0;

    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 1);
}

bool EmuThread::emuIsRunning()
{
    return (EmuRunning == 1);
}


MainWindowPanel::MainWindowPanel(QWidget* parent) : QWidget(parent)
{
    screen[0] = new QImage(256, 192, QImage::Format_RGB32);
    screen[1] = new QImage(256, 192, QImage::Format_RGB32);
}

MainWindowPanel::~MainWindowPanel()
{
    delete screen[0];
    delete screen[1];
}

void MainWindowPanel::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    // fill background
    painter.fillRect(event->rect(), QColor::fromRgb(0, 0, 0));

    int frontbuf = GPU::FrontBuffer;
    if (!GPU::Framebuffer[frontbuf][0] || !GPU::Framebuffer[frontbuf][1]) return;

    memcpy(screen[0]->scanLine(0), GPU::Framebuffer[frontbuf][0], 256*192*4);
    memcpy(screen[1]->scanLine(0), GPU::Framebuffer[frontbuf][1], 256*192*4);

    QRect src = QRect(0, 0, 256, 192);

    QRect dstTop = QRect(0, 0, 256, 192); // TODO
    QRect dstBot = QRect(0, 192, 256, 192); // TODO

    painter.drawImage(dstTop, *screen[0]);
    painter.drawImage(dstBot, *screen[1]);
}


void MainWindowPanel::mousePressEvent(QMouseEvent* event)
{
    event->accept();

    printf("mouse press %d,%d\n", event->pos().x(), event->pos().y());
}

void MainWindowPanel::mouseReleaseEvent(QMouseEvent* event)
{
    event->accept();

    printf("mouse release %d,%d\n", event->pos().x(), event->pos().y());
}

void MainWindowPanel::mouseMoveEvent(QMouseEvent* event)
{
    event->accept();

    printf("mouse move %d,%d  %08X\n", event->pos().x(), event->pos().y(), event->buttons());
}


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("melonDS " MELONDS_VERSION);

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
                char title[16];
                sprintf(title, "%d", i);
                actSaveState[i] = submenu->addAction(title);
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
                char title[16];
                sprintf(title, "%d", i);
                actLoadState[i] = submenu->addAction(title);
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
    }
    {
        QMenu* menu = menubar->addMenu("Config");

        actEmuSettings = menu->addAction("Emu settings");
        connect(actEmuSettings, &QAction::triggered, this, &MainWindow::onOpenEmuSettings);

        actInputConfig = menu->addAction("Input and hotkeys");
        connect(actInputConfig, &QAction::triggered, this, &MainWindow::onOpenInputConfig);
    }
    setMenuBar(menubar);

    panel = new MainWindowPanel(this);
    setCentralWidget(panel);
    panel->setMinimumSize(256, 384);
}

MainWindow::~MainWindow()
{
}


void MainWindow::keyPressEvent(QKeyEvent* event)
{
    printf("key press. %d %d %08X %08X\n", event->key(), event->nativeScanCode(), event->modifiers(), event->nativeModifiers());
}


QString MainWindow::loadErrorStr(int error)
{
    switch (error)
    {
    case Frontend::Load_BIOS9Missing: return "DS ARM9 BIOS was not found or could not be accessed.";
    case Frontend::Load_BIOS9Bad:     return "DS ARM9 BIOS is not a valid BIOS dump.";

    case Frontend::Load_BIOS7Missing: return "DS ARM7 BIOS was not found or could not be accessed.";
    case Frontend::Load_BIOS7Bad:     return "DS ARM7 BIOS is not a valid BIOS dump.";

    case Frontend::Load_FirmwareMissing:     return "DS firmware was not found or could not be accessed.";
    case Frontend::Load_FirmwareBad:         return "DS firmware is not a valid firmware dump.";
    case Frontend::Load_FirmwareNotBootable: return "DS firmware is not bootable.";

    case Frontend::Load_ROMLoadError: return "Failed to load the ROM. Make sure the file is accessible and isn't used by another application.";

    default: return "Unknown error during launch; smack Arisotura.";
    }
}


void MainWindow::onOpenFile()
{
    emuThread->emuPause(true);

    QString filename = QFileDialog::getOpenFileName(this,
                                                    "Open ROM",
                                                    Config::LastROMFolder,
                                                    "DS ROMs (*.nds *.srl);;GBA ROMs (*.gba);;Any file (*.*)");
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
    // TODO: ensure the firmware is actually bootable!!
    // TODO: check the whole GBA cart shito

    emuThread->emuPause(true);

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

    emuThread->emuPause(true);

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
        // TODO: OSD message

        actLoadState[slot]->setEnabled(true);
    }
    else
    {
        // TODO: OSD error message?
    }

    emuThread->emuUnpause();
}

void MainWindow::onLoadState()
{
    int slot = ((QAction*)sender())->data().toInt();

    emuThread->emuPause(true);

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
        /*char msg[64];
        if (slot > 0) sprintf(msg, "State slot %d is empty", slot);
        else          sprintf(msg, "State file does not exist");
        OSD::AddMessage(0xFFA0A0, msg);*/

        emuThread->emuUnpause();
        return;
    }

    if (Frontend::LoadState(filename))
    {
        // TODO: OSD message
    }
    else
    {
        // TODO: OSD error message?
    }

    emuThread->emuUnpause();
}

void MainWindow::onUndoStateLoad()
{
    emuThread->emuPause(true);
    Frontend::UndoStateLoad();
    emuThread->emuUnpause();
}

void MainWindow::onQuit()
{
    QApplication::quit();
}


void MainWindow::onPause(bool checked)
{
    if (emuThread->emuIsRunning())
    {
        emuThread->emuPause(true);
    }
    else
    {
        emuThread->emuUnpause();
    }
}

void MainWindow::onReset()
{
    //
}

void MainWindow::onStop()
{
    //
}


void MainWindow::onTitleUpdate(QString title)
{
    setWindowTitle(title);
}

void MainWindow::onEmuStart()
{
    for (int i = 1; i < 9; i++)
    {
        actSaveState[i]->setEnabled(true);
        actLoadState[i]->setEnabled(Frontend::SavestateExists(i));
    }
    actSaveState[0]->setEnabled(true);
    actLoadState[0]->setEnabled(true);
    actUndoStateLoad->setEnabled(true);

    actPause->setEnabled(true);
    actPause->setChecked(false);
    actReset->setEnabled(true);
    actStop->setEnabled(true);
}

void MainWindow::onEmuStop()
{
    for (int i = 0; i < 9; i++)
    {
        actSaveState[i]->setEnabled(false);
        actLoadState[i]->setEnabled(false);
    }
    actUndoStateLoad->setEnabled(false);

    actPause->setEnabled(false);
    actReset->setEnabled(false);
    actStop->setEnabled(false);
}

void MainWindow::onEmuPause()
{
    //
}

void MainWindow::onEmuUnpause()
{
    //
}


void MainWindow::onOpenEmuSettings()
{
    EmuSettingsDialog::openDlg(this);
}

void MainWindow::onOpenInputConfig()
{
    emuThread->emuPause(true);

    InputConfigDialog* dlg = InputConfigDialog::openDlg(this);
    connect(dlg, &InputConfigDialog::finished, this, &MainWindow::onInputConfigFinished);
}

void MainWindow::onInputConfigFinished(int res)
{
    emuThread->emuUnpause();
}


// FIXME!!!!
#if (!defined(__WIN32__) && !defined(UNIX_PORTABLE))
#include <glib.h>
#endif

int main(int argc, char** argv)
{
    srand(time(NULL));

    printf("melonDS " MELONDS_VERSION "\n");
    printf(MELONDS_URL "\n");

#if defined(__WIN32__) || defined(UNIX_PORTABLE)
    if (argc > 0 && strlen(argv[0]) > 0)
    {
        int len = strlen(argv[0]);
        while (len > 0)
        {
            if (argv[0][len] == '/') break;
            if (argv[0][len] == '\\') break;
            len--;
        }
        if (len > 0)
        {
            EmuDirectory = new char[len+1];
            strncpy(EmuDirectory, argv[0], len);
            EmuDirectory[len] = '\0';
        }
        else
        {
            EmuDirectory = new char[2];
            strcpy(EmuDirectory, ".");
        }
    }
    else
    {
        EmuDirectory = new char[2];
        strcpy(EmuDirectory, ".");
    }
#else
	const char* confdir = g_get_user_config_dir();
	const char* confname = "/melonDS";
	int cdlen = strlen(confdir);
	int cnlen = strlen(confname);
	EmuDirectory = new char[cdlen + cnlen + 1];
	strncpy(&EmuDirectory[0], confdir, cdlen);
	strncpy(&EmuDirectory[cdlen], confname, cnlen);
	EmuDirectory[cdlen+cnlen] = '\0';
#endif

    QApplication melon(argc, argv);

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

    if      (Config::AudioVolume < 0)   Config::AudioVolume = 0;
    else if (Config::AudioVolume > 256) Config::AudioVolume = 256;

    // TODO: this should be checked before running anything
#if 0
    {
        const char* romlist_missing = "Save memory type detection will not work correctly.\n\n"
            "You should use the latest version of romlist.bin (provided in melonDS release packages).";
#if !defined(UNIX_PORTABLE) && !defined(__WIN32__)
        std::string missingstr = std::string(romlist_missing) +
            "\n\nThe ROM list should be placed in " + g_get_user_data_dir() + "/melonds/, otherwise "
            "melonDS will search for it in the current working directory.";
        const char* romlist_missing_text = missingstr.c_str();
#else
        const char* romlist_missing_text = romlist_missing;
#endif

        FILE* f = Platform::OpenDataFile("romlist.bin");
        if (f)
        {
            u32 data;
            fread(&data, 4, 1, f);
            fclose(f);

            if ((data >> 24) == 0) // old CRC-based list
            {
                uiMsgBoxError(NULL, "Your version of romlist.bin is outdated.", romlist_missing_text);
            }
        }
        else
        {
        	uiMsgBoxError(NULL, "romlist.bin not found.", romlist_missing_text);
        }
    }
#endif

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

    Frontend::Init_ROM();
    Frontend::Init_Audio(audioFreq);

    Input::JoystickID = Config::JoystickID;
    Input::OpenJoystick();

    mainWindow = new MainWindow();
    mainWindow->show();

    emuThread = new EmuThread();
    emuThread->start();
    emuThread->emuPause(true);

    #if 0
    if (argc > 1)
    {
        char* file = argv[1];
        char* ext = &file[strlen(file)-3];

        if (!strcasecmp(ext, "nds") || !strcasecmp(ext, "srl"))
        {
            strncpy(ROMPath[0], file, 1023);
            ROMPath[0][1023] = '\0';

            //SetupSRAMPath(0);

            //if (NDS::LoadROM(ROMPath[0], SRAMPath[0], Config::DirectBoot))
            //    Run();
        }

        if (argc > 2)
        {
            file = argv[2];
            ext = &file[strlen(file)-3];

            if (!strcasecmp(ext, "gba"))
            {
                strncpy(ROMPath[1], file, 1023);
                ROMPath[1][1023] = '\0';

                //SetupSRAMPath(1);

                //NDS::LoadGBAROM(ROMPath[1], SRAMPath[1]);
            }
        }
    }
    #endif

    int ret = melon.exec();

    emuThread->emuStop();
    emuThread->wait();
    delete emuThread;

    Input::CloseJoystick();

    if (audioDevice) SDL_CloseAudioDevice(audioDevice);
    //if (MicDevice)   SDL_CloseAudioDevice(MicDevice);

    SDL_DestroyCond(audioSync);
    SDL_DestroyMutex(audioSyncLock);

    //if (MicWavBuffer) delete[] MicWavBuffer;

    Config::Save();

    SDL_Quit();
    delete[] EmuDirectory;
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

    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        printf("\n");
    }

    int ret = main(argc, argv);

    printf("\n\n>");

    for (int i = 0; i < argc; i++) if (argv[i] != nullarg) delete[] argv[i];
    delete[] argv;

    return ret;
}

#endif
