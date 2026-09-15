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

#include <vector>

#include "TestSupport.h"
#include "PokeTypeBindings.h"

using melonDS::PokeTypeKeyboard;
using Region = PokeTypeKeyboard::Region;

// whether two of a region's keys are bound to the same host key
static bool HasDuplicateBindings(const PokeTypeBindings& b, Region r)
{
    std::vector<melonDS::u16> ids = PokeTypeBindings::keyIDs(r);

    for (size_t i = 0; i < ids.size(); i++)
    {
        int ka = b.binding(r, ids[i]);
        if (ka == 0 || ka == -1) continue;   // sentinels only; bit 31 is a real key
        ka = PokeTypeBindings::normaliseHostKey(ka);

        for (size_t j = i + 1; j < ids.size(); j++)
        {
            int kb = b.binding(r, ids[j]);
            if (kb != 0 && kb != -1 && PokeTypeBindings::normaliseHostKey(kb) == ka)
                return true;
        }
    }

    return false;
}

int runBindingsTests()
{
    PokeTypeBindings b;

    // defaults for the special keys are the same in every region
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Europe, 0x75), Qt::Key_Insert);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::France, 0x75), Qt::Key_Insert);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Europe, 0x72), Qt::Key_Shift);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Europe, 0x71),
             (int)(Qt::Key_Shift | (1<<31)));
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Europe, 0x28), Qt::Key_Return);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Europe, 0x52), Qt::Key_Up);

    // printable keys default to the host key matching their letter or digit
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Europe, 0x04), Qt::Key_A);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Europe, 0x1E), Qt::Key_1);
    // France is AZERTY: the A and Q keys are swapped relative to the HID codes
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::France, 0x14), Qt::Key_A);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::France, 0x04), Qt::Key_Q);

    // AZERTY's 1 key is '&' unshifted and '1' shifted
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::France, 0x1E), Qt::Key_1);

    // punctuation falls back to the base character, which is Qt's key code for ASCII
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Europe, 0x33), Qt::Key_Semicolon);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Europe, 0x36), Qt::Key_Comma);

    // sentinels: unset resolves to the default, -1 stays unbound
    CHECK_EQ(b.rawBinding(Region::Europe, 0x04), 0);
    CHECK_EQ(b.binding(Region::Europe, 0x04), Qt::Key_A);

    b.setBinding(Region::Europe, 0x04, -1);
    CHECK_EQ(b.rawBinding(Region::Europe, 0x04), -1);
    CHECK_EQ(b.binding(Region::Europe, 0x04), -1);
    CHECK_EQ(b.keyIDFor(Region::Europe, Qt::Key_A), 0);

    b.setBinding(Region::Europe, 0x04, Qt::Key_Z);
    CHECK_EQ(b.binding(Region::Europe, 0x04), Qt::Key_Z);
    CHECK_EQ(b.keyIDFor(Region::Europe, Qt::Key_Z), 0x04);

    // regions are independent
    CHECK_EQ(b.rawBinding(Region::France, 0x04), 0);

    b.resetRegion(Region::Europe);
    CHECK_EQ(b.rawBinding(Region::Europe, 0x04), 0);
    CHECK_EQ(b.binding(Region::Europe, 0x04), Qt::Key_A);

    // reverse lookup finds the special keys
    CHECK_EQ(b.keyIDFor(Region::Europe, Qt::Key_Insert), 0x75);
    CHECK_EQ(b.keyIDFor(Region::Europe, Qt::Key_Up), 0x52);
    CHECK_EQ(b.keyIDFor(Region::Europe, Qt::Key_F13), 0);

    // right-hand modifiers carry bit 31 as part of the key code
    CHECK_EQ(b.keyIDFor(Region::Europe, (int)(Qt::Key_Shift | (1<<31))), 0x71);

    // no default collides in any region: Spain has '<' on both 0x64 and 0x87,
    // and only the first gets the default
    for (int r = 0; r < PokeTypeBindings::NumRegions; r++)
        CHECK(!HasDuplicateBindings(b, (Region)r));

    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Spain, 0x64), Qt::Key_Less);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Spain, 0x87), 0);

    // Latin-1 keys default to their character's key code (ß has no uppercase form)
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Spain, 0x33), Qt::Key_Ntilde);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Germany, 0x34), Qt::Key_Adiaeresis);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Germany, 0x2D), Qt::Key_ssharp);

    // Italy's table stores à lowercase (0xE0); the default is the uppercase code
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Italy, 0x34), Qt::Key_Agrave);
    CHECK_EQ((int)Qt::Key_Agrave, 0xC0);

    // keyIDFor matches either case
    CHECK_EQ(b.keyIDFor(Region::Italy, 0x00E0), 0x34);
    CHECK_EQ(b.keyIDFor(Region::Italy, 0x00C0), 0x34);

    // accent keys are dead keys on these layouts, so they default to the
    // dead-key code a real press sends rather than Key_acute
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Spain, 0x34), Qt::Key_Dead_Acute);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Germany, 0x2E), Qt::Key_Dead_Acute);

    // normaliseHostKey uppercases Latin-1 letters only: not ASCII, ÷, ß or Qt key codes
    CHECK_EQ(PokeTypeBindings::normaliseHostKey('a'), 'a');
    CHECK_EQ(PokeTypeBindings::normaliseHostKey('Z'), 'Z');
    CHECK_EQ(PokeTypeBindings::normaliseHostKey(0xF7), 0xF7);
    CHECK_EQ(PokeTypeBindings::normaliseHostKey(0xDF), 0xDF);
    CHECK_EQ(PokeTypeBindings::normaliseHostKey(Qt::Key_A), Qt::Key_A);

    // ...and folds bare ´ and ¨ onto their dead-key codes, idempotently
    CHECK_EQ(PokeTypeBindings::normaliseHostKey(0xB4), Qt::Key_Dead_Acute);
    CHECK_EQ(PokeTypeBindings::normaliseHostKey(0xA8), Qt::Key_Dead_Diaeresis);
    CHECK_EQ(PokeTypeBindings::normaliseHostKey(Qt::Key_Dead_Acute), Qt::Key_Dead_Acute);
    CHECK_EQ(PokeTypeBindings::normaliseHostKey(Qt::Key_Dead_Diaeresis), Qt::Key_Dead_Diaeresis);

    // so Spain's ´ key is found by either code
    CHECK_EQ(b.keyIDFor(Region::Spain, 0xB4), 0x34);
    CHECK_EQ(b.keyIDFor(Region::Spain, Qt::Key_Dead_Acute), 0x34);

    for (int r = 0; r < PokeTypeBindings::NumRegions; r++)
        CHECK(!HasDuplicateBindings(b, (Region)r));

    // Spain's 0x89 has no printable level, so it is labelled by usage code
    CHECK(PokeTypeBindings::label(Region::Spain, 0x89) == "Key 0x89");

    // Japan: Esc, Hankaku/Zenkaku and Yen have no printable base, so no default
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Japan, 0x75), Qt::Key_Insert);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Japan, 0x04), Qt::Key_A);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Japan, 0x1F), Qt::Key_2);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Japan, 0x2F), Qt::Key_At);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Japan, 0x87), Qt::Key_Backslash);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Japan, 0x29), 0);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Japan, 0x35), 0);
    CHECK_EQ(PokeTypeBindings::defaultBinding(Region::Japan, 0x89), 0);

    // the Yen key's base is the game's private code U+FF0A, drawn as ¥;
    // Esc and Hankaku/Zenkaku have no character and are named instead
    CHECK(PokeTypeBindings::label(Region::Japan, 0x1F) == "2 / \"");
    CHECK(PokeTypeBindings::label(Region::Japan, 0x87) == "\\ / _");
    CHECK(PokeTypeBindings::label(Region::Japan, 0x89) == "\xc2\xa5 / |");
    CHECK(PokeTypeBindings::label(Region::Japan, 0x29) == "Esc");
    CHECK(PokeTypeBindings::label(Region::Japan, 0x35) == "Hankaku/Zenkaku");

    // keyIDs drives config load/save. Japan's layout table carries only nine of
    // the sixteen special keys, so the rest (modifiers, Home, Fn) must come from
    // the special-key set or their bindings are never saved.
    {
        std::vector<melonDS::u16> eur = PokeTypeBindings::keyIDs(Region::Europe);
        std::vector<melonDS::u16> jpn = PokeTypeBindings::keyIDs(Region::Japan);
        CHECK_EQ(eur.size(), 64);
        CHECK_EQ(jpn.size(), 59 + 7);
        for (size_t i = 1; i < jpn.size(); i++)
            CHECK(jpn[i-1] < jpn[i]);

        auto has = [](const std::vector<melonDS::u16>& v, melonDS::u16 id) {
            for (melonDS::u16 x : v) if (x == id) return true;
            return false;
        };
        CHECK(has(jpn, 0x75));
        CHECK(has(jpn, 0x70));
        CHECK(has(jpn, 0x04));
        CHECK(has(jpn, 0x89));
        CHECK(!has(jpn, 0x64));
        CHECK(PokeTypeBindings::keyIDs(Region::MAX).empty());
    }

    // labels
    CHECK(PokeTypeBindings::label(Region::Europe, 0x75) == "Fn");
    CHECK(PokeTypeBindings::label(Region::Europe, 0x72) == "Left Shift");
    CHECK(PokeTypeBindings::label(Region::Europe, 0x04) == "A / a");

    // letters invert, and only letters
    CHECK_EQ(PokeTypeBindings::flipLetterCase('a'), 'A');
    CHECK_EQ(PokeTypeBindings::flipLetterCase('A'), 'a');
    CHECK_EQ(PokeTypeBindings::flipLetterCase('z'), 'Z');
    CHECK_EQ(PokeTypeBindings::flipLetterCase('1'), '1');
    CHECK_EQ(PokeTypeBindings::flipLetterCase(';'), ';');
    CHECK_EQ(PokeTypeBindings::flipLetterCase(0x00E9), 0x00E9);   // accented, left alone
    CHECK_EQ(PokeTypeBindings::flipLetterCase(0xFF00), 0xFF00);   // pseudo-code, left alone

    // HID modifier bits, matching the game's own producer
    CHECK_EQ(PokeTypeBindings::hidMods(Qt::NoModifier), 0x00);
    CHECK_EQ(PokeTypeBindings::hidMods(Qt::ShiftModifier), 0x02);
    CHECK_EQ(PokeTypeBindings::hidMods(Qt::ControlModifier), 0x01);
    CHECK_EQ(PokeTypeBindings::hidMods(Qt::AltModifier), 0x04);
    CHECK_EQ(PokeTypeBindings::hidMods(Qt::GroupSwitchModifier), 0x40);
    CHECK_EQ(PokeTypeBindings::hidMods(Qt::ShiftModifier | Qt::ControlModifier), 0x03);

    // With Shift held Qt reports the shifted symbol (US Shift+2 is Key_At).
    // Looking that up would find the DS key with '@' unshifted -- JIS's @ key,
    // which types '`' with Shift -- so shifted symbols are not looked up.
    const Qt::KeyboardModifiers shift = Qt::ShiftModifier;
    CHECK(PokeTypeBindings::isShiftedSymbol(Qt::Key_At, shift));
    CHECK(PokeTypeBindings::isShiftedSymbol(Qt::Key_AsciiCircum, shift));
    CHECK(PokeTypeBindings::isShiftedSymbol(Qt::Key_Colon, shift));
    CHECK(PokeTypeBindings::isShiftedSymbol(Qt::Key_Exclam, shift));
    CHECK(PokeTypeBindings::isShiftedSymbol(Qt::Key_Plus, shift));
    CHECK(PokeTypeBindings::isShiftedSymbol(Qt::Key_sterling, shift));   // UK Shift+3
    CHECK(PokeTypeBindings::isShiftedSymbol(Qt::Key_At, shift | Qt::GroupSwitchModifier));

    // the same symbol unshifted is a key of its own -- '@' on a JIS board
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_At, Qt::NoModifier));
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_Plus, Qt::NoModifier));

    // letters keep their key code under Shift, Latin-1 ones included
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_A, shift));
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_Z, shift));
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_Ntilde, shift));
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_Adiaeresis, shift));
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_ssharp, shift));
    CHECK(!PokeTypeBindings::isShiftedSymbol(0x00E9, shift));            // é, as some setups report it

    // so do digits, which AZERTY reports with Shift held
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_1, shift));
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_0, shift));

    // and space, the non-printing keys, and dead keys
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_Space, shift));
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_Return, shift));
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_Left, shift));
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_Shift, shift));
    CHECK(!PokeTypeBindings::isShiftedSymbol(Qt::Key_Dead_Diaeresis, shift));

    // modifiers the game has no bit for contribute nothing
    CHECK_EQ(PokeTypeBindings::hidMods(Qt::MetaModifier), 0x00);
    CHECK_EQ(PokeTypeBindings::hidMods(Qt::KeypadModifier), 0x00);
    CHECK_EQ(PokeTypeBindings::hidMods(Qt::MetaModifier | Qt::ShiftModifier), 0x02);

    // arrow keys map to KEYINPUT bits 4-7 for the "Arrow keys also press the
    // D-pad" option: the game's menus read the pad, not the key queue
    CHECK_EQ(PokeTypeBindings::dpadBitForKeyID(0x4F), 4);
    CHECK_EQ(PokeTypeBindings::dpadBitForKeyID(0x50), 5);
    CHECK_EQ(PokeTypeBindings::dpadBitForKeyID(0x52), 6);
    CHECK_EQ(PokeTypeBindings::dpadBitForKeyID(0x51), 7);
    CHECK_EQ(PokeTypeBindings::dpadBitForKeyID(0x04), -1);
    CHECK_EQ(PokeTypeBindings::dpadBitForKeyID(0x28), -1);
    CHECK_EQ(PokeTypeBindings::dpadBitForKeyID(0x75), -1);
    CHECK_EQ(PokeTypeBindings::dpadBitForKeyID(0), -1);

    // QKeySequence has no name for dead keys; any other key gets an empty label
    CHECK(PokeTypeBindings::deadKeyLabel(Qt::Key_Dead_Acute) == "\xc2\xb4 Dead acute");
    CHECK(PokeTypeBindings::deadKeyLabel(Qt::Key_Dead_Diaeresis) == "\xc2\xa8 Dead diaeresis");
    CHECK(PokeTypeBindings::deadKeyLabel(Qt::Key_Dead_Grave) == "` Dead grave");
    CHECK(PokeTypeBindings::deadKeyLabel(Qt::Key_Dead_Circumflex) == "^ Dead circumflex");
    CHECK(PokeTypeBindings::deadKeyLabel(Qt::Key_Dead_Tilde) == "~ Dead tilde");
    CHECK(PokeTypeBindings::deadKeyLabel(Qt::Key_Dead_Cedilla) == "\xc2\xb8 Dead cedilla");
    CHECK(PokeTypeBindings::deadKeyLabel(Qt::Key_A).empty());
    CHECK(PokeTypeBindings::deadKeyLabel(Qt::Key_Space).empty());

    return TestFailures();
}
