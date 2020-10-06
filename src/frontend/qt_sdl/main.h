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

#ifndef MAIN_H
#define MAIN_H

#include <QThread>
#include <QWidget>
#include <QWindow>
#include <QMainWindow>
#include <QImage>
#include <QActionGroup>

#include <QOffscreenSurface>
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_3_2_Core>
#include <QOpenGLShaderProgram>


class EmuThread : public QThread
{
    Q_OBJECT
    void run() override;

public:
    explicit EmuThread(QObject* parent = nullptr);

    void initOpenGL();
    void deinitOpenGL();

    void* oglGetProcAddress(const char* proc);

    void changeWindowTitle(char* title);

    // to be called from the UI thread
    void emuRun();
    void emuPause();
    void emuUnpause();
    void emuStop();

    bool emuIsRunning();

signals:
    void windowUpdate();
    void windowTitleChange(QString title);

    void windowEmuStart();
    void windowEmuStop();
    void windowEmuPause();
    void windowEmuReset();

    void windowLimitFPSChange();

    void screenLayoutChange();
    
    void windowFullscreenToggle();

private:
    volatile int EmuStatus;
    int PrevEmuStatus;
    int EmuRunning;
    int EmuPause;

    QOffscreenSurface* oglSurface;
    QOpenGLContext* oglContext;
};


class ScreenHandler
{
    Q_GADGET

public:
    virtual ~ScreenHandler() {}

protected:
    void screenSetupLayout(int w, int h);

    QSize screenGetMinSize();

    void screenOnMousePress(QMouseEvent* event);
    void screenOnMouseRelease(QMouseEvent* event);
    void screenOnMouseMove(QMouseEvent* event);

    float screenMatrix[2][6];

    bool touching;
};


class ScreenPanelNative : public QWidget, public ScreenHandler
{
    Q_OBJECT

public:
    explicit ScreenPanelNative(QWidget* parent);
    ~ScreenPanelNative();

protected:
    void paintEvent(QPaintEvent* event) override;

    void resizeEvent(QResizeEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private slots:
    void onScreenLayoutChanged();

private:
    void setupScreenLayout();

    QImage screen[2];
    QTransform screenTrans[2];
};


class ScreenPanelGL : public QOpenGLWidget, public ScreenHandler, protected QOpenGLFunctions_3_2_Core
{
    Q_OBJECT

public:
    explicit ScreenPanelGL(QWidget* parent);
    ~ScreenPanelGL();

protected:
    void initializeGL() override;

    void paintGL() override;

    void resizeEvent(QResizeEvent* event) override;
    void resizeGL(int w, int h) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private slots:
    void onScreenLayoutChanged();

private:
    void setupScreenLayout();

    QOpenGLShaderProgram* screenShader;
    GLuint screenVertexBuffer;
    GLuint screenVertexArray;
    GLuint screenTexture;
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    bool hasOGL;
    QOpenGLContext* getOGLContext();

protected:
    void resizeEvent(QResizeEvent* event) override;

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

signals:
    void screenLayoutChange();

private slots:
    void onOpenFile();
    void onBootFirmware();
    void onSaveState();
    void onLoadState();
    void onUndoStateLoad();
    void onImportSavefile();
    void onQuit();

    void onPause(bool checked);
    void onReset();
    void onStop();
    void onEnableCheats(bool checked);
    void onSetupCheats();
    void onCheatsDialogFinished(int res);

    void onOpenEmuSettings();
    void onEmuSettingsDialogFinished(int res);
    void onOpenInputConfig();
    void onInputConfigFinished(int res);
    void onOpenVideoSettings();
    void onOpenAudioSettings();
    void onOpenFirmwareSettings();
    void onAudioSettingsFinished(int res);
    void onOpenWifiSettings();
    void onWifiSettingsFinished(int res);
    void onFirmwareSettingsFinished(int res);
    void onChangeSavestateSRAMReloc(bool checked);
    void onChangeScreenSize();
    void onChangeScreenRotation(QAction* act);
    void onChangeScreenGap(QAction* act);
    void onChangeScreenLayout(QAction* act);
    void onChangeScreenSizing(QAction* act);
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

private:
    void createScreenPanel();

    QString loadErrorStr(int error);

public:
    QWidget* panel;

    QAction* actOpenROM;
    QAction* actBootFirmware;
    QAction* actSaveState[9];
    QAction* actLoadState[9];
    QAction* actUndoStateLoad;
    QAction* actImportSavefile;
    QAction* actQuit;

    QAction* actPause;
    QAction* actReset;
    QAction* actStop;
    QAction* actEnableCheats;
    QAction* actSetupCheats;

    QAction* actEmuSettings;
    QAction* actInputConfig;
    QAction* actVideoSettings;
    QAction* actAudioSettings;
    QAction* actWifiSettings;
    QAction* actFirmwareSettings;
    QAction* actSavestateSRAMReloc;
    QAction* actScreenSize[4];
    QActionGroup* grpScreenRotation;
    QAction* actScreenRotation[4];
    QActionGroup* grpScreenGap;
    QAction* actScreenGap[6];
    QActionGroup* grpScreenLayout;
    QAction* actScreenLayout[3];
    QActionGroup* grpScreenSizing;
    QAction* actScreenSizing[4];
    QAction* actIntegerScaling;
    QAction* actScreenFiltering;
    QAction* actShowOSD;
    QAction* actLimitFramerate;
    QAction* actAudioSync;
};

#endif // MAIN_H
