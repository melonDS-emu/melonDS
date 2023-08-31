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

#ifndef MAIN_H
#define MAIN_H

#include "glad/glad.h"

#include <QApplication>
#include <QThread>
#include <QWidget>
#include <QWindow>
#include <QMainWindow>
#include <QImage>
#include <QActionGroup>
#include <QTimer>
#include <QMutex>
#include <QScreen>
#include <QCloseEvent>

#include <atomic>

#include <optional>

#include "FrontendUtil.h"
#include "duckstation/gl/context.h"

class EmuThread : public QThread
{
    Q_OBJECT
    void run() override;

public:
    explicit EmuThread(QObject* parent = nullptr);

    void changeWindowTitle(char* title);

    // to be called from the UI thread
    void emuRun();
    void emuPause();
    void emuUnpause();
    void emuStop();
    void emuFrameStep();

    bool emuIsRunning();
    bool emuIsActive();

    void initContext();
    void deinitContext();

    int FrontBuffer = 0;
    QMutex FrontBufferLock;

    void updateScreenSettings(bool filter, const WindowInfo& windowInfo, int numScreens, int* screenKind, float* screenMatrix);

signals:
    void windowUpdate();
    void windowTitleChange(QString title);

    void windowEmuStart();
    void windowEmuStop();
    void windowEmuPause();
    void windowEmuReset();
    void windowEmuFrameStep();

    void windowLimitFPSChange();

    void screenLayoutChange();

    void windowFullscreenToggle();

    void swapScreensToggle();
    void screenEmphasisToggle();

    void syncVolumeLevel();

private:
    void drawScreenGL();
    void initOpenGL();
    void deinitOpenGL();

    enum EmuStatusKind
    {
        emuStatus_Exit,
        emuStatus_Running,
        emuStatus_Paused,
        emuStatus_FrameStep,
    };
    std::atomic<EmuStatusKind> EmuStatus;

    EmuStatusKind PrevEmuStatus;
    EmuStatusKind EmuRunning;

    constexpr static int EmuPauseStackRunning = 0;
    constexpr static int EmuPauseStackPauseThreshold = 1;
    int EmuPauseStack;

    enum ContextRequestKind
    {
        contextRequest_None = 0,
        contextRequest_InitGL,
        contextRequest_DeInitGL
    };
    std::atomic<ContextRequestKind> ContextRequest = contextRequest_None;

    GL::Context* oglContext = nullptr;
    GLuint screenVertexBuffer, screenVertexArray;
    GLuint screenTexture;
    GLuint screenShaderProgram[3];
    GLuint screenShaderTransformULoc, screenShaderScreenSizeULoc;

    QMutex screenSettingsLock;
    WindowInfo windowInfo;
    float screenMatrix[Frontend::MaxScreenTransforms][6];
    int screenKind[Frontend::MaxScreenTransforms];
    int numScreens;
    bool filter;

    int lastScreenWidth = -1, lastScreenHeight = -1;
};


class ScreenHandler
{
    Q_GADGET

public:
    ScreenHandler(QWidget* widget);
    virtual ~ScreenHandler();
    QTimer* setupMouseTimer();
    void updateMouseTimer();
    QTimer* mouseTimer;
    QSize screenGetMinSize(int factor);

protected:
    void screenSetupLayout(int w, int h);

    void screenOnMousePress(QMouseEvent* event);
    void screenOnMouseRelease(QMouseEvent* event);
    void screenOnMouseMove(QMouseEvent* event);

    void screenHandleTablet(QTabletEvent* event);
    void screenHandleTouch(QTouchEvent* event);

    float screenMatrix[Frontend::MaxScreenTransforms][6];
    int screenKind[Frontend::MaxScreenTransforms];
    int numScreens;

    bool touching = false;

    void showCursor();
};


class ScreenPanelNative : public QWidget, public ScreenHandler
{
    Q_OBJECT

public:
    explicit ScreenPanelNative(QWidget* parent);
    virtual ~ScreenPanelNative();

protected:
    void paintEvent(QPaintEvent* event) override;

    void resizeEvent(QResizeEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

    void tabletEvent(QTabletEvent* event) override;
    bool event(QEvent* event) override;
private slots:
    void onScreenLayoutChanged();

private:
    void setupScreenLayout();

    QImage screen[2];
    QTransform screenTrans[Frontend::MaxScreenTransforms];
};


class ScreenPanelGL : public QWidget, public ScreenHandler
{
    Q_OBJECT

public:
    explicit ScreenPanelGL(QWidget* parent);
    virtual ~ScreenPanelGL();

    std::optional<WindowInfo> getWindowInfo();

    bool createContext();

    GL::Context* getContext() { return glContext.get(); }

    void transferLayout(EmuThread* thread);
protected:

    qreal devicePixelRatioFromScreen() const;
    int scaledWindowWidth() const;
    int scaledWindowHeight() const;

    QPaintEngine* paintEngine() const override;

    void resizeEvent(QResizeEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

    void tabletEvent(QTabletEvent* event) override;
    bool event(QEvent* event) override;

private slots:
    void onScreenLayoutChanged();

private:
    void setupScreenLayout();

    std::unique_ptr<GL::Context> glContext;
};

class MelonApplication : public QApplication
{
    Q_OBJECT

public:
    MelonApplication(int &argc, char** argv);
    bool event(QEvent* event) override;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    bool hasOGL;
    GL::Context* getOGLContext();

