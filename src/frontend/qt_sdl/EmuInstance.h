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

#include <atomic>
#include <vector>

#include <SDL2/SDL.h>

#include "Platform.h"
#include "main.h"
#include "NDS.h"
#include "EmuThread.h"
#include "Window.h"
#include "Config.h"
#include "SaveManager.h"
#include "PokeTypeBindings.h"

const int kMaxWindows = 4;

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

    QMutex renderLock;

    PokeTypeBindings pokeTypeBindings;

    /// Whether the binding tables belong in this instance's config: a
    /// supported cart is inserted, or bindings are already saved.
    [[nodiscard]] bool pokeTypeBindingsInConfig();

    /// Fill pokeTypeBindings from localCfg, if pokeTypeBindingsInConfig().
    void pokeTypeLoadBindings();

    /// Write pokeTypeBindings to localCfg and save. Does nothing unless
    /// pokeTypeBindingsInConfig() or userEdited, so an untouched dialog leaves
    /// the config unchanged.
    void pokeTypeSaveBindings(bool userEdited = false);

    /// "PokeType.AutoSendFn" changed; the emulator thread passes it on to the cart.
    void pokeTypeAutoPairChanged() { pokeTypeAutoPairDirty = true; }

    /// Region of the inserted cart, or Region::MAX when it isn't a supported build.
    [[nodiscard]] melonDS::PokeTypeKeyboard::Region pokeTypeRegion() const { return pokeTypeCartRegion; }

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

    /// Apply settings that need the cart object itself. Call only once a cart
    /// is actually in the slot (end of updateConsole(), or loadROM()'s direct
    /// SetNDSCart()), not while it is merely queued in nextCart.
    void applyCartDependentSettings();

    bool pokeTypeKeyboardSupported();
    bool pokeTypeKeyboardActive();

    // Key events arrive on the UI thread, but the console belongs to the emulator
    // thread. The UI thread keeps the bindings and queues what to type; the
    // emulator thread publishes the cart's region and types the queue.

    /// Region of the inserted cart, or Region::MAX when it isn't a supported build.
    std::atomic<melonDS::PokeTypeKeyboard::Region> pokeTypeCartRegion {melonDS::PokeTypeKeyboard::Region::MAX};
    void pokeTypePublishCart();

    struct PokeTypeKey
    {
        melonDS::u16 character;
        melonDS::u8 keyID;          // 0: look the key up from the character
        melonDS::u8 mods;
        bool pairingGesture;        // Fn, which pairs the keyboard until the game has connected
    };

    QMutex pokeTypeKeyLock;
    std::vector<PokeTypeKey> pokeTypeKeys;
    std::atomic<bool> pokeTypeAutoPairDirty {false};

    void pokeTypeQueueKey(const PokeTypeKey& key);

    /// Emulator thread: type the queued keys and pass on a changed auto-pair setting.
    void pokeTypeApplyInput();

    /// Push "PokeType.AutoSendFn" into the inserted cart: whether the emulated
    /// keyboard is discoverable from power-on, as if Fn were held. Safe to call
    /// with no cart, or a cart without a Bluetooth keyboard. Emulator thread only.
    void pokeTypeApplyAutoPair();

    /// UI thread, once a cart is in: reload the bindings and grab the keyboard.
    void pokeTypeCartInserted();

    /// Whether keystrokes go to the game rather than the emulator; the release
    /// key toggles it.
    bool pokeTypeGrabbed = true;

    /// KEYINPUT bits held by arrow bindings. Releases don't go through
    /// handlePokeTypeKey, so remember exactly what was pressed.
    melonDS::u32 pokeTypeDpadHeld = 0;

    void pokeTypeDpadKeyReleased(QKeyEvent* event);

    /// Let go of these D-pad bits, restricted to the ones arrow bindings hold.
    void pokeTypeReleaseDpad(melonDS::u32 bits);

    melonDS::u16 pokeTypeCharFor(melonDS::PokeTypeKeyboard::Region region,
                                 melonDS::u16 keyid, QKeyEvent* event);

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

    void micOpen();
    void micClose();
    void micLoadWav(const std::string& name);
    void setupMicInputData();

    int audioGetNumSamplesOut(int outlen);
    static void audioCallback(void* data, Uint8* stream, int len);

    int micGetNumSamplesIn(int inlen);
    void micResample(melonDS::s16* inbuf, int inlen);
    static void micCallback(void* data, Uint8* stream, int len);

    void onKeyPress(QKeyEvent* event);
    void onKeyRelease(QKeyEvent* event);

    /// Feed a keystroke to Typing Adventure. Returns true when it was consumed.
    bool handlePokeTypeKey(QKeyEvent* event);
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
    double curFPS;
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
    float audioSampleFrac;
    bool audioMutedToggle;
    bool audioMutedByFastForward;
    bool audioMutedByWindowFocus;
    SDL_cond* audioSyncCond;
    SDL_mutex* audioSyncLock;

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
