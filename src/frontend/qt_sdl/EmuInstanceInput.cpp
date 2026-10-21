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

#include <QKeyEvent>
#include <SDL2/SDL.h>

#include "Platform.h"
#include "SDL_gamecontroller.h"
#include "SDL_sensor.h"
#include "main.h"
#include "Config.h"
#include "NDSCart/CartRetailBT.h"

// Learn with Pokémon: Typing Adventure: the cart's Bluetooth keyboard is emulated
// (CartRetailBT), but keystrokes are pushed straight into the game's input queue
// (PokeTypeKeyboard). Key bindings live in PokeTypeBindings.

using namespace melonDS;

const char* EmuInstance::buttonNames[12] =
{
    "A",
    "B",
    "Select",
    "Start",
    "Right",
    "Left",
    "Up",
    "Down",
    "R",
    "L",
    "X",
    "Y"
};

const char* EmuInstance::hotkeyNames[HK_MAX] =
{
    "HK_Lid",
    "HK_Mic",
    "HK_Pause",
    "HK_Reset",
    "HK_FastForward",
    "HK_FrameLimitToggle",
    "HK_FullscreenToggle",
    "HK_SwapScreens",
    "HK_SwapScreenEmphasis",
    "HK_SolarSensorDecrease",
    "HK_SolarSensorIncrease",
    "HK_FrameStep",
    "HK_PowerButton",
    "HK_VolumeUp",
    "HK_VolumeDown",
    "HK_AudioMuteToggle",
    "HK_SlowMo",
    "HK_FastForwardToggle",
    "HK_SlowMoToggle",
    "HK_GuitarGripGreen",
    "HK_GuitarGripRed",
    "HK_GuitarGripYellow",
    "HK_GuitarGripBlue"
};

std::shared_ptr<SDL_mutex> EmuInstance::joyMutexGlobal = nullptr;


void EmuInstance::inputInit()
{
    if (!joyMutexGlobal)
    {
        SDL_mutex* mutex = SDL_CreateMutex();
        joyMutexGlobal = std::shared_ptr<SDL_mutex>(mutex, SDL_DestroyMutex);
    }
    joyMutex = joyMutexGlobal;

    keyInputMask = 0xFFF;
    joyInputMask = 0xFFF;
    inputMask = 0xFFF;

    keyHotkeyMask = 0;
    joyHotkeyMask = 0;
    hotkeyMask = 0;
    lastHotkeyMask = 0;

    isTouching = false;
    touchX = 0;
    touchY = 0;

    joystick = nullptr;
    controller = nullptr;
    hasRumble = false;
    hasAccelerometer = false;
    hasGyroscope = false;
    isRumbling = false;

    inputLoadConfig();
}

void EmuInstance::inputDeInit()
{
    SDL_LockMutex(joyMutex.get());
    closeJoystick();
    SDL_UnlockMutex(joyMutex.get());
}

void EmuInstance::inputLoadConfig()
{
    SDL_LockMutex(joyMutex.get());

    Config::Table keycfg = localCfg.GetTable("Keyboard");
    Config::Table joycfg = localCfg.GetTable("Joystick");

    for (int i = 0; i < 12; i++)
    {
        keyMapping[i] = keycfg.GetInt(buttonNames[i]);
        joyMapping[i] = joycfg.GetInt(buttonNames[i]);
    }

    for (int i = 0; i < HK_MAX; i++)
    {
        hkKeyMapping[i] = keycfg.GetInt(hotkeyNames[i]);
        hkJoyMapping[i] = joycfg.GetInt(hotkeyNames[i]);
    }

    setJoystick(localCfg.GetInt("JoystickID"));
    SDL_UnlockMutex(joyMutex.get());
}

void EmuInstance::inputRumbleStart(melonDS::u32 len_ms)
{
    SDL_LockMutex(joyMutex.get());

    if (controller && hasRumble && !isRumbling)
    {
        SDL_GameControllerRumble(controller, 0xFFFF, 0xFFFF, len_ms);
        isRumbling = true;
    }

    SDL_UnlockMutex(joyMutex.get());
}

