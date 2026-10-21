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

#include <cstdio>

#include "PokeTypeBindings.h"

using melonDS::PokeTypeKeyboard;
using melonDS::u8;
using melonDS::u16;
using melonDS::u32;

namespace
{

struct SpecialKeyInfo
{
    u16 KeyID;
    const char* Label;
    int HostKey;
};

// Identical in every region, Japan included: the Japanese build takes a bare
// character, but its modifiers and arrows still travel as these codes.
const SpecialKeyInfo SpecialKeyTable[] =
{
    {0x28, "Enter",       Qt::Key_Return},
    {0x2A, "Backspace",   Qt::Key_Backspace},
    {0x2B, "Tab",         Qt::Key_Tab},
    {0x2C, "Space",       Qt::Key_Space},
    {0x39, "Caps Lock",   Qt::Key_CapsLock},
    {0x4F, "Right",       Qt::Key_Right},
    {0x50, "Left",        Qt::Key_Left},
    {0x51, "Down",        Qt::Key_Down},
    {0x52, "Up",          Qt::Key_Up},
    {0x70, "Ctrl",        Qt::Key_Control},
    {0x71, "Right Shift", (int)(Qt::Key_Shift | (1<<31))},
    {0x72, "Left Shift",  Qt::Key_Shift},
    {0x73, "Alt",         Qt::Key_Alt},
    {0x74, "Home",        Qt::Key_Home},
    {0x75, "Fn",          Qt::Key_Insert},
    {0x76, "AltGr",       Qt::Key_AltGr},
};

const SpecialKeyInfo* findSpecial(u16 keyid)
{
    for (const SpecialKeyInfo& info : SpecialKeyTable)
        if (info.KeyID == keyid)
            return &info;

    return nullptr;
}

const PokeTypeKeyboard::KeyDesc* findKey(PokeTypeKeyboard::Region r, u16 keyid)
{
    u32 count = 0;
    const PokeTypeKeyboard::KeyDesc* table = PokeTypeKeyboard::GetKeyTable(r, count);
    if (!table) return nullptr;

    for (u32 i = 0; i < count; i++)
        if (table[i].KeyID == keyid)
            return &table[i];

    return nullptr;
}

}

int PokeTypeBindings::slotFor(Region r, u16 keyid)
{
    if (r >= Region::MAX) return -1;
    if (keyid >= MaxKeys) return -1;

    return (int)keyid;
}

std::vector<u16> PokeTypeBindings::keyIDs(Region r)
{
    std::vector<u16> out;
    if (r >= Region::MAX) return out;

    for (u16 keyid = 0; keyid < MaxKeys; keyid++)
        if (findSpecial(keyid) || findKey(r, keyid))
            out.push_back(keyid);

    return out;
}

int PokeTypeBindings::dpadBitForKeyID(u16 keyid)
{
    switch (keyid)
    {
    case 0x4F: return 4;    // Right
    case 0x50: return 5;    // Left
    case 0x52: return 6;    // Up
    case 0x51: return 7;    // Down
    default:   return -1;
    }
}

std::string PokeTypeBindings::deadKeyLabel(int key)
{
    // lead with the accent glyph, which any UI font can draw
    switch (key)
    {
    case Qt::Key_Dead_Grave:      return "` Dead grave";
    case Qt::Key_Dead_Acute:      return "\xc2\xb4 Dead acute";
    case Qt::Key_Dead_Circumflex: return "^ Dead circumflex";
    case Qt::Key_Dead_Tilde:      return "~ Dead tilde";
    case Qt::Key_Dead_Diaeresis:  return "\xc2\xa8 Dead diaeresis";
    case Qt::Key_Dead_Cedilla:    return "\xc2\xb8 Dead cedilla";
    default:                      return "";
    }
}

// Some boards carry one key under two usage codes: Spain reaches '<' from both
// NonUSBackslash (0x64) and IntlRo (0x87). Only the first gets a default, or
// every fresh config would start out with a duplicate binding.
bool PokeTypeBindings::isAliasKey(Region r, u16 keyid)
{
    const PokeTypeKeyboard::KeyDesc* desc = findKey(r, keyid);
    if (!desc) return false;

    u32 count = 0;
    const PokeTypeKeyboard::KeyDesc* table = PokeTypeKeyboard::GetKeyTable(r, count);
    if (!table) return false;

    for (u32 i = 0; i < count; i++)
    {
        if (table[i].KeyID >= keyid) break;          // sorted; only earlier keys
        if (table[i].Base == desc->Base) return true;
    }

    return false;
}

int PokeTypeBindings::defaultBinding(Region r, u16 keyid)
{
    if (const SpecialKeyInfo* info = findSpecial(keyid))
        return info->HostKey;

    const PokeTypeKeyboard::KeyDesc* desc = findKey(r, keyid);
    if (!desc) return 0;

    if (isAliasKey(r, keyid)) return 0;

    // Bind the host key for this key's own letter or digit, so in positional
    // mode the host A gives whichever DS key types 'A'. Both levels count:
    // AZERTY puts the digits behind Shift.
    const u16 levels[2] = {desc->Base, desc->Shift};

    for (u16 c : levels)
    {
        if (c >= 'A' && c <= 'Z') return Qt::Key_A + (c - 'A');
        if (c >= 'a' && c <= 'z') return Qt::Key_A + (c - 'a');
        if (c >= '0' && c <= '9') return Qt::Key_0 + (c - '0');
    }

    // for printable ASCII, Qt's key code is the uppercased character
    if (desc->Base > 0x20 && desc->Base < 0x7F)
    {
        u16 c = desc->Base;
        if (c >= 'a' && c <= 'z') c -= 0x20;
        return (int)c;
    }

    // accent keys are dead keys on the layouts that have them
    if (desc->Base == 0xB4) return Qt::Key_Dead_Acute;
    if (desc->Base == 0xA8) return Qt::Key_Dead_Diaeresis;

    // Qt's Latin-1 key codes are the codepoints themselves (Ñ, ß, Ä...)
    if (desc->Base >= 0xA0 && desc->Base <= 0xFF)
        return normaliseHostKey(desc->Base);

    return 0;
}

