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

#ifndef POKETYPEBINDINGS_H
#define POKETYPEBINDINGS_H

#include <string>
#include <vector>

#include <QtCore/qnamespace.h>

#include "PokeTypeKeyboard.h"

// Host key bindings for the Learn with Pokémon: Typing Adventure keyboard, per
// region since each build ships a different keyboard. Stored as Qt key codes
// OR'd with modifiers, as KeyMapButton writes them: 0 means never set (use the
// default), -1 deliberately unbound.
class PokeTypeBindings
{
public:
    using Region = melonDS::PokeTypeKeyboard::Region;

    enum
    {
        ModeLayout      = 0,    // characters follow the host layout
        ModeOverrides   = 1,    // as above, but bound keys win
        ModePositional  = 2,    // bindings only
    };

    static constexpr int NumRegions = (int)Region::MAX;
    // key IDs are one-byte HID usage codes
    static constexpr int MaxKeys = 256;

    int mode = ModeOverrides;
    int releaseKey = Qt::Key_F12;

    /// Every key ID a region has, ascending: the sixteen special keys plus the
    /// layout table's keys. Japan's table carries only nine of the special keys.
    [[nodiscard]] static std::vector<melonDS::u16> keyIDs(Region r);

    [[nodiscard]] bool releaseKeyBound() const
    {
        return releaseKey != 0 && releaseKey != -1;
    }

    /// The built-in host key for one DS key, or 0 when there is no obvious one.
    [[nodiscard]] static int defaultBinding(Region r, melonDS::u16 keyid);

    /// Fold a host key code to one canonical form so bindings and keystrokes
    /// compare equal: Latin-1 letters to uppercase, since which case Qt reports
    /// for them varies by platform, and plain accent characters to dead keys.
    [[nodiscard]] static int normaliseHostKey(int key)
    {
        // some setups report the plain accent character instead of the dead key
        if (key == 0xB4) return Qt::Key_Dead_Acute;
        if (key == 0xA8) return Qt::Key_Dead_Diaeresis;

        // 0xF7 is the division sign, not a letter
        if (key >= 0xE0 && key <= 0xFE && key != 0xF7)
            return key - 0x20;

        return key;
    }

    /// Row label: special keys have fixed names, others show their characters.
    [[nodiscard]] static std::string label(Region r, melonDS::u16 keyid);

    /// The stored value, 0 when never set.
    [[nodiscard]] int rawBinding(Region r, melonDS::u16 keyid) const;

    /// The stored value, or the default when never set.
    [[nodiscard]] int binding(Region r, melonDS::u16 keyid) const;

    void setBinding(Region r, melonDS::u16 keyid, int hostkey);

    /// The DS key bound to a host key, or 0 if none. When two keys share a
    /// host key the lowest key ID wins.
    [[nodiscard]] melonDS::u16 keyIDFor(Region r, int hostkey) const;

    void resetRegion(Region r);

    /// The game's keyboard reports letters the other way up from a host one:
    /// a bare A carries 'A', and Shift+A carries 'a'.
    [[nodiscard]] static melonDS::u16 flipLetterCase(melonDS::u16 ch);

    /// The HID modifier bitmask, which the game puts in the event's top byte.
    [[nodiscard]] static melonDS::u8 hidMods(Qt::KeyboardModifiers mods);

    /// Whether Shift turned the key code into the symbol it produced (Qt
    /// reports Shift+2 on a US layout as Key_At), so it names a character
    /// rather than a key and cannot be looked up in the bindings.
    [[nodiscard]] static bool isShiftedSymbol(int key, Qt::KeyboardModifiers mods);

    /// The KEYINPUT bit an arrow key ID also presses when "Arrow keys also
    /// press the D-pad" is on, or -1. The game's menus only read the D-pad.
    [[nodiscard]] static int dpadBitForKeyID(melonDS::u16 keyid);

    /// A UTF-8 label for a dead key, which QKeySequence has no name for.
    /// "" for any other key.
    [[nodiscard]] static std::string deadKeyLabel(int key);

private:
    int raw[NumRegions][MaxKeys] = {};

    [[nodiscard]] static int slotFor(Region r, melonDS::u16 keyid);

    [[nodiscard]] static bool isAliasKey(Region r, melonDS::u16 keyid);
};

#endif // POKETYPEBINDINGS_H
