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

#include "PokeTypeKeyboard.h"
#include "NDS.h"
#include "NDSCart/CartRetailBT.h"

namespace melonDS
{

extern const PokeTypeKeyboard::KeyDesc PokeTypeLayout_EUR[];
extern const u32 PokeTypeLayout_EUR_Length;
extern const PokeTypeKeyboard::KeyDesc PokeTypeLayout_FRA[];
extern const u32 PokeTypeLayout_FRA_Length;
extern const PokeTypeKeyboard::KeyDesc PokeTypeLayout_GER[];
extern const u32 PokeTypeLayout_GER_Length;
extern const PokeTypeKeyboard::KeyDesc PokeTypeLayout_ITA[];
extern const u32 PokeTypeLayout_ITA_Length;
extern const PokeTypeKeyboard::KeyDesc PokeTypeLayout_SPA[];
extern const u32 PokeTypeLayout_SPA_Length;
extern const PokeTypeKeyboard::KeyDesc PokeTypeLayout_JPN[];
extern const u32 PokeTypeLayout_JPN_Length;

struct GameInfo
{
    u32 GameCode;
    const PokeTypeKeyboard::KeyDesc* Layout;
    u32 LayoutLength;
    PokeTypeKeyboard::Region Reg;
};

#define LAYOUT(r) PokeTypeLayout_##r, PokeTypeLayout_##r##_Length

static const GameInfo GameTable[] =
{
    {0x50505A55, LAYOUT(EUR), PokeTypeKeyboard::Region::Europe},  // UZPP Europe
    {0x46505A55, LAYOUT(FRA), PokeTypeKeyboard::Region::France},  // UZPF France
    {0x44505A55, LAYOUT(GER), PokeTypeKeyboard::Region::Germany}, // UZPD Germany
    {0x49505A55, LAYOUT(ITA), PokeTypeKeyboard::Region::Italy},   // UZPI Italy
    {0x53505A55, LAYOUT(SPA), PokeTypeKeyboard::Region::Spain},   // UZPS Spain
    {0x4A505A55, LAYOUT(JPN), PokeTypeKeyboard::Region::Japan},   // UZPJ Japan
};

#undef LAYOUT


bool PokeTypeKeyboard::SetGameCode(u32 gamecode) noexcept
{
    Reg = Region::MAX;

    for (const GameInfo& info : GameTable)
    {
        if (info.GameCode == gamecode)
        {
            Reg = info.Reg;
            break;
        }
    }

    return Reg != Region::MAX;
}

NDSCart::CartRetailBT* PokeTypeKeyboard::Cart() const noexcept
{
    NDSCart::CartCommon* cart = NDS.GetNDSCart();
    if (!cart || cart->Type() != NDSCart::CartType::RetailBT) return nullptr;

    return static_cast<NDSCart::CartRetailBT*>(cart);
}

void PokeTypeKeyboard::SetKey(u8 keyid, bool down) noexcept
{
    if (NDSCart::CartRetailBT* cart = Cart())
        cart->SetKey(keyid, down);
}

void PokeTypeKeyboard::TypeKey(const Keystroke& keystroke) noexcept
{
    if (NDSCart::CartRetailBT* cart = Cart())
        cart->TypeKey(keystroke.KeyID, keystroke.Shift, keystroke.AltGr);
}

void PokeTypeKeyboard::ReleaseKeys() noexcept
{
    if (NDSCart::CartRetailBT* cart = Cart())
        cart->ReleaseKeys();
}

// Keys with no printable character: private-range codes for the modifiers, PC
// extended scancodes for the arrows, control characters for the rest. The same
// in all five European tables and in what the Japanese decoder produces.
static const struct
{
    u16 Character;
    u16 KeyID;
} SpecialKeys[] =
{
    {0x000D,0x28}, {0x0008,0x2A}, {0x0009,0x2B}, {0x0020,0x2C},
    {0xFF06,0x39}, {0xE04D,0x4F}, {0xE04B,0x50}, {0xE050,0x51},
    {0xE048,0x52}, {0xFF02,0x70}, {0xFF01,0x71}, {0xFF00,0x72},
    {0xFF04,0x73}, {0xFF08,0x74}, {0xFF07,0x75}, {0xFF0B,0x76},
};

const char* PokeTypeKeyboard::RegionCode(Region r) noexcept
{
    switch (r)
    {
    case Region::Europe:  return "EUR";
    case Region::France:  return "FRA";
    case Region::Germany: return "GER";
    case Region::Italy:   return "ITA";
    case Region::Spain:   return "SPA";
    case Region::Japan:   return "JPN";
    default:              return "";
    }
}

const PokeTypeKeyboard::KeyDesc* PokeTypeKeyboard::GetKeyTable(Region r, u32& count) noexcept
{
    for (const GameInfo& info : GameTable)
    {
        if (info.Reg == r)
        {
            count = info.LayoutLength;
            return info.Layout;
        }
    }

    count = 0;
    return nullptr;
}

u16 PokeTypeKeyboard::SpecialCharForKeyID(u16 keyid) noexcept
{
    for (const auto& pair : SpecialKeys)
        if (pair.KeyID == keyid)
            return pair.Character;

    return 0;
}

// Caps Lock only swaps base and shift on keys marked caps-sensitive (the letters).
u16 PokeTypeKeyboard::CharForKey(const KeyDesc& key, bool shift, bool altgr,
                                 bool caps) noexcept
{
    bool swap = caps && key.CapsSensitive;

    if (shift && altgr) return key.ShiftAltGr;
    if (altgr)          return key.AltGr;
    if (shift)          return swap ? key.Base : key.Shift;

    return swap ? key.Shift : key.Base;
}

// Where a character is on two keys, or two levels of one, the fewest modifiers
// win, then the first key.
bool PokeTypeKeyboard::FindKeystroke(Region r, u16 character, Keystroke& out) noexcept
{
    if (character == 0) return false;

    u32 count = 0;
    const KeyDesc* table = GetKeyTable(r, count);

    for (int level = 0; level < 4; level++)
    {
        bool shift = level & 1;
        bool altgr = level & 2;

        for (u32 i = 0; i < count; i++)
        {
            if (CharForKey(table[i], shift, altgr, false) != character) continue;

            out = {(u8)table[i].KeyID, shift, altgr};
            return true;
        }
    }

    return false;
}

}
