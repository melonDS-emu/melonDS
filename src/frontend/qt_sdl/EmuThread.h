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

#ifndef EMUTHREAD_H
#define EMUTHREAD_H

#include <QThread>
#include <QMutex>

#include <atomic>
#include <variant>
#include <optional>

#include "NDSCart.h"
#include "GBACart.h"

using Keep = std::monostate;
using UpdateConsoleNDSArgs = std::variant<Keep, std::unique_ptr<melonDS::NDSCart::CartCommon>>;
using UpdateConsoleGBAArgs = std::variant<Keep, std::unique_ptr<melonDS::GBACart::CartCommon>>;
namespace melonDS
{
class NDS;
}

class ScreenPanelGL;

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

    ScreenPanelGL* screenGL;

    int autoScreenSizing;

    int videoRenderer;
    bool videoSettingsDirty;
};

#endif // EMUTHREAD_H