void EmuInstance::inputRumbleStop()
{
    SDL_LockMutex(joyMutex.get());

    if (controller && hasRumble && isRumbling)
    {
        SDL_GameControllerRumble(controller, 0, 0, 0);
        isRumbling = false;
    }

    SDL_UnlockMutex(joyMutex.get());
}

float EmuInstance::inputMotionQuery(melonDS::Platform::MotionQueryType type)
{
    float values[3];
    SDL_LockMutex(joyMutex.get());
    if (type <= melonDS::Platform::MotionAccelerationZ)
    {
        if (controller && hasAccelerometer)
        {
            if (SDL_GameControllerGetSensorData(controller, SDL_SENSOR_ACCEL, values, 3) == 0)
            {
                // Map values from DS console orientation to SDL controller orientation.
                SDL_UnlockMutex(joyMutex.get());
                switch (type)
                {
                case melonDS::Platform::MotionAccelerationX:
                    return values[0];
                case melonDS::Platform::MotionAccelerationY:
                    return -values[2];
                case melonDS::Platform::MotionAccelerationZ:
                    return values[1];
                default:
                    break;
                }
            }
        }
    }
    else if (type <= melonDS::Platform::MotionRotationZ)
    {
        if (controller && hasGyroscope)
        {
            if (SDL_GameControllerGetSensorData(controller, SDL_SENSOR_GYRO, values, 3) == 0)
            {
                // Map values from DS console orientation to SDL controller orientation.
                SDL_UnlockMutex(joyMutex.get());
                switch (type)
                {
                case melonDS::Platform::MotionRotationX:
                    return values[0];
                case melonDS::Platform::MotionRotationY:
                    return -values[2];
                case melonDS::Platform::MotionRotationZ:
                    return values[1];
                default:
                    break;
                }
            }
        }
    }
    SDL_UnlockMutex(joyMutex.get());
    if (type == melonDS::Platform::MotionAccelerationZ)
        return SDL_STANDARD_GRAVITY;
    return 0.0f;
}


void EmuInstance::setJoystick(int id)
{
    SDL_LockMutex(joyMutex.get());
    joystickID = id;
    openJoystick();
    SDL_UnlockMutex(joyMutex.get());
}

void EmuInstance::openJoystick()
{
    if (controller) SDL_GameControllerClose(controller);

    if (joystick) SDL_JoystickClose(joystick);

    int num = SDL_NumJoysticks();
    if (num < 1)
    {
        controller = nullptr;
        joystick = nullptr;
        hasRumble = false;
        hasAccelerometer = false;
        hasGyroscope = false;
        return;
    }

    if (joystickID >= num)
        joystickID = 0;

    joystick = SDL_JoystickOpen(joystickID);

    if (SDL_IsGameController(joystickID))
    {
        controller = SDL_GameControllerOpen(joystickID);
    }

    if (controller)
    {
        if (SDL_GameControllerHasRumble(controller))
        {
            hasRumble = true;
        }
        if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL))
        {
            hasAccelerometer = SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, SDL_TRUE) == 0;
        }
        if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO))
        {
            hasGyroscope = SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE) == 0;
        }
    }
}

void EmuInstance::closeJoystick()
{
    if (controller)
    {
        SDL_GameControllerClose(controller);
        controller = nullptr;
        hasRumble = false;
        hasAccelerometer = false;
        hasGyroscope = false;
    }
    if (joystick)
    {
        SDL_JoystickClose(joystick);
        joystick = nullptr;
    }
}


// distinguish between left and right modifier keys (Ctrl, Alt, Shift)
// Qt provides no real cross-platform way to do this, so here we go
// for Windows and Linux we can distinguish via scancodes (but both
// provide different scancodes)
bool isRightModKey(QKeyEvent* event)
{
#ifdef __WIN32__
    quint32 scan = event->nativeScanCode();
    return (scan == 0x11D || scan == 0x138 || scan == 0x36);
#elif __APPLE__
    quint32 scan = event->nativeVirtualKey();
    return (scan == 0x36 || scan == 0x3C || scan == 0x3D || scan == 0x3E);
#else
    quint32 scan = event->nativeScanCode();
    return (scan == 0x69 || scan == 0x6C || scan == 0x3E);
#endif
}