    bool preloadROMs(QStringList file, QStringList gbafile, bool boot);
    QStringList splitArchivePath(const QString& filename, bool useMemberSyntax);

    void onAppStateChanged(Qt::ApplicationState state);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

signals:
    void screenLayoutChange();

private slots:
    void onOpenFile();
    void onClickRecentFile();
    void onClearRecentFiles();
    void onBootFirmware();
    void onInsertCart();
    void onEjectCart();
    void onInsertGBACart();
    void onInsertGBAAddon();
    void onEjectGBACart();
    void onSaveState();
    void onLoadState();
    void onUndoStateLoad();
    void onImportSavefile();
    void onQuit();

    void onPause(bool checked);
    void onReset();
    void onStop();
    void onFrameStep();
    void onEnableCheats(bool checked);
    void onSetupCheats();
    void onCheatsDialogFinished(int res);
    void onROMInfo();
    void onRAMInfo();
    void onOpenTitleManager();
    void onMPNewInstance();

    void onOpenEmuSettings();
    void onEmuSettingsDialogFinished(int res);
    void onOpenPowerManagement();
    void onOpenInputConfig();
    void onInputConfigFinished(int res);
    void onOpenVideoSettings();
    void onOpenCameraSettings();
    void onCameraSettingsFinished(int res);
    void onOpenAudioSettings();
    void onUpdateAudioSettings();
    void onAudioSettingsFinished(int res);
    void onOpenMPSettings();
    void onMPSettingsFinished(int res);
    void onOpenWifiSettings();
    void onWifiSettingsFinished(int res);
    void onOpenFirmwareSettings();
    void onFirmwareSettingsFinished(int res);
    void onOpenPathSettings();
    void onPathSettingsFinished(int res);
    void onOpenInterfaceSettings();
    void onInterfaceSettingsFinished(int res);
    void onUpdateMouseTimer();
    void onChangeSavestateSRAMReloc(bool checked);
    void onChangeScreenSize();
    void onChangeScreenRotation(QAction* act);
    void onChangeScreenGap(QAction* act);
    void onChangeScreenLayout(QAction* act);
    void onChangeScreenSwap(bool checked);
    void onChangeScreenSizing(QAction* act);
    void onChangeScreenAspect(QAction* act);
    void onChangeIntegerScaling(bool checked);
    void onChangeScreenFiltering(bool checked);
    void onChangeShowOSD(bool checked);
    void onChangeLimitFramerate(bool checked);
    void onChangeAudioSync(bool checked);

    void onTitleUpdate(QString title);

    void onEmuStart();
    void onEmuStop();

    void onUpdateVideoSettings(bool glchange);

    void onFullscreenToggled();
    void onScreenEmphasisToggled();

private:
    virtual void closeEvent(QCloseEvent* event) override;

    QStringList currentROM;
    QStringList currentGBAROM;
    QList<QString> recentFileList;
    QMenu *recentMenu;
    void updateRecentFilesMenu();

    bool verifySetup();
    QString pickFileFromArchive(QString archiveFileName);
    QStringList pickROM(bool gba);
    void updateCartInserted(bool gba);

    void createScreenPanel();

    bool pausedManually = false;

    int oldW, oldH;
    bool oldMax;

public:
    ScreenHandler* panel;
    QWidget* panelWidget;

    QAction* actOpenROM;
    QAction* actBootFirmware;
    QAction* actCurrentCart;
    QAction* actInsertCart;
    QAction* actEjectCart;
    QAction* actCurrentGBACart;
    QAction* actInsertGBACart;
    QAction* actInsertGBAAddon[1];
    QAction* actEjectGBACart;
    QAction* actImportSavefile;
    QAction* actSaveState[9];
    QAction* actLoadState[9];
    QAction* actUndoStateLoad;
    QAction* actQuit;

    QAction* actPause;
    QAction* actReset;
    QAction* actStop;
    QAction* actFrameStep;
    QAction* actEnableCheats;
    QAction* actSetupCheats;
    QAction* actROMInfo;
    QAction* actRAMInfo;
    QAction* actTitleManager;
    QAction* actMPNewInstance;

    QAction* actEmuSettings;
#ifdef __APPLE__
    QAction* actPreferences;
#endif
    QAction* actPowerManagement;
    QAction* actInputConfig;
    QAction* actVideoSettings;
    QAction* actCameraSettings;
    QAction* actAudioSettings;
    QAction* actMPSettings;
    QAction* actWifiSettings;
    QAction* actFirmwareSettings;
    QAction* actPathSettings;
    QAction* actInterfaceSettings;
    QAction* actSavestateSRAMReloc;
    QAction* actScreenSize[4];
    QActionGroup* grpScreenRotation;
    QAction* actScreenRotation[Frontend::screenRot_MAX];
    QActionGroup* grpScreenGap;
    QAction* actScreenGap[6];
    QActionGroup* grpScreenLayout;
    QAction* actScreenLayout[Frontend::screenLayout_MAX];
    QAction* actScreenSwap;
    QActionGroup* grpScreenSizing;
    QAction* actScreenSizing[Frontend::screenSizing_MAX];
    QAction* actIntegerScaling;
    QActionGroup* grpScreenAspectTop;
    QAction** actScreenAspectTop;
    QActionGroup* grpScreenAspectBot;
    QAction** actScreenAspectBot;
    QAction* actScreenFiltering;
    QAction* actShowOSD;
    QAction* actLimitFramerate;
    QAction* actAudioSync;
};

#endif // MAIN_H
