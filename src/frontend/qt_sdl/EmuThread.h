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

#ifndef EMUTHREAD_H
#define EMUTHREAD_H

#include <QThread>
#include <QMutex>
#include <QSemaphore>
#include <QQueue>

#include <atomic>
#include <variant>
#include <optional>
#include <list>

#include "NDSCart.h"
#include "GBACart.h"

using Keep = std::monostate;
using UpdateConsoleNDSArgs = std::variant<Keep, std::unique_ptr<melonDS::NDSCart::CartCommon>>;
using UpdateConsoleGBAArgs = std::variant<Keep, std::unique_ptr<melonDS::GBACart::CartCommon>>;
namespace melonDS
{
class NDS;
}

class EmuInstance;
class MainWindow;
class ScreenPanelGL;

class EmuThread : public QThread
{
    Q_OBJECT
    void run() override;

public:
    explicit EmuThread(EmuInstance* inst, QObject* parent = nullptr);

    void attachWindow(MainWindow* window);
    void detachWindow(MainWindow* window);

    enum MessageType
    {
        msg_Exit,

        msg_EmuRun,
        msg_EmuPause,
        msg_EmuUnpause,
        msg_EmuStop,
        msg_EmuFrameStep,
        msg_EmuReset,

        msg_InitGL,
        msg_DeInitGL,
    };

    struct Message
    {
        MessageType type;
        union
        {
            bool stopExternal;
        };
    };

    void sendMessage(Message msg);
    void waitMessage(int num = 1);
    void waitAllMessages();

    void sendMessage(MessageType type)
    {
        return sendMessage({.type = type});
    }

    void changeWindowTitle(char* title);

    // to be called from the UI thread
    void emuRun();
    void emuPause();
    void emuUnpause();
    void emuTogglePause();
    void emuStop(bool external);
    void emuExit();
    void emuFrameStep();
    void emuReset();

    bool emuIsRunning();
    bool emuIsActive();

    void initContext();
    void deinitContext();
    void updateVideoSettings() { videoSettingsDirty = true; }

    int FrontBuffer = 0;
    QMutex FrontBufferLock;

signals:
    void windowUpdate();
    void windowTitleChange(QString title);

    void windowEmuStart();
    void windowEmuStop();
    void windowEmuPause(bool pause);
    void windowEmuReset();

    void windowLimitFPSChange();

    void autoScreenSizingChange(int sizing);

    void windowFullscreenToggle();

    void swapScreensToggle();
    void screenEmphasisToggle();

    void syncVolumeLevel();

private:
    void handleMessages();

    void updateRenderer();
    void compileShaders();

    enum EmuStatusKind
    {
        emuStatus_Exit,
        emuStatus_Running,
        emuStatus_Paused,
        emuStatus_FrameStep,
    };

    EmuStatusKind prevEmuStatus;
    EmuStatusKind emuStatus;
    bool emuActive;

    constexpr static int emuPauseStackRunning = 0;
    constexpr static int emuPauseStackPauseThreshold = 1;
    int emuPauseStack;

    QMutex msgMutex;
    QSemaphore msgSemaphore;
    QQueue<Message> msgQueue;

    EmuInstance* emuInstance;

    int autoScreenSizing;

    int lastVideoRenderer = -1;

    double perfCountsSec;

    bool useOpenGL;
    int videoRenderer;
    bool videoSettingsDirty;
};

#endif // EMUTHREAD_H