int getEventKeyVal(QKeyEvent* event)
{
    int key = event->key();
    int mod = event->modifiers();
    bool ismod = (key == Qt::Key_Control ||
                  key == Qt::Key_Alt ||
                  key == Qt::Key_AltGr ||
                  key == Qt::Key_Shift ||
                  key == Qt::Key_Meta);

    if (!ismod)
        key |= mod;
    else if (isRightModKey(event))
        key |= (1<<31);

    return key;
}


// The character one binding produces under the modifiers currently held.
melonDS::u16 EmuInstance::pokeTypeCharFor(melonDS::PokeTypeKeyboard::Region region,
                                          melonDS::u16 keyid, QKeyEvent* event)
{
    // modifiers, arrows and control keys use the game's own special codes
    if (melonDS::u16 special = melonDS::PokeTypeKeyboard::SpecialCharForKeyID(keyid))
        return special;

    // use the game's layout, not the host's: that is the point of positional bindings
    melonDS::u32 count = 0;
    const melonDS::PokeTypeKeyboard::KeyDesc* table =
        melonDS::PokeTypeKeyboard::GetKeyTable(region, count);
    if (!table) return 0;

    for (melonDS::u32 i = 0; i < count; i++)
    {
        if (table[i].KeyID != keyid) continue;

        bool shift = event->modifiers() & Qt::ShiftModifier;
        bool altgr = event->modifiers() & Qt::GroupSwitchModifier;

        // Qt doesn't report Caps Lock state; the game tracks its own from
        // the forwarded Caps Lock key
        return melonDS::PokeTypeKeyboard::CharForKey(table[i], shift, altgr, false);
    }

    return 0;
}

bool EmuInstance::handlePokeTypeKey(QKeyEvent* event)
{
    if (!pokeTypeKeyboardSupported()) return false;
    if (!localCfg.GetBool("PokeType.Enabled")) return false;

    // The release key may be a chord and never reaches the game. key() is 0 for
    // compose sequences, so an unset release key must not match.
    int keyChord = getEventKeyVal(event);
    if (pokeTypeBindings.releaseKeyBound() && keyChord == pokeTypeBindings.releaseKey)
    {
        pokeTypeGrabbed = !pokeTypeGrabbed;
        if (!pokeTypeGrabbed) pokeTypeReleaseDpad(pokeTypeDpadHeld);
        osdAddMessage(0, pokeTypeGrabbed ? "Typing keyboard: capturing"
                                         : "Typing keyboard: released");
        return true;
    }

    if (!pokeTypeGrabbed) return false;

    // Bindings match on the bare key, so Shift+A still reaches the A binding.
    int keyBare = keyChord;
    if (event->modifiers() != Qt::KeypadModifier)
        keyBare &= ~event->modifiers();

    auto region = pokeTypeCartRegion.load();
    melonDS::u8 mods = PokeTypeBindings::hidMods(event->modifiers());

    int mode = pokeTypeBindings.mode;

    // a shifted symbol names no key, so leave it to the host layout below
    melonDS::u16 keyid = PokeTypeBindings::isShiftedSymbol(keyBare, event->modifiers())
                       ? 0 : pokeTypeBindings.keyIDFor(region, keyBare);

    // The game's registration prompt asks to turn the keyboard on while holding
    // Fn, which makes a real keyboard discoverable. Until the game has
    // connected, Fn does that for the emulated one; afterwards it types as usual.
    constexpr melonDS::u16 FnKeyID = 0x75;
    if (keyid == FnKeyID)
    {
        pokeTypeQueueKey({pokeTypeCharFor(region, keyid, event), (melonDS::u8)keyid, mods, true});
        return true;
    }

    // The game's menus read only the D-pad, never keyboard arrow events. Opt-in,
    // as it isn't known whether a real keyboard's arrows move them: an arrow
    // binding also presses its D-pad bit, released in onKeyRelease.
    int dpadBit = PokeTypeBindings::dpadBitForKeyID(keyid);
    if (dpadBit >= 0 && localCfg.GetBool("PokeType.ArrowsToDpad"))
    {
        keyInputMask &= ~(1u << dpadBit);
        pokeTypeDpadHeld |= (1u << dpadBit);
    }

    if (keyid != 0)
    {
        // keys with no host text (modifiers, arrows, Enter, Backspace, Tab)
        // go through the bindings in every mode, layout mode included
        bool special = melonDS::PokeTypeKeyboard::SpecialCharForKeyID(keyid) != 0;

        if (special || mode != PokeTypeBindings::ModeLayout)
        {
            melonDS::u16 ch = pokeTypeCharFor(region, keyid, event);
            if (ch != 0)
            {
                pokeTypeQueueKey({ch, (melonDS::u8)keyid, mods, false});
                return true;
            }

            // nothing on the level being held (e.g. AltGr): let the host layout try
        }
    }

    // Positional mode consults the bindings and nothing else.
    if (mode == PokeTypeBindings::ModePositional)
        return true;

    QString text = event->text();
    if (!text.isEmpty())
    {
        melonDS::u16 ch = text.at(0).unicode();
        if (ch >= 0x20 && ch != 0x7F)
            pokeTypeQueueKey({PokeTypeBindings::flipLetterCase(ch), 0, mods, false});
    }

    // while grabbed, swallow every key rather than let it drive DS buttons and
    // hotkeys mid-sentence; the release key is the way out
    return true;
}