std::string PokeTypeBindings::label(Region r, u16 keyid)
{
    if (const SpecialKeyInfo* info = findSpecial(keyid))
        return info->Label;

    const PokeTypeKeyboard::KeyDesc* desc = findKey(r, keyid);
    if (!desc) return "";

    // printable levels in key cap order, skipping repeats: "A / a"
    const u16 levels[3] = {desc->Base, desc->Shift, desc->AltGr};

    std::string out;
    u16 seen[3] = {0, 0, 0};
    int nseen = 0;

    for (u16 c : levels)
    {
        // Japan's Yen key (International3) sends the game's private U+FF0A
        if (c == 0xFF0A) c = 0x00A5;

        // skip control codes, private-use arrow glyphs, and the game's own
        // modifier codes at U+FF00..U+FF0B
        if (c == 0 || c < 0x20) continue;
        if (c >= 0xE000 && c <= 0xF8FF) continue;
        if (c >= 0xFF00 && c <= 0xFF0B) continue;

        bool dup = false;
        for (int i = 0; i < nseen; i++)
            if (seen[i] == c) dup = true;
        if (dup) continue;

        seen[nseen++] = c;

        if (!out.empty()) out += " / ";

        if (c < 0x80)
        {
            out += (char)c;
        }
        else if (c < 0x800)
        {
            // UTF-8 encode; the layout tables reach U+20AC at most
            out += (char)(0xC0 | (c >> 6));
            out += (char)(0x80 | (c & 0x3F));
        }
        else
        {
            out += (char)(0xE0 | (c >> 12));
            out += (char)(0x80 | ((c >> 6) & 0x3F));
            out += (char)(0x80 | (c & 0x3F));
        }
    }

    // No printable level: Japan's Esc and Hankaku/Zenkaku (both U+FF09, and
    // 0x35 is only unprintable on the JIS board) are named for their key caps,
    // anything else shows its usage code.
    if (out.empty())
    {
        if (keyid == 0x29) return "Esc";
        if (keyid == 0x35) return "Hankaku/Zenkaku";

        char buf[16];
        snprintf(buf, sizeof(buf), "Key 0x%02X", keyid);
        out = buf;
    }

    return out;
}

int PokeTypeBindings::rawBinding(Region r, u16 keyid) const
{
    int slot = slotFor(r, keyid);
    if (slot < 0) return 0;

    return raw[(int)r][slot];
}

int PokeTypeBindings::binding(Region r, u16 keyid) const
{
    int raw = rawBinding(r, keyid);
    if (raw != 0) return raw;

    return defaultBinding(r, keyid);
}

void PokeTypeBindings::setBinding(Region r, u16 keyid, int hostkey)
{
    int slot = slotFor(r, keyid);
    if (slot < 0) return;

    raw[(int)r][slot] = hostkey;
}

u16 PokeTypeBindings::keyIDFor(Region r, int hostkey) const
{
    // not <= 0: right-hand modifiers carry bit 31, so they are negative
    if (hostkey == 0 || hostkey == -1) return 0;
    if (r >= Region::MAX) return 0;

    int wanted = normaliseHostKey(hostkey);

    for (u16 keyid = 0; keyid < MaxKeys; keyid++)
    {
        if (!findSpecial(keyid) && !findKey(r, keyid)) continue;
        if (normaliseHostKey(binding(r, keyid)) == wanted) return keyid;
    }

    return 0;
}

void PokeTypeBindings::resetRegion(Region r)
{
    if (r >= Region::MAX) return;

    for (int i = 0; i < MaxKeys; i++)
        raw[(int)r][i] = 0;
}

u16 PokeTypeBindings::flipLetterCase(u16 ch)
{
    if (ch >= 'a' && ch <= 'z') return ch - 0x20;
    if (ch >= 'A' && ch <= 'Z') return ch + 0x20;

    return ch;
}

bool PokeTypeBindings::isShiftedSymbol(int key, Qt::KeyboardModifiers mods)
{
    if (!(mods & Qt::ShiftModifier)) return false;

    if (key > 0x20 && key < 0x7F)
    {
        if (key >= 'A' && key <= 'Z') return false;
        if (key >= 'a' && key <= 'z') return false;
        if (key >= '0' && key <= '9') return false;
        return true;
    }

    // Latin-1 symbols, not letters (0xD7 and 0xF7 are the multiply and divide signs)
    if (key > 0xA0 && key <= 0xFF)
        return key <= 0xBF || key == 0xD7 || key == 0xF7;

    return false;
}

u8 PokeTypeBindings::hidMods(Qt::KeyboardModifiers mods)
{
    u8 out = 0;

    if (mods & Qt::ControlModifier)     out |= 0x01;
    if (mods & Qt::ShiftModifier)       out |= 0x02;
    if (mods & Qt::AltModifier)         out |= 0x04;
    if (mods & Qt::GroupSwitchModifier) out |= 0x40;

    return out;
}
