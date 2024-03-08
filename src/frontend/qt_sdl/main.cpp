/*
    Copyright 2016-2023 melonDS team

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
#include "DateTimeDialog.h"
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

#include "Args.h"
#include "NDS.h"
#include "NDSCart.h"
#include "GBACart.h"
#include "GPU.h"
#include "SPU.h"
#include "Wifi.h"
#include "Platform.h"
#include "LocalMP.h"
#include "Config.h"
#include "RTC.h"
#include "DSi.h"
#include "DSi_I2C.h"
#include "GPU3D_Soft.h"
#include "GPU3D_OpenGL.h"

#include "Savestate.h"

//#include "main_shaders.h"

#include "ROMManager.h"
#include "ArchiveUtil.h"
#include "CameraManager.h"

#include "CLI.h"

// TODO: uniform variable spelling
using namespace melonDS;
QString NdsRomMimeType = "application/x-nintendo-ds-rom";
QStringList NdsRomExtensions { ".nds", ".srl", ".dsi", ".ids" };

QString GbaRomMimeType = "application/x-gba-rom";
QStringList GbaRomExtensions { ".gba", ".agb" };

QString* systemThemeName;

// This list of supported archive formats is based on libarchive(3) version 3.6.2 (2022-12-09).
QStringList ArchiveMimeTypes
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

QStringList ArchiveExtensions
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
bool videoSettingsDirty;

CameraManager* camManager[2];
bool camStarted[2];

//extern int AspectRatiosNum;






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





void emuStop()
{
    RunningSomething = false;

    emit emuThread->windowEmuStop();
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

#if QT_VERSION_MAJOR == 6 && defined(__WIN32__)
    // Allow using the system dark theme palette on Windows
    qputenv("QT_QPA_PLATFORM", "windows:darkmode=2");
#endif

    printf("melonDS " MELONDS_VERSION "\n");
    printf(MELONDS_URL "\n");

    // easter egg - not worth checking other cases for something so dumb
    if (argc != 0 && (!strcasecmp(argv[0], "derpDS") || !strcasecmp(argv[0], "./derpDS")))
        printf("did you just call me a derp???\n");

    MelonApplication melon(argc, argv);

    Platform::Init(argc, argv);

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

    if (!Config::Load()) QMessageBox::critical(NULL, "melonDS", "Unable to write to config.\nPlease check the write permissions of the folder you placed melonDS in.");

#define SANITIZE(var, min, max)  { var = std::clamp(var, min, max); }
    SANITIZE(Config::ConsoleType, 0, 1);
#ifdef OGLRENDERER_ENABLED
    SANITIZE(Config::_3DRenderer, 0, 1); // 0 is the software renderer, 1 is the OpenGL renderer
#else
    SANITIZE(Config::_3DRenderer, 0, 0);
#endif
    SANITIZE(Config::ScreenVSyncInterval, 1, 20);
    SANITIZE(Config::GL_ScaleFactor, 1, 16);
    SANITIZE(Config::AudioInterp, 0, 4);
    SANITIZE(Config::AudioVolume, 0, 256);
    SANITIZE(Config::MicInputType, 0, (int)micInputType_MAX);
    SANITIZE(Config::ScreenRotation, 0, (int)Frontend::screenRot_MAX);
    SANITIZE(Config::ScreenGap, 0, 500);
    SANITIZE(Config::ScreenLayout, 0, (int)Frontend::screenLayout_MAX);
    SANITIZE(Config::ScreenSizing, 0, (int)Frontend::screenSizing_MAX);
    SANITIZE(Config::ScreenAspectTop, 0, AspectRatiosNum);
    SANITIZE(Config::ScreenAspectBot, 0, AspectRatiosNum);
#undef SANITIZE

    camStarted[0] = false;
    camStarted[1] = false;
    camManager[0] = new CameraManager(0, 640, 480, true);
    camManager[1] = new CameraManager(1, 640, 480, true);
    camManager[0]->setXFlip(Config::Camera[0].XFlip);
    camManager[1]->setXFlip(Config::Camera[1].XFlip);

    systemThemeName = new QString(QApplication::style()->objectName());

    if (!Config::UITheme.empty())
    {
        QApplication::setStyle(QString::fromStdString(Config::UITheme));
    }

    Input::JoystickID = Config::JoystickID;
    Input::OpenJoystick();

    mainWindow = new MainWindow();
    if (options->fullscreen)
        ToggleFullscreen(mainWindow);

    emuThread = new EmuThread();
    emuThread->start();
    emuThread->emuPause();

    AudioInOut::Init(emuThread);
    ROMManager::EnableCheats(*emuThread->NDS, Config::EnableCheats != 0);
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

    delete options;

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
