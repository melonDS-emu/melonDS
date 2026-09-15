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

// Keystrokes for Learn with Pokémon: Typing Adventure. The keyboard's Bluetooth
// link is emulated by CartRetailBT, but keys don't travel over it: they are written
// straight into the 16-entry ring buffer in ARM9 memory that the game's driver fills
// with decoded keystrokes and the game drains once per frame.
//
// Queue layout (at QueueAddr):
//   +0x00 .. +0x3C   16 u32 event slots
//   +0x40            read index
//   +0x44            write index
// An event is (mods << 24) | (HID usage code << 16) | character, except on the
// Japanese build, which stores the bare character.
class PokeTypeKeyboard
{
public:
    struct CharKeyPair
    {
        u16 Character;
        u16 KeyID;
    };

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

    struct GameInfo
    {
        u32 GameCode;
        u32 GetInputAddr;
        u32 QueueAddr;
        u32 StateAddr;
        const CharKeyPair* Map;
        u32 MapLength;
        const KeyDesc* Layout;
        u32 LayoutLength;
        Region Reg;
    };

    explicit PokeTypeKeyboard(melonDS::NDS& nds) : NDS(nds) {}

    void Reset() noexcept { Game = nullptr; }

    /// Returns true if the cart is one of the supported builds.
    bool SetGameCode(u32 gamecode) noexcept;

    [[nodiscard]] bool IsSupportedGame() const noexcept { return Game != nullptr; }

    /// Whether the input queue is live, checked against the dequeue routine's
    /// bytes so an unknown build fails closed instead of corrupting memory.
    [[nodiscard]] bool IsReady() noexcept;

    /// HID usage code for a character in this build's typing map, or 0xFF.
    [[nodiscard]] u8 FindKeyID(u16 character) const noexcept;

    /// Three-letter tag used in config paths. "" for Region::MAX.
    [[nodiscard]] static const char* RegionCode(Region r) noexcept;

    /// The key set for a region, sorted by key ID. nullptr for Region::MAX.
    [[nodiscard]] static const KeyDesc* GetKeyTable(Region r, u32& count) noexcept;

    /// The character -> key ID table for a region. nullptr for Japan, whose
    /// events carry a bare character.
    [[nodiscard]] static const CharKeyPair* GetTypingMap(Region r, u32& count) noexcept;

    /// The character a non-printing key (modifier, arrow, Enter, Backspace, Tab,
    /// Space) sends, or 0. Japan's table has no modifier or Fn rows, so this is
    /// how those are reached.
    [[nodiscard]] static u16 SpecialCharForKeyID(u16 keyid) noexcept;

    /// The character a key produces under the given modifiers, following the
    /// game's decoder at UZPP 0x0205B00C.
    [[nodiscard]] static u16 CharForKey(const KeyDesc& key, bool shift, bool altgr,
                                        bool caps) noexcept;

    /// Region of the loaded cart, or Region::MAX when none is loaded.
    [[nodiscard]] Region GetRegion() const noexcept;

    /// Returns false if the game isn't ready, the character isn't in this
    /// build's typing map, or the queue is full.
    bool PushKey(u16 character, u8 mods = 0) noexcept;

    /// mods is the HID modifier byte (bit 0 LCtrl, 1 LShift, 2 LAlt, 6 RAlt).
    /// The frontend only sets left-hand bits; which Shift is held reaches the
    /// game through the key ID.
    bool PushKeyID(u16 character, u8 keyid, u8 mods = 0) noexcept;

private:
    [[nodiscard]] bool GameHasWirelessKeyboard() const noexcept;

    const GameInfo* Game = nullptr;
    melonDS::NDS& NDS;
};

}

#endif // POKETYPEKEYBOARD_H
