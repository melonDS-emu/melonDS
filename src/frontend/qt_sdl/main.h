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
#include <variant>
#include <optional>

#include "Window.h"
#include "FrontendUtil.h"
#include "duckstation/gl/context.h"

#include "NDSCart.h"
#include "GBACart.h"

using Keep = std::monostate;
using UpdateConsoleNDSArgs = std::variant<Keep, std::unique_ptr<melonDS::NDSCart::CartCommon>>;
using UpdateConsoleGBAArgs = std::variant<Keep, std::unique_ptr<melonDS::GBACart::CartCommon>>;
namespace melonDS
{
class NDS;
}

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

    //void updateScreenSettings(bool filter, const WindowInfo& windowInfo, int numScreens, int* screenKind, float* screenMatrix);

    /// Applies the config in args.
    /// Creates a new NDS console if needed,
    /// modifies the existing one if possible.
    /// @return \c true if the console was updated.
    /// If this returns \c false, then the existing NDS console is not modified.
    bool UpdateConsole(UpdateConsoleNDSArgs&& ndsargs, UpdateConsoleGBAArgs&& gbaargs) noexcept;
    std::unique_ptr<melonDS::NDS> NDS; // TODO: Proper encapsulation and synchronization
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
    std::unique_ptr<melonDS::NDS> CreateConsole(
        std::unique_ptr<melonDS::NDSCart::CartCommon>&& ndscart,
        std::unique_ptr<melonDS::GBACart::CartCommon>&& gbacart
    ) noexcept;
    //void drawScreenGL();
    //void initOpenGL();
    //void deinitOpenGL();

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

    /*GL::Context* oglContext = nullptr;
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

    int lastScreenWidth = -1, lastScreenHeight = -1;*/
    ScreenPanelGL* screenGL;
};

class MelonApplication : public QApplication
{
    Q_OBJECT

public:
    MelonApplication(int &argc, char** argv);
    bool event(QEvent* event) override;
};

#endif // MAIN_H
