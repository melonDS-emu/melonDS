/*
    Copyright 2016-2024 melonDS team

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
#include <QStandardPaths>
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
#include "CheatsDialog.h"
#include "DateTimeDialog.h"
#include "EmuSettingsDialog.h"
#include "InputConfig/InputConfigDialog.h"
#include "VideoSettingsDialog.h"
#include "ROMInfoDialog.h"
#include "RAMInfoDialog.h"
#include "PowerManagement/PowerManagementDialog.h"

#include "version.h"

#include "Config.h"
#include "DSi.h"

#include "EmuInstance.h"
#include "ArchiveUtil.h"
#include "CameraManager.h"
#include "LocalMP.h"
#include "Net.h"

#include "CLI.h"

#ifdef __WIN32__
#include <windows.h>
#endif

using namespace melonDS;

QString* systemThemeName;



QString emuDirectory;

const int kMaxEmuInstances = 16;
EmuInstance* emuInstances[kMaxEmuInstances];

CameraManager* camManager[2];
bool camStarted[2];


bool createEmuInstance()
{
    int id = -1;
    for (int i = 0; i < kMaxEmuInstances; i++)
    {
        if (!emuInstances[i])
        {
            id = i;
            break;
        }
    }

    if (id == -1)
        return false;

    auto inst = new EmuInstance(id);
    emuInstances[id] = inst;

    return true;
}

void deleteEmuInstance(int id)
{
    auto inst = emuInstances[id];
    if (!inst) return;

    delete inst;
    emuInstances[id] = nullptr;
}

void deleteAllEmuInstances()
{
    for (int i = 0; i < kMaxEmuInstances; i++)
        deleteEmuInstance(i);
}


void pathInit()
{
    // First, check for the portable directory next to the executable.
    QString appdirpath = QCoreApplication::applicationDirPath();
    QString portablepath = appdirpath + QDir::separator() + "portable";

#if defined(__APPLE__)
    // On Apple platforms we may need to navigate outside an app bundle.
    // The executable directory would be "melonDS.app/Contents/MacOS", so we need to go a total of three steps up.
    QDir bundledir(appdirpath);
    if (bundledir.cd("..") && bundledir.cd("..") && bundledir.dirName().endsWith(".app") && bundledir.cd(".."))
    {
        portablepath = bundledir.absolutePath() + QDir::separator() + "portable";
    }
#endif

    QDir portabledir(portablepath);
    if (portabledir.exists())
    {
        emuDirectory = portabledir.absolutePath();
    }
    else
    {
        // If no overrides are specified, use the default path.
#if defined(__WIN32__) && defined(WIN32_PORTABLE)
        emuDirectory = appdirpath;
#else
        QString confdir;
        QDir config(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));
        config.mkdir("melonDS");
        confdir = config.absolutePath() + QDir::separator() + "melonDS";
        emuDirectory = confdir;
#endif
    }
}



MelonApplication::MelonApplication(int& argc, char** argv)
    : QApplication(argc, argv)
{
#if !defined(Q_OS_APPLE)
    setWindowIcon(QIcon(":/melon-icon"));
    #if defined(Q_OS_UNIX)
        setDesktopFileName(QString("net.kuribo64.melonDS"));
    #endif
#endif
}

// TODO: ROM loading should be moved to EmuInstance
// especially so the preloading below and in main() can be done in a nicer fashion

bool MelonApplication::event(QEvent *event)
{
    if (event->type() == QEvent::FileOpen)
    {
        EmuInstance* inst = emuInstances[0];
        MainWindow* win = inst->getMainWindow();
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent*>(event);

        inst->getEmuThread()->emuPause();
        const QStringList file = win->splitArchivePath(openEvent->file(), true);
        if (!win->preloadROMs(file, {}, true))
            inst->getEmuThread()->emuUnpause();
    }

    return QApplication::event(event);
}

int main(int argc, char** argv)
{
    srand(time(nullptr));

    for (int i = 0; i < kMaxEmuInstances; i++)
        emuInstances[i] = nullptr;

    qputenv("QT_SCALE_FACTOR", "1");

#if QT_VERSION_MAJOR == 6 && defined(__WIN32__)
    // Allow using the system dark theme palette on Windows
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=2");
#endif

    MelonApplication melon(argc, argv);
    pathInit();

    CLI::CommandLineOptions* options = CLI::ManageArgs(melon);

#ifdef __WIN32__
    if (options->consoleOnWindows)
    {
        if (AllocConsole())
            freopen("CONOUT$", "w", stdout);
        else
            printf("Failed to allocate console\n");
    }
#endif

    printf("melonDS " MELONDS_VERSION "\n");
    printf(MELONDS_URL "\n");

    // easter egg - not worth checking other cases for something so dumb
    if (argc != 0 && (!strcasecmp(argv[0], "derpDS") || !strcasecmp(argv[0], "./derpDS")))
        printf("did you just call me a derp???\n");

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

        QMessageBox::critical(nullptr, "melonDS", errorStr);
        return 1;
    }

    SDL_JoystickEventState(SDL_ENABLE);

    SDL_InitSubSystem(SDL_INIT_VIDEO);
    SDL_EnableScreenSaver(); SDL_DisableScreenSaver();

    if (!Config::Load())
        QMessageBox::critical(nullptr,
                              "melonDS",
                              "Unable to write to config.\nPlease check the write permissions of the folder you placed melonDS in.");

    camStarted[0] = false;
    camStarted[1] = false;
    camManager[0] = new CameraManager(0, 640, 480, true);
    camManager[1] = new CameraManager(1, 640, 480, true);

    systemThemeName = new QString(QApplication::style()->objectName());

    {
        Config::Table cfg = Config::GetGlobalTable();
        QString uitheme = cfg.GetQString("UITheme");
        if (!uitheme.isEmpty())
        {
            QApplication::setStyle(uitheme);
        }
    }

    LocalMP::Init();
    {
        Config::Table cfg = Config::GetGlobalTable();
        bool direct = cfg.GetBool("LAN.DirectMode");
        std::string devicename = cfg.GetString("LAN.Device");
        Net::Init(direct, devicename.c_str());
    }

    createEmuInstance();

    {
        MainWindow* win = emuInstances[0]->getMainWindow();
        bool memberSyntaxUsed = false;
        const auto prepareRomPath = [&](const std::optional<QString> &romPath,
                                        const std::optional<QString> &romArchivePath) -> QStringList
        {
            if (!romPath.has_value())
                return {};

            if (romArchivePath.has_value())
                return {*romPath, *romArchivePath};

            const QStringList path = win->splitArchivePath(*romPath, true);
            if (path.size() > 1) memberSyntaxUsed = true;
            return path;
        };

        const QStringList dsfile = prepareRomPath(options->dsRomPath, options->dsRomArchivePath);
        const QStringList gbafile = prepareRomPath(options->gbaRomPath, options->gbaRomArchivePath);

        if (memberSyntaxUsed) printf("Warning: use the a.zip|b.nds format at your own risk!\n");

        win->preloadROMs(dsfile, gbafile, options->boot);
    }

    int ret = melon.exec();

    delete options;

    // if we get here, all the existing emu instances should have been deleted already
    // but with this we make extra sure they are all deleted
    deleteAllEmuInstances();

    LocalMP::DeInit();
    Net::DeInit();

    delete camManager[0];
    delete camManager[1];

    Config::Save();

    SDL_Quit();
    return ret;
}

#ifdef __WIN32__

int CALLBACK WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int cmdshow)
{
    int ret = main(__argc, __argv);

    printf("\n\n>");

    return ret;
}

#endif