void EmuInstance::pokeTypeQueueKey(const PokeTypeKey& key)
{
    pokeTypeKeyLock.lock();
    pokeTypeKeys.push_back(key);
    pokeTypeKeyLock.unlock();
}

void EmuInstance::pokeTypeApplyInput()
{
    if (pokeTypeAutoPairDirty.exchange(false))
        pokeTypeApplyAutoPair();

    std::vector<PokeTypeKey> keys;
    pokeTypeKeyLock.lock();
    keys.swap(pokeTypeKeys);
    pokeTypeKeyLock.unlock();

    if (!nds) return;

    for (const PokeTypeKey& key : keys)
    {
        if (key.pairingGesture)
        {
            auto* bt = dynamic_cast<NDSCart::CartRetailBT*>(nds->GetNDSCart());
            if (bt && bt->EnterPairingMode())
                continue;

            // the game has connected, so Fn types, if it has a character here
            if (key.character == 0)
                continue;
        }

        if (key.keyID != 0)
            nds->PokeTypeKeyboard.PushKeyID(key.character, key.keyID, key.mods);
        else
            nds->PokeTypeKeyboard.PushKey(key.character, key.mods);
    }
}

void EmuInstance::onKeyPress(QKeyEvent* event)
{
    // while the game is taking dictation, don't also drive the DS buttons
    if (handlePokeTypeKey(event))
        return;

    int keyHK = getEventKeyVal(event);
    int keyKP = keyHK;
    if (event->modifiers() != Qt::KeypadModifier)
        keyKP &= ~event->modifiers();

    for (int i = 0; i < 12; i++)
        if (keyKP == keyMapping[i])
            keyInputMask &= ~(1<<i);

    for (int i = 0; i < HK_MAX; i++)
        if (keyHK == hkKeyMapping[i])
            keyHotkeyMask |= (1<<i);
}

void EmuInstance::pokeTypeDpadKeyReleased(QKeyEvent* event)
{
    if (!pokeTypeDpadHeld) return;

    // cart gone with a key still down: nothing to resolve against, so release all
    if (!pokeTypeKeyboardSupported())
    {
        pokeTypeReleaseDpad(pokeTypeDpadHeld);
        return;
    }

    // Same resolution as the press: the bare key, in the loaded region.
    int keyBare = getEventKeyVal(event);
    if (event->modifiers() != Qt::KeypadModifier)
        keyBare &= ~event->modifiers();

    melonDS::u16 keyid = pokeTypeBindings.keyIDFor(pokeTypeCartRegion.load(), keyBare);
    int dpadBit = PokeTypeBindings::dpadBitForKeyID(keyid);
    if (dpadBit >= 0)
        pokeTypeReleaseDpad(1u << dpadBit);
}

