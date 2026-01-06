/*
    Copyright 2016-2025 melonDS team

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
#include <QWaitCondition>
#include <QQueue>
#include <QVariant>

#include <atomic>
#include <variant>
#include <optional>
#include <list>

#include "NDSCart.h"
#include "GBACart.h"

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
        msg_BorrowGL,

        msg_BootROM,
        msg_BootFirmware,
        msg_InsertCart,
        msg_EjectCart,
        msg_InsertGBACart,
        msg_InsertGBAAddon,
        msg_EjectGBACart,

        msg_LoadState,
        msg_SaveState,
        msg_UndoStateLoad,

        msg_ImportSavefile,

        msg_EnableCheats,
    };

    struct Message
    {
        MessageType type;
        QVariant param;
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
    void emuPause(bool broadcast = true);
    void emuUnpause(bool broadcast = true);
    void emuTogglePause(bool broadcast = true);
    void emuStop(bool external);
    void emuExit();
    void emuFrameStep();
    void emuReset();

    int bootROM(const QStringList& filename, QString& errorstr);
    int bootFirmware(QString& errorstr);
    int insertCart(const QStringList& filename, bool gba, QString& errorstr);
    void ejectCart(bool gba);
    int insertGBAAddon(int type, QString& errorstr);

    int saveState(const QString& filename);
    int loadState(const QString& filename);
    int undoStateLoad();

    int importSavefile(const QString& filename);

    void enableCheats(bool enable);

    bool emuIsRunning();
    bool emuIsActive();

    void initContext(int win);
    void deinitContext(int win);
    void borrowGL();
    void returnGL();
    void updateVideoSettings() { videoSettingsDirty = true; }
    void updateVideoRenderer() { videoSettingsDirty = true; lastVideoRenderer = -1; }

    int frontBuffer = 0;
    QMutex frontBufferLock;

    QWaitCondition glBorrowCond;
    QMutex glBorrowMutex;

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

    int msgResult = 0;
    QString msgError;

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
