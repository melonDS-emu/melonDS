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

#include "Platform.h"
#include "Config.h"
#include "version.h"
#include "Savestate.h"
#include "LocalMP.h"

//#include "main_shaders.h"

#include "ROMManager.h"
#include "ArchiveUtil.h"
#include "CameraManager.h"

using namespace melonDS;

// TEMP
extern MainWindow* mainWindow;
extern EmuThread* emuThread;
extern bool RunningSomething;
extern QString NdsRomMimeType;
extern QStringList NdsRomExtensions;
extern QString GbaRomMimeType;
extern QStringList GbaRomExtensions;
extern QStringList ArchiveMimeTypes;
extern QStringList ArchiveExtensions;
/*static bool FileExtensionInList(const QString& filename, const QStringList& extensions, Qt::CaseSensitivity cs);
static bool MimeTypeInList(const QMimeType& mimetype, const QStringList& superTypeNames);
static bool NdsRomByExtension(const QString& filename);
static bool GbaRomByExtension(const QString& filename);
static bool SupportedArchiveByExtension(const QString& filename);
static bool NdsRomByMimetype(const QMimeType& mimetype);
static bool GbaRomByMimetype(const QMimeType& mimetype);
static bool SupportedArchiveByMimetype(const QMimeType& mimetype);
static bool ZstdNdsRomByExtension(const QString& filename);
static bool ZstdGbaRomByExtension(const QString& filename);
static bool FileIsSupportedFiletype(const QString& filename, bool insideArchive);*/

extern CameraManager* camManager[2];
extern bool camStarted[2];

extern int videoRenderer;
extern bool videoSettingsDirty;


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
            actInsertGBAAddon[0]->setData(QVariant(GBAAddon_RAMExpansion));
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

    actDateTime->setEnabled(true);
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
    delete[] actScreenAspectTop;
    delete[] actScreenAspectBot;
}

void MainWindow::osdAddMessage(unsigned int color, const char* fmt, ...)
{
    if (fmt == nullptr)
        return;

    char msg[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, 256, fmt, args);
    va_end(args);

    panel->osdAddMessage(color, msg);
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
    panel->osdSetEnabled(Config::ShowOSD);

    connect(this, SIGNAL(screenLayoutChange()), panel, SLOT(onScreenLayoutChanged()));
    emit screenLayoutChange();
}

GL::Context* MainWindow::getOGLContext()
{
    if (!hasOGL) return nullptr;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->getContext();
}

/*void MainWindow::initOpenGL()
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

void MainWindow::drawScreenGL()
{
    if (!hasOGL) return;

    ScreenPanelGL* glpanel = static_cast<ScreenPanelGL*>(panel);
    return glpanel->drawScreenGL();
}*/

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
    //if (event->key() == Qt::Key_F11) emuThread->NDS->debug(0);

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
        if (!ROMManager::LoadROM(mainWindow, emuThread, file, true))
        {
            emuThread->emuUnpause();
            return;
        }

        const QString barredFilename = file.join('|');
        recentFileList.removeAll(barredFilename);
        recentFileList.prepend(barredFilename);
        updateRecentFilesMenu();

        assert(emuThread->NDS != nullptr);
        emuThread->NDS->Start();
        emuThread->emuRun();

        updateCartInserted(false);
    }
    else if (isGbaRom)
    {
        if (!ROMManager::LoadGBAROM(mainWindow, *emuThread->NDS, file))
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
        Input::KeyReleaseAll();
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
        if (!ROMManager::LoadGBAROM(mainWindow, *emuThread->NDS, gbafile)) return false;

        gbaloaded = true;
    }

    bool ndsloaded = false;
    if (!file.isEmpty())
    {
        if (!ROMManager::LoadROM(mainWindow, emuThread, file, true)) return false;
        
        recentFileList.removeAll(file.join("|"));
        recentFileList.prepend(file.join("|"));
        updateRecentFilesMenu();
        ndsloaded = true;
    }

    if (boot)
    {
        if (ndsloaded)
        {
            emuThread->NDS->Start();
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
    
    if (!ROMManager::LoadROM(mainWindow, emuThread, file, true))
    {
        emuThread->emuUnpause();
        return;
    }

    QString filename = file.join('|');
    recentFileList.removeAll(filename);
    recentFileList.prepend(filename);
    updateRecentFilesMenu();

    assert(emuThread->NDS != nullptr);
    emuThread->NDS->Start();
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
    
    if (!ROMManager::LoadROM(mainWindow, emuThread, file, true))
    {
        emuThread->emuUnpause();
        return;
    }

    recentFileList.removeAll(filename);
    recentFileList.prepend(filename);
    updateRecentFilesMenu();

    assert(emuThread->NDS != nullptr);
    emuThread->NDS->Start();
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

    if (!ROMManager::BootToMenu(emuThread))
    {
        // TODO: better error reporting?
        QMessageBox::critical(this, "melonDS", "This firmware is not bootable.");
        emuThread->emuUnpause();
        return;
    }

    assert(emuThread->NDS != nullptr);
    emuThread->NDS->Start();
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

    if (!ROMManager::LoadROM(mainWindow, emuThread, file, false))
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

    ROMManager::EjectCart(*emuThread->NDS);

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

    if (!ROMManager::LoadGBAROM(mainWindow, *emuThread->NDS, file))
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

    ROMManager::LoadGBAAddon(*emuThread->NDS, type);

    emuThread->emuUnpause();

    updateCartInserted(true);
}

void MainWindow::onEjectGBACart()
{
    emuThread->emuPause();

    ROMManager::EjectGBACart(*emuThread->NDS);

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

    if (ROMManager::SaveState(*emuThread->NDS, filename))
    {
        if (slot > 0) osdAddMessage(0, "State saved to slot %d", slot);
        else          osdAddMessage(0, "State saved to file");

        actLoadState[slot]->setEnabled(true);
    }
    else
    {
        osdAddMessage(0xFFA0A0, "State save failed");
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
        if (slot > 0) osdAddMessage(0xFFA0A0, "State slot %d is empty", slot);
        else          osdAddMessage(0xFFA0A0, "State file does not exist");

        emuThread->emuUnpause();
        return;
    }

    if (ROMManager::LoadState(*emuThread->NDS, filename))
    {
        if (slot > 0) osdAddMessage(0, "State loaded from slot %d", slot);
        else          osdAddMessage(0, "State loaded from file");

        actUndoStateLoad->setEnabled(true);
    }
    else
    {
        osdAddMessage(0xFFA0A0, "State load failed");
    }

    emuThread->emuUnpause();
}

void MainWindow::onUndoStateLoad()
{
    emuThread->emuPause();
    ROMManager::UndoStateLoad(*emuThread->NDS);
    emuThread->emuUnpause();

    osdAddMessage(0, "State load undone");
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

        ROMManager::Reset(emuThread);
    }

    u32 len = FileLength(f);

    std::unique_ptr<u8[]> data = std::make_unique<u8[]>(len);
    Platform::FileRewind(f);
    Platform::FileRead(data.get(), len, 1, f);

    assert(emuThread->NDS != nullptr);
    emuThread->NDS->SetNDSSave(data.get(), len);

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
        osdAddMessage(0, "Paused");
        pausedManually = true;
    }
    else
    {
        emuThread->emuUnpause();
        osdAddMessage(0, "Resumed");
        pausedManually = false;
    }
}

