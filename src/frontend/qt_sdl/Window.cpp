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
#include <QDesktopServices>
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

#include "main.h"
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

#include "Platform.h"
#include "Config.h"
#include "version.h"
#include "Savestate.h"
#include "MPInterface.h"
#include "LANDialog.h"

//#include "main_shaders.h"

#include "EmuInstance.h"
#include "ArchiveUtil.h"
#include "CameraManager.h"
#include "Window.h"

using namespace melonDS;




extern CameraManager* camManager[2];
extern bool camStarted[2];


QString NdsRomMimeType = "application/x-nintendo-ds-rom";
QStringList NdsRomExtensions { ".nds", ".srl", ".dsi", ".ids" };

QString GbaRomMimeType = "application/x-gba-rom";
QStringList GbaRomExtensions { ".gba", ".agb" };


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

// AAAAAAA
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


MainWindow::MainWindow(int id, EmuInstance* inst, QWidget* parent) :
    QMainWindow(parent),
    windowID(id),
    emuInstance(inst),
    globalCfg(inst->globalCfg),
    localCfg(inst->localCfg),
    windowCfg(localCfg.GetTable("Window"+std::to_string(id), "Window0")),
    emuThread(inst->getEmuThread())
{
#ifndef _WIN32
    if (!parent)
    {
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
    }
#endif

    showOSD = windowCfg.GetBool("ShowOSD");

    setWindowTitle("melonDS " MELONDS_VERSION);
    setAttribute(Qt::WA_DeleteOnClose);
    setAcceptDrops(true);
    setFocusPolicy(Qt::ClickFocus);

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
        Config::Array recentROMs = globalCfg.GetArray("RecentROM");
        int numrecent = std::min(kMaxRecentROMs, (int)recentROMs.Size());
        for (int i = 0; i < numrecent; ++i)
        {
            std::string item = recentROMs.GetString(i);
            if (!item.empty())
                recentFileList.push_back(QString::fromStdString(item));
        }
        updateRecentFilesMenu();

        //actBootFirmware = menu->addAction("Launch DS menu");
        actBootFirmware = menu->addAction("Boot firmware");
        connect(actBootFirmware, &QAction::triggered, this, &MainWindow::onBootFirmware);

        menu->addSeparator();

        actCurrentCart = menu->addAction("DS slot: " + emuInstance->cartLabel());
        actCurrentCart->setEnabled(false);

        actInsertCart = menu->addAction("Insert cart...");
        connect(actInsertCart, &QAction::triggered, this, &MainWindow::onInsertCart);

        actEjectCart = menu->addAction("Eject cart");
        connect(actEjectCart, &QAction::triggered, this, &MainWindow::onEjectCart);

        menu->addSeparator();

        actCurrentGBACart = menu->addAction("GBA slot: " + emuInstance->gbaCartLabel());
        actCurrentGBACart->setEnabled(false);

        actInsertGBACart = menu->addAction("Insert ROM cart...");
        connect(actInsertGBACart, &QAction::triggered, this, &MainWindow::onInsertGBACart);

        {
            QMenu* submenu = menu->addMenu("Insert add-on cart");

            actInsertGBAAddon[0] = submenu->addAction("Memory expansion");
            actInsertGBAAddon[0]->setData(QVariant(GBAAddon_RAMExpansion));
            connect(actInsertGBAAddon[0], &QAction::triggered, this, &MainWindow::onInsertGBAAddon);

            actInsertGBAAddon[1] = submenu->addAction("Rumble Pak");
            actInsertGBAAddon[1]->setData(QVariant(GBAAddon_RumblePak));
            connect(actInsertGBAAddon[1], &QAction::triggered, this, &MainWindow::onInsertGBAAddon);
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
        actOpenConfig = menu->addAction("Open melonDS directory");
        connect(actOpenConfig, &QAction::triggered, this, [&]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(emuDirectory));
        });

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

        actDateTime = menu->addAction("Date and time");
        connect(actDateTime, &QAction::triggered, this, &MainWindow::onOpenDateTime);

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

            submenu->addSeparator();

            actLANStartHost = submenu->addAction("Host LAN game");
            connect(actLANStartHost, &QAction::triggered, this, &MainWindow::onLANStartHost);

            actLANStartClient = submenu->addAction("Join LAN game");
            connect(actLANStartClient, &QAction::triggered, this, &MainWindow::onLANStartClient);

            /*submenu->addSeparator();

            actNPStartHost = submenu->addAction("NETPLAY HOST");
            connect(actNPStartHost, &QAction::triggered, this, &MainWindow::onNPStartHost);

            actNPStartClient = submenu->addAction("NETPLAY CLIENT");
            connect(actNPStartClient, &QAction::triggered, this, &MainWindow::onNPStartClient);

            actNPTest = submenu->addAction("NETPLAY GO");
            connect(actNPTest, &QAction::triggered, this, &MainWindow::onNPTest);*/
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

            for (int i = 0; i < screenRot_MAX; i++)
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

            for (int i = 0; i < screenLayout_MAX; i++)
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

            for (int i = 0; i < screenSizing_MAX; i++)
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

        if (parentWidget() != nullptr) // TEST
        {
            QMenu* menu = menubar->addMenu("Test");

            menu->addAction("Test");
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

    if (localCfg.GetString("Firmware.Username") == "Arisotura")
        actMPNewInstance->setText("Fart");

#ifdef Q_OS_MAC
    QPoint screenCenter = screen()->availableGeometry().center();
    QRect frameGeo = frameGeometry();
    frameGeo.moveCenter(screenCenter);
    move(frameGeo.topLeft());
#endif

    std::string geom = windowCfg.GetString("Geometry");
    if (!geom.empty())
    {
        QByteArray raw = QByteArray::fromStdString(geom);
        QByteArray dec = QByteArray::fromBase64(raw, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
        if (!dec.isEmpty())
            restoreGeometry(dec);
        // if the window was closed in fullscreen do not restore this
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    }
    show();

    createScreenPanel();

    actEjectCart->setEnabled(false);
    actEjectGBACart->setEnabled(false);

    if (globalCfg.GetInt("Emu.ConsoleType") == 1)
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

    actDateTime->setEnabled(true);
    actPowerManagement->setEnabled(false);

    actSetupCheats->setEnabled(false);
    actTitleManager->setEnabled(!globalCfg.GetString("DSi.NANDPath").empty());

    actEnableCheats->setChecked(localCfg.GetBool("EnableCheats"));

    actROMInfo->setEnabled(false);
    actRAMInfo->setEnabled(false);

    actSavestateSRAMReloc->setChecked(globalCfg.GetBool("Savestate.RelocSRAM"));

    actScreenRotation[windowCfg.GetInt("ScreenRotation")]->setChecked(true);

    int screenGap = windowCfg.GetInt("ScreenGap");
    for (int i = 0; i < 6; i++)
    {
        if (actScreenGap[i]->data().toInt() == screenGap)
        {
            actScreenGap[i]->setChecked(true);
            break;
        }
    }

    actScreenLayout[windowCfg.GetInt("ScreenLayout")]->setChecked(true);
    actScreenSizing[windowCfg.GetInt("ScreenSizing")]->setChecked(true);
    actIntegerScaling->setChecked(windowCfg.GetBool("IntegerScaling"));

    actScreenSwap->setChecked(windowCfg.GetBool("ScreenSwap"));

    int aspectTop = windowCfg.GetInt("ScreenAspectTop");
    int aspectBot = windowCfg.GetInt("ScreenAspectBot");
    for (int i = 0; i < AspectRatiosNum; i++)
    {
        if (aspectTop == aspectRatios[i].id)
            actScreenAspectTop[i]->setChecked(true);
        if (aspectBot == aspectRatios[i].id)
            actScreenAspectBot[i]->setChecked(true);
    }

    actScreenFiltering->setChecked(windowCfg.GetBool("ScreenFilter"));
    actShowOSD->setChecked(showOSD);

    actLimitFramerate->setChecked(emuInstance->doLimitFPS);
    actAudioSync->setChecked(emuInstance->doAudioSync);

    if (emuInstance->instanceID > 0)
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

    QObject::connect(qApp, &QApplication::applicationStateChanged, this, &MainWindow::onAppStateChanged);
    onUpdateInterfaceSettings();

    updateMPInterface(MPInterface::GetType());
}

MainWindow::~MainWindow()
{
    delete[] actScreenAspectTop;
    delete[] actScreenAspectBot;
}

void MainWindow::osdAddMessage(unsigned int color, const char* msg)
{
    if (!showOSD) return;
    panel->osdAddMessage(color, msg);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    QByteArray geom = saveGeometry();
    QByteArray enc = geom.toBase64(QByteArray::Base64Encoding);
    windowCfg.SetString("Geometry", enc.toStdString());
    Config::Save();

    emuInstance->deleteWindow(windowID, false);

    // emuInstance may be deleted
    // prevent use after free from us
    emuInstance = nullptr;
    QMainWindow::closeEvent(event);
}

void MainWindow::createScreenPanel()
{
    hasOGL = globalCfg.GetBool("Screen.UseGL") ||
            (globalCfg.GetInt("3D.Renderer") != renderer3D_Software);

    if (hasOGL)
    {
        ScreenPanelGL* panelGL = new ScreenPanelGL(this);
        panelGL->show();

        panel = panelGL;

        panelGL->createContext();
    }

    if (!hasOGL)
    {
        ScreenPanelNative* panelNative = new ScreenPanelNative(this);
        panel = panelNative;
        panel->show();
    }
    setCentralWidget(panel);

    actScreenFiltering->setEnabled(hasOGL);
    panel->osdSetEnabled(showOSD);

    connect(this, SIGNAL(screenLayoutChange()), panel, SLOT(onScreenLayoutChanged()));
    emit screenLayoutChange();
}

GL::Context* MainWindow::getOGLContext()
{
    if (!hasOGL) return nullptr;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->getContext();
}

void MainWindow::initOpenGL()
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->initOpenGL();
}