void EmuInstance::pokeTypeReleaseDpad(melonDS::u32 bits)
{
    bits &= pokeTypeDpadHeld;
    keyInputMask |= bits;
    pokeTypeDpadHeld &= ~bits;
}

void EmuInstance::onKeyRelease(QKeyEvent* event)
{
    // not gated on the grab or the setting: either may have changed since the press
    pokeTypeDpadKeyReleased(event);

    int keyHK = getEventKeyVal(event);
    int keyKP = keyHK;
    if (event->modifiers() != Qt::KeypadModifier)
        keyKP &= ~event->modifiers();

    for (int i = 0; i < 12; i++)
        if (keyKP == keyMapping[i])
            keyInputMask |= (1<<i);

    for (int i = 0; i < HK_MAX; i++)
        if (keyHK == hkKeyMapping[i])
            keyHotkeyMask &= ~(1<<i);
}

void EmuInstance::keyReleaseAll()
{
    keyInputMask = 0xFFF;
    keyHotkeyMask = 0;
    pokeTypeDpadHeld = 0;
}

bool EmuInstance::joystickButtonDown(int val)
{
    if (val == -1) return false;

    bool hasbtn = ((val & 0xFFFF) != 0xFFFF);

    if (hasbtn)
    {
        if (val & 0x100)
        {
            int hatnum = (val >> 4) & 0xF;
            int hatdir = val & 0xF;
            Uint8 hatval = SDL_JoystickGetHat(joystick, hatnum);

            bool pressed = false;
            if      (hatdir == 0x1) pressed = (hatval & SDL_HAT_UP);
            else if (hatdir == 0x4) pressed = (hatval & SDL_HAT_DOWN);
            else if (hatdir == 0x2) pressed = (hatval & SDL_HAT_RIGHT);
            else if (hatdir == 0x8) pressed = (hatval & SDL_HAT_LEFT);

            if (pressed) return true;
        }
        else
        {
            int btnnum = val & 0xFFFF;
            Uint8 btnval = SDL_JoystickGetButton(joystick, btnnum);

            if (btnval) return true;
        }
    }

    if (val & 0x10000)
    {
        int axisnum = (val >> 24) & 0xF;
        int axisdir = (val >> 20) & 0xF;
        Sint16 axisval = SDL_JoystickGetAxis(joystick, axisnum);

        switch (axisdir)
        {
            case 0: // positive
                if (axisval > 16384) return true;
                break;

            case 1: // negative
                if (axisval < -16384) return true;
                break;

            case 2: // trigger
                if (axisval > 0) return true;
                break;
        }
    }

    return false;
}

void EmuInstance::inputProcess()
{
    SDL_LockMutex(joyMutex.get());
    SDL_JoystickUpdate();

    if (joystick)
    {
        if (!SDL_JoystickGetAttached(joystick))
        {
            SDL_JoystickClose(joystick);
            joystick = nullptr;
        }
    }
    if (!joystick && (SDL_NumJoysticks() > 0))
    {
        openJoystick();
    }

    joyInputMask = 0xFFF;
    if (joystick)
    {
        for (int i = 0; i < 12; i++)
            if (joystickButtonDown(joyMapping[i]))
                joyInputMask &= ~(1 << i);
    }

    inputMask = keyInputMask & joyInputMask;

    joyHotkeyMask = 0;
    if (joystick)
    {
        for (int i = 0; i < HK_MAX; i++)
            if (joystickButtonDown(hkJoyMapping[i]))
                joyHotkeyMask |= (1 << i);
    }

    hotkeyMask = keyHotkeyMask | joyHotkeyMask;
    hotkeyPress = hotkeyMask & ~lastHotkeyMask;
    hotkeyRelease = lastHotkeyMask & ~hotkeyMask;
    lastHotkeyMask = hotkeyMask;
    SDL_UnlockMutex(joyMutex.get());

    pokeTypeApplyInput();
}

void EmuInstance::touchScreen(int x, int y)
{
    touchX = x;
    touchY = y;
    isTouching = true;
}

void EmuInstance::releaseScreen()
{
    isTouching = false;
}
