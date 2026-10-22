/*
    Copyright 2016-2026 melonDS team

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

#ifndef EMUINSTANCE_H
#define EMUINSTANCE_H

#include <SDL2/SDL.h>

#include <atomic>

#include "Platform.h"
#include "main.h"
#include "NDS.h"
#include "EmuThread.h"
#include "Window.h"
#include "Config.h"
#include "SaveManager.h"
#include "AudioSpeed.h"
#include "AudioLowPass.h"
#include "AudioTimeStretch.h"
#include "AudioStreamRamp.h"

const int kMaxWindows = 4;

// upper bound on frames drained from the SPU FIFO per emulated frame, and the
// size of the staging buffer that receives them. tied to kMinOutputSkew: at a
// skew of 0.4 the SPU emits 802.3/0.4 ~= 2006 frames per emulated frame, so
// lowering that floor means raising this too.
const int kAudioDrainMax = 2048;

// cross-fade across a stretcher handover, in output frames. kept short: both
// sides are the same material at different points, so a long overlap is heard
// for itself. the dip at the midpoint attenuates the discontinuity the
// handover carries, at the cost of a shallow notch.
const unsigned kAudioHandoverFadeFrames = 256;
const float kAudioHandoverDipDepth = 0.15f;

enum
{
    HK_Lid = 0,
    HK_Mic,
    HK_Pause,
    HK_Reset,
    HK_FastForward,
    HK_FrameLimitToggle,
    HK_FullscreenToggle,
    HK_SwapScreens,
    HK_SwapScreenEmphasis,
    HK_SolarSensorDecrease,
    HK_SolarSensorIncrease,
    HK_FrameStep,
    HK_PowerButton,
    HK_VolumeUp,
    HK_VolumeDown,
    HK_AudioMuteToggle,
    HK_SlowMo,
    HK_FastForwardToggle,
    HK_SlowMoToggle,
    HK_GuitarGripGreen,
    HK_GuitarGripRed,
    HK_GuitarGripYellow,
    HK_GuitarGripBlue,
    HK_MAX
};

enum
{
    micInputType_Silence,
    micInputType_External,
    micInputType_Noise,
    micInputType_Wav,
    micInputType_MAX,
};

enum
{
    renderer3D_Software = 0,
#ifdef OGLRENDERER_ENABLED
    renderer3D_OpenGL,
    renderer3D_OpenGLCompute,
#endif
    renderer3D_Max,
};

bool isRightModKey(QKeyEvent* event);
int getEventKeyVal(QKeyEvent* event);

class EmuInstance
{
public:
    EmuInstance(int inst);
    ~EmuInstance();

    int getInstanceID() { return instanceID; }
    int getConsoleType() { return consoleType; }
    EmuThread* getEmuThread() { return emuThread; }
    melonDS::NDS* getNDS() { return nds; }

    MainWindow* getMainWindow() { return mainWindow; }
    int getNumWindows() { return numWindows; }
    MainWindow* getWindow(int id) { return windowList[id]; }

    void doOnAllWindows(std::function<void(MainWindow*)> func, int exclude = -1);
    void saveEnabledWindows();

    Config::Table& getGlobalConfig() { return globalCfg; }
    Config::Table& getLocalConfig() { return localCfg; }

    void broadcastCommand(int cmd, QVariant param = QVariant());
    void handleCommand(int cmd, QVariant& param);

    std::string instanceFileSuffix();

    void createWindow(int id = -1);
    void deleteWindow(int id, bool close);
    void deleteAllWindows();

    void osdAddMessage(unsigned int color, const char* fmt, ...);

    bool emuIsActive();
    void emuStop(melonDS::Platform::StopReason reason);

    bool usesOpenGL();
    void initOpenGL(int win);
    void deinitOpenGL(int win);
    void setVSyncGL(bool vsync);
    void makeCurrentGL();
    void releaseGL();

    void drawScreen();

    // return: empty string = setup OK, non-empty = error message
    QString verifySetup();

    bool updateConsole() noexcept;

    void enableCheats(bool enable);
    melonDS::ARCodeFile* getCheatFile();

    void romIcon(const melonDS::u8 (&data)[512],
                 const melonDS::u16 (&palette)[16],
                 melonDS::u32 (&iconRef)[32*32]);
    void animatedROMIcon(const melonDS::u8 (&data)[8][512],
                         const melonDS::u16 (&palette)[8][16],
                         const melonDS::u16 (&sequence)[64],
                         melonDS::u32 (&animatedIconRef)[64][32*32],
                         std::vector<int> &animatedSequenceRef);

    static const char* buttonNames[12];
    static const char* hotkeyNames[HK_MAX];

    void inputInit();
    void inputDeInit();
    void inputLoadConfig();
    void inputRumbleStart(melonDS::u32 len_ms);
    void inputRumbleStop();

    bool inputHotkeyDown(int id) { return hotkeyDown(id); }
    float inputMotionQuery(melonDS::Platform::MotionQueryType type);

    void setJoystick(int id);
    int getJoystickID() { return joystickID; }
    SDL_Joystick* getJoystick() { return joystick; }
    std::shared_ptr<SDL_mutex> getJoyMutex() { return joyMutex; }

    void touchScreen(int x, int y);
    void releaseScreen();

    // mic start/stop control from core
    void micStart();
    void micStop();
    int micReadInput(melonDS::s16* data, int maxlength);

    // called from the emu thread every frame: it, not the audio callback,
    // drains the SPU while off-speed
    void audioDrainSPU();
    bool audioOffSpeed() const;
    // emu thread. call when the stream jumps - savestate load, undo
    void audioMarkDiscontinuity();
    // emu thread. bracket a jump in the game's state - a load, an undo, a
    // reset - with the pause tail and the resume ramp, so the splice lands in
    // silence. a stream already down is left alone: begin returns false and
    // end then does nothing.
    bool audioJumpBegin();
    void audioJumpEnd(bool ramped);
    // emu thread. the achieved frame rate, for the low-pass only
    void audioSetMeasuredFPS(double fps);

    QMutex renderLock;

private:
    static int lastSep(const std::string& path);
    std::string getAssetPath(bool gba, const std::string& configpath, const std::string& ext, const std::string& file);

    QString verifyDSBIOS();
    QString verifyDSiBIOS();
    QString verifyDSFirmware();
    QString verifyDSiFirmware();
    QString verifyDSiNAND(bool isoptional);

    std::string getEffectiveFirmwareSavePath();
    void initFirmwareSaveManager() noexcept;
    std::string getSavestateName(int slot);
    bool savestateExists(int slot);
    bool loadState(const std::string& filename);
    bool saveState(const std::string& filename);
    void undoStateLoad();
    void unloadCheats();
    void loadCheats();
    std::unique_ptr<melonDS::ARM9BIOSImage> loadARM9BIOS() noexcept;
    std::unique_ptr<melonDS::ARM7BIOSImage> loadARM7BIOS() noexcept;
    std::unique_ptr<melonDS::DSiBIOSImage> loadDSiARM9BIOS() noexcept;
    std::unique_ptr<melonDS::DSiBIOSImage> loadDSiARM7BIOS() noexcept;
    melonDS::Firmware generateFirmware(int type) noexcept;
    std::optional<melonDS::Firmware> loadFirmware(int type) noexcept;
    std::optional<melonDS::DSi_NAND::NANDImage> loadNAND(const std::array<melonDS::u8, melonDS::DSiBIOSSize>& arm7ibios) noexcept;
    std::optional<melonDS::FATStorageArgs> getSDCardArgs(const std::string& key) noexcept;
    std::optional<melonDS::FATStorage> loadSDCard(const std::string& key) noexcept;
    void setBatteryLevels();
    void reset();
    bool bootToMenu(QString& errorstr);
    melonDS::u32 decompressROM(const melonDS::u8* inContent, const melonDS::u32 inSize, std::unique_ptr<melonDS::u8[]>& outContent);
    void clearBackupState();
    std::pair<std::unique_ptr<melonDS::Firmware>, std::string> generateDefaultFirmware();
    bool parseMacAddress(void* data);
    void customizeFirmware(melonDS::Firmware& firmware, bool overridesettings) noexcept;

    bool loadROMData(const QStringList& filepath, std::unique_ptr<melonDS::u8[]>& filedata, melonDS::u32& filelen, std::string& basepath, std::string& romname) noexcept;
    QString getSavErrorString(std::string& filepath, bool gba);
    bool loadROM(QStringList filepath, bool reset, QString& errorstr);
    void ejectCart();
    bool cartInserted();
    QString cartLabel();

    bool loadGBAROM(QStringList filepath, QString& errorstr);
    void loadGBAAddon(int type, QString& errorstr);
    void ejectGBACart();
    bool gbaCartInserted();
    QString gbaAddonName(int addon);
    QString gbaCartLabel();

    void audioInit();
    void audioDeInit();
    void audioEnable();
    void audioDisable();
    void updateAudioMuteByWindowFocus();
    void toggleAudioMute();
    void updateFastForwardMute(bool fastForward);
    void audioSync();
    void audioUpdateSettings();
    void audioUpdateSpeedUpSettings();
    void audioUpdateOutputSkew();
    bool audioStreamEnd();
    void audioDrainRing();

    void micOpen();
    void micClose();
    void micLoadWav(const std::string& name);
    void setupMicInputData();

    int audioGetNumSamplesOut(int outlen);
    double audioLowPassCutoff() const;
    static void audioCallback(void* data, Uint8* stream, int len);
    void audioDirectRead(melonDS::s16* stream, int len);
    void audioStretchedRead(melonDS::s16* stream, int len);
    void audioArmHandoverFade();
    void audioFinishBuffer(melonDS::s16* stream, int len, int num_in);

    int micGetNumSamplesIn(int inlen);
    void micResample(melonDS::s16* inbuf, int inlen);
    static void micCallback(void* data, Uint8* stream, int len);

    void onKeyPress(QKeyEvent* event);
    void onKeyRelease(QKeyEvent* event);
    void keyReleaseAll();

    void openJoystick();
    void closeJoystick();
    bool joystickButtonDown(int val);

    void inputProcess();

    bool hotkeyDown(int id)     { return hotkeyMask    & (1<<id); }
    bool hotkeyPressed(int id)  { return hotkeyPress   & (1<<id); }
    bool hotkeyReleased(int id) { return hotkeyRelease & (1<<id); }

    void loadRTCData();
    void saveRTCData();
    void setDateTime();
    void syncRTC();

    bool deleting;

    int instanceID;

    EmuThread* emuThread;

    MainWindow* mainWindow;
    MainWindow* windowList[kMaxWindows];
    int numWindows;

    Config::Table globalCfg;
    Config::Table localCfg;

    int consoleType;
    melonDS::NDS* nds;

    int cartType;
    std::string baseROMDir;
    std::string baseROMName;
    std::string baseAssetName;
    bool changeCart;
    std::unique_ptr<melonDS::NDSCart::CartCommon> nextCart;

    int gbaCartType;
    std::string baseGBAROMDir;
    std::string baseGBAROMName;
    std::string baseGBAAssetName;
    bool changeGBACart;
    std::unique_ptr<melonDS::GBACart::CartCommon> nextGBACart;

    // HACK
public:
    std::unique_ptr<SaveManager> ndsSave;
    std::unique_ptr<SaveManager> gbaSave;
    std::unique_ptr<SaveManager> firmwareSave;

    bool doLimitFPS;
    // the emu thread writes this, the audio thread reads it
    std::atomic<double> curFPS;
    double targetFPS;
    double fastForwardFPS;
    double slowmoFPS;
    bool fastForwardToggled;
    bool slowmoToggled;
    bool doAudioSync;
private:

    std::unique_ptr<melonDS::Savestate> backupState;
    bool savestateLoaded;

    std::unique_ptr<melonDS::ARCodeFile> cheatFile;
    bool cheatsOn;

    SDL_AudioDeviceID audioDevice;
    int audioFreq;
    int audioBufSize;
    // written from the Qt thread with the emu thread and the audio device
    // paused, read on both while running. atomic so a slip in that pausing
    // degrades to a stale read; they are independent and need no ordering.
    std::atomic<int> audioSpeedUpLowPass{kSpeedUpLowPassOff};
    std::atomic<bool> audioTimeStretchEnable{false};
    std::atomic<bool> audioLowPassEnable{false};
    // the rate actually achieved, published by the emu thread. curFPS is only
    // the frame limiter's setpoint, and with an uncapped fast-forward target it
    // is nowhere near the truth.
    std::atomic<double> audioMeasuredFPS{0.0};
    // frames handed to the stretcher, accepted or not. the audio thread derives
    // the arrival rate from this: counting only accepted frames would read a
    // saturated ring as silence and stretch the wrong way.
    std::atomic<melonDS::s64> audioOfferedFrames{0};
    AudioLowPass audioLowPass;
    AudioTimeStretch audioTimeStretch;
    bool audioStretchEngaged;
    melonDS::u32 audioLastFrame;
    double audioArrivalAvg;
    // speed the last fast-forward hold actually reached, banked at release
    // and seeding the next engage; 0 until one has happened
    double audioSpeedAchieved;
    melonDS::s64 audioLastOffered;
    double audioSampleFrac;
    melonDS::s16 audioDrainTemp[kAudioDrainMax * 2];
    bool audioDrainEngaged;
    // audio thread. audioStretchEngaged as of the last buffer, so the
    // handover is seen as an edge; the cross-fade still to run, and the
    // level it started from
    bool audioFadePrevEngaged;
    unsigned audioFadeOutFrames;
    float audioFadeOutFrom[2];
    bool audioMutedToggle;
    bool audioMutedByFastForward;
    bool audioMutedByWindowFocus;
    SDL_cond* audioSyncCond;
    SDL_mutex* audioSyncLock;
    // conceals the ends of the stream. the emu thread asks for the tail
    // through audioTailRequested and waits on audioTailSpent before pausing
    // the device; everything else is the audio thread's.
    AudioStreamRamp audioRamp;
    std::atomic<bool> audioTailRequested{false};
    std::atomic<bool> audioTailSpent{false};

    int mpAudioMode;

    bool micStarted;

    SDL_AudioDeviceID micDevice;
    int micFreq;
    int micBufSize;
    float micSampleFrac;

    melonDS::s16 micExtBuffer[4096];
    melonDS::u32 micExtBufferWritePos;
    melonDS::u32 micExtBufferCount;

    melonDS::u32 micWavLength;
    melonDS::s16* micWavBuffer;

    melonDS::s16* micBuffer;
    melonDS::u32 micBufferLength;
    melonDS::u32 micBufferReadPos;

    SDL_mutex* micLock;

    //int audioInterp;
    int audioVolume;
    bool audioDSiVolumeSync;
    int micInputType;
    std::string micDeviceName;
    std::string micWavPath;

    int keyMapping[12];
    int joyMapping[12];
    int hkKeyMapping[HK_MAX];
    int hkJoyMapping[HK_MAX];

    int joystickID;
    SDL_Joystick* joystick;
    SDL_GameController* controller;
    bool hasAccelerometer = false;
    bool hasGyroscope = false;
    bool hasRumble = false;
    bool isRumbling = false;

    static std::shared_ptr<SDL_mutex> joyMutexGlobal;
    std::shared_ptr<SDL_mutex> joyMutex;

    melonDS::u32 keyInputMask, joyInputMask;
    melonDS::u32 keyHotkeyMask, joyHotkeyMask;
    melonDS::u32 hotkeyMask, lastHotkeyMask;
    melonDS::u32 hotkeyPress, hotkeyRelease;

    melonDS::u32 inputMask;

    bool isTouching;
    melonDS::u16 touchX, touchY;

    friend class EmuThread;
    friend class MainWindow;
};

#endif //EMUINSTANCE_H