void MainWindow::deinitOpenGL()
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->deinitOpenGL();
}

void MainWindow::setGLSwapInterval(int intv)
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->setSwapInterval(intv);
}

void MainWindow::makeCurrentGL()
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->makeCurrentGL();
}

void MainWindow::drawScreenGL()
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->drawScreenGL();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    // TODO!! REMOVE ME IN RELEASE BUILDS!!
    //if (event->key() == Qt::Key_F11) emuThread->NDS->debug(0);

    emuInstance->onKeyPress(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->isAutoRepeat()) return;

    emuInstance->onKeyRelease(event);
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
        if (!emuInstance->loadROM(file, true))
        {
            emuThread->emuUnpause();
            return;
        }

        const QString barredFilename = file.join('|');
        recentFileList.removeAll(barredFilename);
        recentFileList.prepend(barredFilename);
        updateRecentFilesMenu();

        assert(emuInstance->nds != nullptr);
        emuInstance->nds->Start();
        emuThread->emuRun();

        updateCartInserted(false);
    }
    else if (isGbaRom)
    {
        if (!emuInstance->loadGBAROM(file))
        {
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
    emuInstance->audioMute();
}

void MainWindow::focusOutEvent(QFocusEvent* event)
{
    // focusOutEvent is called through the window close event handler
    // prevent use after free
    if (emuInstance)
        emuInstance->audioMute();
}

void MainWindow::onAppStateChanged(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationInactive)
    {
        emuInstance->keyReleaseAll();
        if (pauseOnLostFocus && emuThread->emuIsRunning())
            emuThread->emuPause();
    }
    else if (state == Qt::ApplicationActive)
    {
        if (pauseOnLostFocus && !pausedManually)
            emuThread->emuUnpause();
    }
}