void MainWindow::onReset()
{
    if (!RunningSomething) return;

    emuThread->emuPause();

    actUndoStateLoad->setEnabled(false);

    ROMManager::Reset(emuThread);

    osdAddMessage(0, "Reset");
    emuThread->emuRun();
}

void MainWindow::onStop()
{
    if (!RunningSomething) return;

    emuThread->emuPause();
    emuThread->NDS->Stop();
}

void MainWindow::onFrameStep()
{
    if (!RunningSomething) return;

    emuThread->emuFrameStep();
}

void MainWindow::onOpenDateTime()
{
    DateTimeDialog* dlg = DateTimeDialog::openDlg(this);
}

void MainWindow::onOpenPowerManagement()
{
    PowerManagementDialog* dlg = PowerManagementDialog::openDlg(this, emuThread);
}

void MainWindow::onEnableCheats(bool checked)
{
    Config::EnableCheats = checked?1:0;
    ROMManager::EnableCheats(*emuThread->NDS, Config::EnableCheats != 0);
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
    auto cart = emuThread->NDS->NDSCartSlot.GetCart();
    if (cart)
        ROMInfoDialog* dlg = ROMInfoDialog::openDlg(this, *cart);
}

void MainWindow::onRAMInfo()
{
    RAMInfoDialog* dlg = RAMInfoDialog::openDlg(this, emuThread);
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
    AudioSettingsDialog* dlg = AudioSettingsDialog::openDlg(this, emuThread->emuIsActive(), emuThread);
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
    assert(emuThread->NDS != nullptr);
    emuThread->NDS->SPU.SetInterpolation(static_cast<AudioInterpolation>(Config::AudioInterp));

    if (Config::AudioBitDepth == 0)
        emuThread->NDS->SPU.SetDegrade10Bit(emuThread->NDS->ConsoleType == 0);
    else
        emuThread->NDS->SPU.SetDegrade10Bit(Config::AudioBitDepth == 1);
}

void MainWindow::onAudioSettingsFinished(int res)
{
    AudioInOut::UpdateSettings(*emuThread->NDS);
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
    QSize diff = size() - panel->size();
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
    panel->osdSetEnabled(Config::ShowOSD);
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

    actDateTime->setEnabled(false);
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

    actDateTime->setEnabled(true);
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
        connect(emuThread, SIGNAL(windowUpdate()), panel, SLOT(repaint()));
    }

    videoSettingsDirty = true;

    if (glchange)
    {
        if (hasOGL) emuThread->initContext();
        emuThread->emuUnpause();
    }
}
