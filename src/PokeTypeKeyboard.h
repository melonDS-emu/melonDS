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

#ifndef POKETYPEKEYBOARD_H
#define POKETYPEKEYBOARD_H

#include "types.h"

namespace melonDS
{
class NDS;

namespace NDSCart { class CartRetailBT; }

// Learn with Pokémon: Typing Adventure and the Bluetooth keyboard it came with.
// CartRetailBT emulates the keyboard and its link to the cart, and the game decodes
// the keyboard's input reports itself. This finds that keyboard for a frontend, and
// describes the one each build was sold with, so host keys can be mapped onto it.
//
// Keys go by the game's IDs, as BTKeyboard takes them: the HID usage code for most
// keys, and 0x70..0x76 for Ctrl, Right Shift, Left Shift, Alt, Home, Fn and AltGr.
class PokeTypeKeyboard
{
public:
    enum class Region { Europe, France, Germany, Italy, Spain, Japan, MAX };

    /// One key's character on each of the four shift levels (0 = unused), and
    /// whether Caps Lock swaps the first two.
    struct KeyDesc
    {
        u16 KeyID;
        u16 Base;
        u16 Shift;
        u16 AltGr;
        u16 ShiftAltGr;
        u8  CapsSensitive;
    };

    /// A key, and the modifiers held to type one character with it.
    struct Keystroke
    {
        u8 KeyID;
        bool Shift;
        bool AltGr;
    };

    explicit PokeTypeKeyboard(melonDS::NDS& nds) : NDS(nds) {}

    void Reset() noexcept { Reg = Region::MAX; }

    /// Returns true if the cart is one of the supported builds.
    bool SetGameCode(u32 gamecode) noexcept;

    [[nodiscard]] bool IsSupportedGame() const noexcept { return Reg != Region::MAX; }

    /// Region of the loaded cart, or Region::MAX when none is loaded.
    [[nodiscard]] Region GetRegion() const noexcept { return Reg; }

    /// Press or release one of the keyboard's keys. Before the game has connected,
    /// pressing Fn makes the keyboard discoverable instead, the gesture the game's
    /// registration prompt asks for. Does nothing without a Typing Adventure cart.
    void SetKey(u8 keyid, bool down) noexcept;

    /// Press and release a key, holding Shift and AltGr only if it needs them.
    void TypeKey(const Keystroke& keystroke) noexcept;

    /// Let go of every key.
    void ReleaseKeys() noexcept;

    /// Three-letter tag used in config paths. "" for Region::MAX.
    [[nodiscard]] static const char* RegionCode(Region r) noexcept;

    /// The key set for a region, sorted by key ID. nullptr for Region::MAX.
    [[nodiscard]] static const KeyDesc* GetKeyTable(Region r, u32& count) noexcept;

    /// The character a non-printing key (modifier, arrow, Enter, Backspace, Tab,
    /// Space) types, or 0. Japan's table has no modifier or Fn rows, so this is
    /// how those are reached.
    [[nodiscard]] static u16 SpecialCharForKeyID(u16 keyid) noexcept;

    /// The character a key produces under the given modifiers, following the
    /// game's decoder at UZPP 0x0205B00C.
    [[nodiscard]] static u16 CharForKey(const KeyDesc& key, bool shift, bool altgr,
                                        bool caps) noexcept;

    /// The keystroke that types a character on a region's keyboard with Caps Lock
    /// off. Returns false if no key types it.
    static bool FindKeystroke(Region r, u16 character, Keystroke& out) noexcept;

private:
    [[nodiscard]] NDSCart::CartRetailBT* Cart() const noexcept;

    Region Reg = Region::MAX;
    melonDS::NDS& NDS;
};

}

#endif // POKETYPEKEYBOARD_H