bool MainWindow::verifySetup()
{
    QString res = emuInstance->verifySetup();
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
        if (!emuInstance->loadGBAROM(gbafile)) return false;

        gbaloaded = true;
    }

    bool ndsloaded = false;
    if (!file.isEmpty())
    {
        if (!emuInstance->loadROM(file, true)) return false;
        
        recentFileList.removeAll(file.join("|"));
        recentFileList.prepend(file.join("|"));
        updateRecentFilesMenu();
        ndsloaded = true;
    }

    if (boot)
    {
        if (ndsloaded)
        {
            emuInstance->nds->Start();
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
        globalCfg.GetQString("LastROMFolder"),
        "All supported files (*" + allROMs + ")" + extraFilters
    );

    if (filename.isEmpty()) return {};

    globalCfg.SetQString("LastROMFolder", QFileInfo(filename).dir().path());
    return splitArchivePath(filename, false);
}

void MainWindow::updateCartInserted(bool gba)
{
    bool inserted;
    if (gba)
    {
        inserted = emuInstance->gbaCartInserted() && (globalCfg.GetInt("Emu.ConsoleType") == 0);
        actCurrentGBACart->setText("GBA slot: " + emuInstance->gbaCartLabel());
        actEjectGBACart->setEnabled(inserted);
    }
    else
    {
        inserted = emuInstance->cartInserted();
        actCurrentCart->setText("DS slot: " + emuInstance->cartLabel());
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
    
    if (!emuInstance->loadROM(file, true))
    {
        emuThread->emuUnpause();
        return;
    }

    QString filename = file.join('|');
    recentFileList.removeAll(filename);
    recentFileList.prepend(filename);
    updateRecentFilesMenu();

    assert(emuInstance->nds != nullptr);
    emuInstance->nds->Start();
    emuThread->emuRun();

    updateCartInserted(false);
}

void MainWindow::onClearRecentFiles()
{
    recentFileList.clear();
    globalCfg.GetArray("RecentROM").Clear();
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
    recentMenu->clear();

    Config::Array recentroms = globalCfg.GetArray("RecentROM");
    recentroms.Clear();

    for (int i = 0; i < recentFileList.size(); ++i)
    {
        if (i >= kMaxRecentROMs) break;

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

        recentroms.SetQString(i, recentFileList.at(i));
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
    
    if (!emuInstance->loadROM(file, true))
    {
        emuThread->emuUnpause();
        return;
    }

    recentFileList.removeAll(filename);
    recentFileList.prepend(filename);
    updateRecentFilesMenu();

    assert(emuInstance->nds != nullptr);
    emuInstance->nds->Start();
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

    if (!emuInstance->bootToMenu())
    {
        // TODO: better error reporting?
        QMessageBox::critical(this, "melonDS", "This firmware is not bootable.");
        emuThread->emuUnpause();
        return;
    }

    assert(emuInstance->nds != nullptr);
    emuInstance->nds->Start();
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

    if (!emuInstance->loadROM(file, false))
    {
        emuThread->emuUnpause();
        return;
    }

    emuThread->emuUnpause();

    updateCartInserted(false);
}

void MainWindow::onEjectCart()
{
    emuThread->emuPause();

    emuInstance->ejectCart();

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

    if (!emuInstance->loadGBAROM(file))
    {
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

    emuInstance->loadGBAAddon(type);

    emuThread->emuUnpause();

    updateCartInserted(true);
}

void MainWindow::onEjectGBACart()
{
    emuThread->emuPause();

    emuInstance->ejectGBACart();

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
        filename = emuInstance->getSavestateName(slot);
    }
    else
    {
        // TODO: specific 'last directory' for savestate files?
        QString qfilename = QFileDialog::getSaveFileName(this,
                                                         "Save state",
                                                         globalCfg.GetQString("LastROMFolder"),
                                                         "melonDS savestates (*.mln);;Any file (*.*)");
        if (qfilename.isEmpty())
        {
            emuThread->emuUnpause();
            return;
        }

        filename = qfilename.toStdString();
    }

    if (emuInstance->saveState(filename))
    {
        if (slot > 0) emuInstance->osdAddMessage(0, "State saved to slot %d", slot);
        else          emuInstance->osdAddMessage(0, "State saved to file");

        actLoadState[slot]->setEnabled(true);
    }
    else
    {
        emuInstance->osdAddMessage(0xFFA0A0, "State save failed");
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
        filename = emuInstance->getSavestateName(slot);
    }
    else
    {
        // TODO: specific 'last directory' for savestate files?
        QString qfilename = QFileDialog::getOpenFileName(this,
                                                         "Load state",
                                                         globalCfg.GetQString("LastROMFolder"),
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
        if (slot > 0) emuInstance->osdAddMessage(0xFFA0A0, "State slot %d is empty", slot);
        else          emuInstance->osdAddMessage(0xFFA0A0, "State file does not exist");

        emuThread->emuUnpause();
        return;
    }

    if (emuInstance->loadState(filename))
    {
        if (slot > 0) emuInstance->osdAddMessage(0, "State loaded from slot %d", slot);
        else          emuInstance->osdAddMessage(0, "State loaded from file");

        actUndoStateLoad->setEnabled(true);
    }
    else
    {
        emuInstance->osdAddMessage(0xFFA0A0, "State load failed");
    }

    emuThread->emuUnpause();
}

void MainWindow::onUndoStateLoad()
{
    emuThread->emuPause();
    emuInstance->undoStateLoad();
    emuThread->emuUnpause();

    emuInstance->osdAddMessage(0, "State load undone");
}

void MainWindow::onImportSavefile()
{
    emuThread->emuPause();
    QString path = QFileDialog::getOpenFileName(this,
                                            "Select savefile",
                                            globalCfg.GetQString("LastROMFolder"),
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

    if (emuThread->emuIsActive())
    {
        if (QMessageBox::warning(this,
                        "melonDS",
                        "The emulation will be reset and the current savefile overwritten.",
                        QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok)
        {
            emuThread->emuUnpause();
            return;
        }

        emuInstance->reset();
    }

    u32 len = FileLength(f);

    std::unique_ptr<u8[]> data = std::make_unique<u8[]>(len);
    Platform::FileRewind(f);
    Platform::FileRead(data.get(), len, 1, f);

    assert(emuInstance->nds != nullptr);
    emuInstance->nds->SetNDSSave(data.get(), len);

    CloseFile(f);
    emuThread->emuUnpause();
}

void MainWindow::onQuit()
{
#ifndef _WIN32
    if (!parentWidget())
        signalSn->setEnabled(false);
#endif
    close();
}


void MainWindow::onPause(bool checked)
{
    if (!emuThread->emuIsActive()) return;

    if (checked)
    {
        emuThread->emuPause();
        pausedManually = true;
    }
    else
    {
        emuThread->emuUnpause();
        pausedManually = false;
    }
}

void MainWindow::onReset()
{
    if (!emuThread->emuIsActive()) return;

    emuThread->emuReset();
}

void MainWindow::onStop()
{
    if (!emuThread->emuIsActive()) return;

    emuThread->emuStop(true);
}

void MainWindow::onFrameStep()
{
    if (!emuThread->emuIsActive()) return;

    emuThread->emuFrameStep();
}

void MainWindow::onOpenDateTime()
{
    DateTimeDialog* dlg = DateTimeDialog::openDlg(this);
}

void MainWindow::onOpenPowerManagement()
{
    PowerManagementDialog* dlg = PowerManagementDialog::openDlg(this);
}

void MainWindow::onEnableCheats(bool checked)
{
    localCfg.SetBool("EnableCheats", checked);
    emuInstance->enableCheats(checked);
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
    auto cart = emuInstance->nds->NDSCartSlot.GetCart();
    if (cart)
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
    createEmuInstance();
}

void MainWindow::onLANStartHost()
{
    if (!lanWarning(true)) return;
    LANStartHostDialog::openDlg(this);
}

void MainWindow::onLANStartClient()
{
    if (!lanWarning(false)) return;
    LANStartClientDialog::openDlg(this);
}

void MainWindow::onNPStartHost()
{
    //Netplay::StartHost();
    //NetplayStartHostDialog::openDlg(this);
}

void MainWindow::onNPStartClient()
{
    //Netplay::StartClient();
    //NetplayStartClientDialog::openDlg(this);
}

void MainWindow::onNPTest()
{
    // HAX
    //Netplay::StartGame();
}

void MainWindow::updateMPInterface(MPInterfaceType type)
{
    // MP interface was changed, reflect it in the UI

    bool enable = (type == MPInterface_Local);
    actMPNewInstance->setEnabled(enable);
    actLANStartHost->setEnabled(enable);
    actLANStartClient->setEnabled(enable);
    /*actNPStartHost->setEnabled(enable);
    actNPStartClient->setEnabled(enable);
    actNPTest->setEnabled(enable);*/
}

bool MainWindow::lanWarning(bool host)
{
    if (numEmuInstances() < 2)
        return true;

    QString verb = host ? "host" : "join";
    QString msg = "Multiple emulator instances are currently open.\n"
            "If you "+verb+" a LAN game now, all secondary instances will be closed.\n\n"
            "Do you wish to continue?";

    auto res = QMessageBox::warning(this, "melonDS", msg, QMessageBox::Yes|QMessageBox::No, QMessageBox::No);
    if (res == QMessageBox::No)
        return false;

    deleteAllEmuInstances(1);
    return true;
}

void MainWindow::onOpenEmuSettings()
{
    emuThread->emuPause();

    EmuSettingsDialog* dlg = EmuSettingsDialog::openDlg(this);
    connect(dlg, &EmuSettingsDialog::finished, this, &MainWindow::onEmuSettingsDialogFinished);
}

void MainWindow::onEmuSettingsDialogFinished(int res)
{
    if (globalCfg.GetInt("Emu.ConsoleType") == 1)
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
        actEjectGBACart->setEnabled(emuInstance->gbaCartInserted());
    }

    if (EmuSettingsDialog::needsReset)
        onReset();

    actCurrentGBACart->setText("GBA slot: " + emuInstance->gbaCartLabel());

    if (!emuThread->emuIsActive())
        actTitleManager->setEnabled(!globalCfg.GetString("DSi.NANDPath").empty());

    emuThread->emuUnpause();
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
    AudioSettingsDialog* dlg = AudioSettingsDialog::openDlg(this);
    connect(emuThread, &EmuThread::syncVolumeLevel, dlg, &AudioSettingsDialog::onSyncVolumeLevel);
    connect(emuThread, &EmuThread::windowEmuStart, dlg, &AudioSettingsDialog::onConsoleReset);
    connect(dlg, &AudioSettingsDialog::updateAudioVolume, this, &MainWindow::onUpdateAudioVolume);
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

void MainWindow::onUpdateAudioVolume(int vol, int dsisync)
{
    emuInstance->audioVolume = vol;
    emuInstance->audioDSiVolumeSync = dsisync;
}

void MainWindow::onUpdateAudioSettings()
{
    assert(emuInstance->nds != nullptr);

    int interp = globalCfg.GetInt("Audio.Interpolation");
    emuInstance->nds->SPU.SetInterpolation(static_cast<AudioInterpolation>(interp));

    int bitdepth = globalCfg.GetInt("Audio.BitDepth");
    if (bitdepth == 0)
        emuInstance->nds->SPU.SetDegrade10Bit(emuInstance->nds->ConsoleType == 0);
    else
        emuInstance->nds->SPU.SetDegrade10Bit(bitdepth == 1);
}

void MainWindow::onAudioSettingsFinished(int res)
{
    //AudioInOut::UpdateSettings(*emuThread->NDS);
}

void MainWindow::onOpenMPSettings()
{
    emuThread->emuPause();

    MPSettingsDialog* dlg = MPSettingsDialog::openDlg(this);
    connect(dlg, &MPSettingsDialog::finished, this, &MainWindow::onMPSettingsFinished);
}

void MainWindow::onMPSettingsFinished(int res)
{
    emuInstance->mpAudioMode = globalCfg.GetInt("MP.AudioMode");
    emuInstance->audioMute();
    MPInterface::Get().SetRecvTimeout(globalCfg.GetInt("MP.RecvTimeout"));

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
    if (WifiSettingsDialog::needsReset)
        onReset();

    emuThread->emuUnpause();
}

void MainWindow::onOpenInterfaceSettings()
{
    emuThread->emuPause();
    InterfaceSettingsDialog* dlg = InterfaceSettingsDialog::openDlg(this);
    connect(dlg, &InterfaceSettingsDialog::finished, this, &MainWindow::onInterfaceSettingsFinished);
    connect(dlg, &InterfaceSettingsDialog::updateInterfaceSettings, this, &MainWindow::onUpdateInterfaceSettings);
}

void MainWindow::onUpdateInterfaceSettings()
{
    pauseOnLostFocus = globalCfg.GetBool("PauseLostFocus");
    emuInstance->targetFPS = 1.0 / globalCfg.GetDouble("TargetFPS");
    emuInstance->fastForwardFPS = 1.0 / globalCfg.GetDouble("FastForwardFPS");
    emuInstance->slowmoFPS = 1.0 / globalCfg.GetDouble("SlowmoFPS");
    panel->setMouseHide(globalCfg.GetBool("MouseHide"),
                        globalCfg.GetInt("MouseHideSeconds")*1000);
}

void MainWindow::onInterfaceSettingsFinished(int res)
{
    emuThread->emuUnpause();
}

void MainWindow::onChangeSavestateSRAMReloc(bool checked)
{
    globalCfg.SetBool("Savestate.RelocSRAM", checked);
}

void MainWindow::onChangeScreenSize()
{
    int factor = ((QAction*)sender())->data().toInt();
    QSize diff = size() - panel->size();
    resize(panel->screenGetMinSize(factor) + diff);
}

void MainWindow::onChangeScreenRotation(QAction* act)
{
    int rot = act->data().toInt();
    windowCfg.SetInt("ScreenRotation", rot);

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenGap(QAction* act)
{
    int gap = act->data().toInt();
    windowCfg.SetInt("ScreenGap", gap);

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenLayout(QAction* act)
{
    int layout = act->data().toInt();
    windowCfg.SetInt("ScreenLayout", layout);

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenSwap(bool checked)
{
    windowCfg.SetBool("ScreenSwap", checked);

    // Swap between top and bottom screen when displaying one screen.
    int sizing = windowCfg.GetInt("ScreenSizing");
    if (sizing == screenSizing_TopOnly)
    {
        // Bottom Screen.
        sizing = screenSizing_BotOnly;
        actScreenSizing[screenSizing_TopOnly]->setChecked(false);
        actScreenSizing[sizing]->setChecked(true);
    }
    else if (sizing == screenSizing_BotOnly)
    {
        // Top Screen.
        sizing = screenSizing_TopOnly;
        actScreenSizing[screenSizing_BotOnly]->setChecked(false);
        actScreenSizing[sizing]->setChecked(true);
    }
    windowCfg.SetInt("ScreenSizing", sizing);

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenSizing(QAction* act)
{
    int sizing = act->data().toInt();
    windowCfg.SetInt("ScreenSizing", sizing);

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenAspect(QAction* act)
{
    int aspect = act->data().toInt();
    QActionGroup* group = act->actionGroup();

    if (group == grpScreenAspectTop)
    {
        windowCfg.SetInt("ScreenAspectTop", aspect);
    }
    else
    {
        windowCfg.SetInt("ScreenAspectBot", aspect);
    }

    emit screenLayoutChange();
}

void MainWindow::onChangeIntegerScaling(bool checked)
{
    windowCfg.SetBool("IntegerScaling", checked);

    emit screenLayoutChange();
}

void MainWindow::onChangeScreenFiltering(bool checked)
{
    windowCfg.SetBool("ScreenFilter", checked);

    //emit screenLayoutChange();
    panel->setFilter(checked);
}

void MainWindow::onChangeShowOSD(bool checked)
{
    showOSD = checked;
    panel->osdSetEnabled(showOSD);
    windowCfg.SetBool("ShowOSD", showOSD);
}

void MainWindow::onChangeLimitFramerate(bool checked)
{
    emuInstance->doLimitFPS = checked;
    globalCfg.SetBool("LimitFPS", emuInstance->doLimitFPS);
}

void MainWindow::onChangeAudioSync(bool checked)
{
    emuInstance->doAudioSync = checked;
    globalCfg.SetBool("AudioSync", emuInstance->doAudioSync);
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
    int currentSizing = windowCfg.GetInt("ScreenSizing");
    if (currentSizing == screenSizing_EmphTop)
    {
        currentSizing = screenSizing_EmphBot;
    }
    else if (currentSizing == screenSizing_EmphBot)
    {
        currentSizing = screenSizing_EmphTop;
    }
    windowCfg.SetInt("ScreenSizing", currentSizing);

    emit screenLayoutChange();
}

void MainWindow::onEmuStart()
{
    for (int i = 1; i < 9; i++)
    {
        actSaveState[i]->setEnabled(true);
        actLoadState[i]->setEnabled(emuInstance->savestateExists(i));
    }
    actSaveState[0]->setEnabled(true);
    actLoadState[0]->setEnabled(true);
    actUndoStateLoad->setEnabled(false);

    actPause->setEnabled(true);
    actPause->setChecked(false);
    actReset->setEnabled(true);
    actStop->setEnabled(true);
    actFrameStep->setEnabled(true);

    actDateTime->setEnabled(false);
    actPowerManagement->setEnabled(true);

    actTitleManager->setEnabled(false);
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
    actFrameStep->setEnabled(false);

    actDateTime->setEnabled(true);
    actPowerManagement->setEnabled(false);

    actTitleManager->setEnabled(!globalCfg.GetString("DSi.NANDPath").empty());
}

void MainWindow::onEmuPause(bool pause)
{
    actPause->setChecked(pause);
}

void MainWindow::onEmuReset()
{
    actUndoStateLoad->setEnabled(false);
}

void MainWindow::onUpdateVideoSettings(bool glchange)
{
    if (glchange)
    {
        emuThread->emuPause();
        if (hasOGL) emuThread->deinitContext();

        delete panel;
        createScreenPanel();
        connect(emuThread, SIGNAL(windowUpdate()), panel, SLOT(repaint()));
    }

    emuThread->updateVideoSettings();

    if (glchange)
    {
        if (hasOGL) emuThread->initContext();
        emuThread->emuUnpause();
    }
}
