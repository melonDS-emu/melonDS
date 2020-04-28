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

#include <SDL2/SDL.h>

#include "main.h"

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


char* EmuDirectory;

bool RunningSomething;

MainWindow* mainWindow;
EmuThread* emuThread;


EmuThread::EmuThread(QObject* parent) : QThread(parent)
{
    EmuStatus = 0;
    EmuRunning = 2;
    RunningSomething = false;

    connect(this, SIGNAL(windowTitleChange(QString)), mainWindow, SLOT(onTitleUpdate(QString)));
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
    KeyInputMask = 0xFFF;
    JoyInputMask = 0xFFF;
    KeyHotkeyMask = 0;
    JoyHotkeyMask = 0;
    HotkeyMask = 0;
    LastHotkeyMask = 0;
    LidStatus = false;*/

    u32 nframes = 0;
    u32 starttick = SDL_GetTicks();
    u32 lasttick = starttick;
    u32 lastmeasuretick = lasttick;
    u32 fpslimitcount = 0;
    u64 perfcount = SDL_GetPerformanceCounter();
    u64 perffreq = SDL_GetPerformanceFrequency();
    float samplesleft = 0;
    u32 nsamples = 0;

    char melontitle[100];

    while (EmuRunning != 0)
    {
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
            /*bool fastforward = HotkeyDown(HK_FastForward);

            if (Config::AudioSync && !fastforward)
            {
                SDL_LockMutex(AudioSyncLock);
                while (SPU::GetOutputSize() > 1024)
                {
                    int ret = SDL_CondWaitTimeout(AudioSync, AudioSyncLock, 500);
                    if (ret == SDL_MUTEX_TIMEDOUT) break;
                }
                SDL_UnlockMutex(AudioSyncLock);
            }*/

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
                        nsamples = 0;
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
}

void EmuThread::emuPause(bool refresh)
{
    int status = refresh ? 2:3;
    PrevEmuStatus = EmuRunning;
    EmuRunning = status;
    while (EmuStatus != status);
}

void EmuThread::emuUnpause()
{
    EmuRunning = PrevEmuStatus;
}

void EmuThread::emuStop()
{
    EmuRunning = 0;
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


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("melonDS " MELONDS_VERSION);

    QMenuBar* menubar = new QMenuBar();
    {
        QMenu* menu = menubar->addMenu("File");

        actOpenROM = menu->addAction("Open file...");
        connect(actOpenROM, &QAction::triggered, this, &MainWindow::onOpenFile);

        actBootFirmware = menu->addAction("Launch DS menu");
        connect(actBootFirmware, &QAction::triggered, this, &MainWindow::onBootFirmware);

        menu->addSeparator();

        actQuit = menu->addAction("Quit");
        connect(actQuit, &QAction::triggered, this, &MainWindow::onQuit);
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

    // this shit is stupid
    char file[1024];
    strncpy(file, filename.toStdString().c_str(), 1023); file[1023] = '\0';

    int pos = strlen(file)-1;
    while (file[pos] != '/' && file[pos] != '\\' && pos > 0) pos--;
    strncpy(Config::LastROMFolder, file, pos);
    Config::LastROMFolder[pos] = '\0';
    char* ext = &file[strlen(file)-3];

    int slot; bool res;
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

    if (!res)
    {
        QMessageBox::critical(this,
                              "melonDS",
                              "Failed to load the ROM.\n\nMake sure the file is accessible and isn't used by another application.");
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

    bool res = Frontend::LoadBIOS();
    if (!res)
    {
        // TODO!

        emuThread->emuUnpause();
    }
    else
    {
        emuThread->emuRun();
    }
}

void MainWindow::onQuit()
{
    QApplication::quit();
}


void MainWindow::onTitleUpdate(QString title)
{
    setWindowTitle(title);
}


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
	EmuDirectory = new char[strlen(confdir) + strlen(confname) + 1];
	strcat(EmuDirectory, confdir);
	strcat(EmuDirectory, confname);
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

    //if      (Config::AudioVolume < 0)   Config::AudioVolume = 0;
    //else if (Config::AudioVolume > 256) Config::AudioVolume = 256;

    // TODO: those should be checked before running anything
    // (as to let the user specify their own BIOS/firmware path etc)
#if 0
    if (!Platform::LocalFileExists("bios7.bin") ||
        !Platform::LocalFileExists("bios9.bin") ||
        !Platform::LocalFileExists("firmware.bin"))
    {
#if defined(__WIN32__) || defined(UNIX_PORTABLE)
		const char* locationName = "the directory you run melonDS from";
#else
		char* locationName = EmuDirectory;
#endif
		char msgboxtext[512];
		sprintf(msgboxtext,
            "One or more of the following required files don't exist or couldn't be accessed:\n\n"
            "bios7.bin -- ARM7 BIOS\n"
            "bios9.bin -- ARM9 BIOS\n"
            "firmware.bin -- firmware image\n\n"
            "Dump the files from your DS and place them in %s.\n"
            "Make sure that the files can be accessed.",
			locationName
		);

        uiMsgBoxError(NULL, "BIOS/Firmware not found", msgboxtext);

        uiUninit();
        SDL_Quit();
        return 0;
    }
    if (!Platform::LocalFileExists("firmware.bin.bak"))
    {
        // verify the firmware
        //
        // there are dumps of an old hacked firmware floating around on the internet
        // and those are problematic
        // the hack predates WFC, and, due to this, any game that alters the WFC
        // access point data will brick that firmware due to it having critical
        // data in the same area. it has the same problem on hardware.
        //
        // but this should help stop users from reporting that issue over and over
        // again, when the issue is not from melonDS but from their firmware dump.
        //
        // I don't know about all the firmware hacks in existence, but the one I
        // looked at has 0x180 bytes from the header repeated at 0x3FC80, but
        // bytes 0x0C-0x14 are different.

        FILE* f = Platform::OpenLocalFile("firmware.bin", "rb");
        u8 chk1[0x180], chk2[0x180];

        fseek(f, 0, SEEK_SET);
        fread(chk1, 1, 0x180, f);
        fseek(f, -0x380, SEEK_END);
        fread(chk2, 1, 0x180, f);

        memset(&chk1[0x0C], 0, 8);
        memset(&chk2[0x0C], 0, 8);

        fclose(f);

        if (!memcmp(chk1, chk2, 0x180))
        {
            uiMsgBoxError(NULL,
                          "Problematic firmware dump",
                          "You are using an old hacked firmware dump.\n"
                          "Firmware boot will stop working if you run any game that alters WFC settings.\n\n"
                          "Note that the issue is not from melonDS, it would also happen on an actual DS.");
        }
    }
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
