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

#include <cstring>
#include <set>

#include "NDSSupport.h"
#include "TestSupport.h"
#include "PokeTypeKeyboard.h"

using melonDS::PokeTypeKeyboard;
using melonDS::u16;
using melonDS::u32;

static const PokeTypeKeyboard::Region kMapped[5] =
{
    PokeTypeKeyboard::Region::Europe,
    PokeTypeKeyboard::Region::France,
    PokeTypeKeyboard::Region::Germany,
    PokeTypeKeyboard::Region::Italy,
    PokeTypeKeyboard::Region::Spain,
};

// checked per region as well as in total, so errors in two regions can't cancel out
static const u32 kExpectedCount[5] = { 64, 64, 64, 64, 66 };

static const u16 kSpecialIDs[16] =
{
    0x28, 0x2A, 0x2B, 0x2C, 0x39, 0x4F, 0x50, 0x51,
    0x52, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
};

int runKeyTableTests()
{
    u32 total = 0;

    for (u32 r = 0; r < 5; r++)
    {
        PokeTypeKeyboard::Region region = kMapped[r];
        u32 count = 0;
        const PokeTypeKeyboard::KeyDesc* table = PokeTypeKeyboard::GetKeyTable(region, count);

        CHECK(table != nullptr);
        if (!table) continue;
        total += count;
        CHECK_EQ(count, kExpectedCount[r]);

        // sorted by key ID, no duplicates, every key has a base character
        for (u32 i = 0; i < count; i++)
        {
            CHECK(table[i].Base != 0);
            if (i) CHECK(table[i-1].KeyID < table[i].KeyID);
        }

        // all sixteen special keys are present and carry their pseudo-code
        for (u16 id : kSpecialIDs)
        {
            const PokeTypeKeyboard::KeyDesc* desc = nullptr;
            for (u32 i = 0; i < count; i++)
                if (table[i].KeyID == id) { desc = &table[i]; break; }

            CHECK(desc != nullptr);
            if (!desc) continue;

            u16 special = PokeTypeKeyboard::SpecialCharForKeyID(id);
            CHECK(special != 0);
            CHECK_EQ(desc->Base, special);
        }

        // every character on the keyboard can be typed, by a keystroke the game
        // decodes back to it, on this level or an earlier one
        for (u32 i = 0; i < count; i++)
        {
            const PokeTypeKeyboard::KeyDesc& k = table[i];
            const u16 levels[4] = {k.Base, k.Shift, k.AltGr, k.ShiftAltGr};

            for (int level = 0; level < 4; level++)
            {
                u16 ch = levels[level];
                if (ch == 0) continue;

                PokeTypeKeyboard::Keystroke ks {};
                CHECK(PokeTypeKeyboard::FindKeystroke(region, ch, ks));

                const PokeTypeKeyboard::KeyDesc* found = nullptr;
                for (u32 j = 0; j < count; j++)
                    if (table[j].KeyID == ks.KeyID) found = &table[j];

                CHECK(found != nullptr);
                if (!found) continue;
                CHECK_EQ(PokeTypeKeyboard::CharForKey(*found, ks.Shift, ks.AltGr, false), ch);
                CHECK((ks.Shift ? 1 : 0) + (ks.AltGr ? 2 : 0) <= level);
            }
        }
    }

    // Spain's extra keys 0x87 (IntlRo) and 0x89 repeat characters of 0x64 and
    // 0x76, so nothing above reaches them. Both depend on the extractor's
    // KEYTAB_USAGE_COUNT (0x90), which is set by inspection, not read from the ROM.
    {
        u32 spaCount = 0;
        const PokeTypeKeyboard::KeyDesc* spa = PokeTypeKeyboard::GetKeyTable(
            PokeTypeKeyboard::Region::Spain, spaCount);
        CHECK(spa != nullptr);
        if (spa)
        {
            const PokeTypeKeyboard::KeyDesc* key87 = nullptr;
            const PokeTypeKeyboard::KeyDesc* key89 = nullptr;
            for (u32 i = 0; i < spaCount; i++)
            {
                if (spa[i].KeyID == 0x87) key87 = &spa[i];
                if (spa[i].KeyID == 0x89) key89 = &spa[i];
            }

            CHECK(key87 != nullptr);
            CHECK(key89 != nullptr);
            if (key87)
            {
                CHECK_EQ(key87->Base, 0x003C);
                CHECK_EQ(key87->Shift, 0x003E);
            }
            if (key89)
                CHECK_EQ(key89->Base, 0xFF0B);
        }
    }

    // KEYTAB has 81 (83 for Spain) rows with a non-zero raw base byte, but 17
    // of them (F1-F12, PageUp/Delete/End/PageDown and usage 0x01) decode through
    // CHARTAB to 0 on every level: keys this keyboard doesn't have.
    CHECK_EQ(total, 322);

    // Japan's layout comes from the Japanese build's two-stage decoder, not a
    // CHARTAB/KEYTAB pair, and has no TypingMap: that build's queue takes a bare
    // character. The decoder also has a US table; these checks pin the JIS one.
    {
        u32 jpcount = 0;
        const PokeTypeKeyboard::KeyDesc* jpn = PokeTypeKeyboard::GetKeyTable(
            PokeTypeKeyboard::Region::Japan, jpcount);
        CHECK(jpn != nullptr);
        // 26 letters, 10 digits, 14 punctuation and JIS keys, 9 special keys
        CHECK_EQ(jpcount, 59);

        auto jpKey = [&](u16 id) -> const PokeTypeKeyboard::KeyDesc* {
            for (u32 i = 0; jpn && i < jpcount; i++)
                if (jpn[i].KeyID == id) return &jpn[i];
            return nullptr;
        };

        for (u32 i = 0; jpn && i < jpcount; i++)
        {
            CHECK(jpn[i].Base != 0);
            if (i) CHECK(jpn[i-1].KeyID < jpn[i].KeyID);

            // the Japanese decoder never looks at AltGr or Caps Lock
            CHECK_EQ(jpn[i].AltGr, jpn[i].Base);
            CHECK_EQ(jpn[i].ShiftAltGr, jpn[i].Shift);
            CHECK_EQ(jpn[i].CapsSensitive, 0);
        }

        // its special keys decode to the characters melonDS sends for them; the
        // modifiers are not keys here, but pseudo-usages 0x60..0x64
        for (u16 id : kSpecialIDs)
        {
            const PokeTypeKeyboard::KeyDesc* desc = jpKey(id);
            if (id >= 0x70) { CHECK(desc == nullptr); continue; }

            CHECK(desc != nullptr);
            if (desc) CHECK_EQ(desc->Base, PokeTypeKeyboard::SpecialCharForKeyID(id));
        }
        for (u16 id = 0x60; id <= 0x64; id++)
            CHECK(jpKey(id) == nullptr);

        // the rows where JIS and US disagree, plus a letter (uppercase unshifted)
        struct Row { u16 id, base, shift; };
        static const Row jis[] =
        {
            {0x04, 'A', 'a'},
            {0x1F, '2', '"'},   {0x23, '6', '&'},   {0x24, '7', '\''},
            {0x25, '8', '('},   {0x26, '9', ')'},   {0x27, '0', 0},
            {0x2D, '-', '='},   {0x2E, '^', '~'},   {0x2F, '@', '`'},
            {0x30, '[', '{'},   {0x32, ']', '}'},   {0x33, ';', '+'},
            {0x34, ':', '*'},   {0x87, '\\', '_'},  {0x89, 0xFF0A, '|'},
            {0x29, 0xFF09, 0xFF09}, {0x35, 0xFF09, 0xFF09},
        };
        for (const Row& row : jis)
        {
            const PokeTypeKeyboard::KeyDesc* desc = jpKey(row.id);
            CHECK(desc != nullptr);
            if (!desc) continue;
            CHECK_EQ(desc->Base, row.base);
            CHECK_EQ(desc->Shift, row.shift);
        }

        // every printable ASCII character is typeable and nothing else printable
        // is: the board types romaji, not kana
        std::set<u16> chars;
        for (u32 i = 0; jpn && i < jpcount; i++)
        {
            for (u16 c : {jpn[i].Base, jpn[i].Shift})
            {
                if (c == 0) continue;
                chars.insert(c);
                CHECK(c < 0x80 || c >= 0xE000);
            }
        }
        for (u16 c = 0x20; c < 0x7F; c++)
            CHECK(chars.count(c) == 1);
    }

    // game codes resolve to regions
    {
        auto nds = MakeNDS();
        CHECK(nds->PokeTypeKeyboard.SetGameCode(0x50505A55));
        CHECK(nds->PokeTypeKeyboard.GetRegion() == PokeTypeKeyboard::Region::Europe);
        CHECK(nds->PokeTypeKeyboard.SetGameCode(0x4A505A55));
        CHECK(nds->PokeTypeKeyboard.GetRegion() == PokeTypeKeyboard::Region::Japan);
        CHECK(!nds->PokeTypeKeyboard.SetGameCode(0x00000000));
        CHECK(nds->PokeTypeKeyboard.GetRegion() == PokeTypeKeyboard::Region::MAX);
    }

    // three-letter region tags used in config paths
    CHECK(strcmp(PokeTypeKeyboard::RegionCode(PokeTypeKeyboard::Region::Europe),  "EUR") == 0);
    CHECK(strcmp(PokeTypeKeyboard::RegionCode(PokeTypeKeyboard::Region::France),  "FRA") == 0);
    CHECK(strcmp(PokeTypeKeyboard::RegionCode(PokeTypeKeyboard::Region::Germany), "GER") == 0);
    CHECK(strcmp(PokeTypeKeyboard::RegionCode(PokeTypeKeyboard::Region::Italy),   "ITA") == 0);
    CHECK(strcmp(PokeTypeKeyboard::RegionCode(PokeTypeKeyboard::Region::Spain),   "SPA") == 0);
    CHECK(strcmp(PokeTypeKeyboard::RegionCode(PokeTypeKeyboard::Region::Japan),   "JPN") == 0);
    CHECK(strcmp(PokeTypeKeyboard::RegionCode(PokeTypeKeyboard::Region::MAX),     "")    == 0);

    // a printable key is not a special key
    CHECK_EQ(PokeTypeKeyboard::SpecialCharForKeyID(0x04), 0);

    // the game's convention: a letter's unshifted character is uppercase
    u32 count = 0;
    const PokeTypeKeyboard::KeyDesc* eur = PokeTypeKeyboard::GetKeyTable(
        PokeTypeKeyboard::Region::Europe, count);
    const PokeTypeKeyboard::KeyDesc* keyA = nullptr;
    const PokeTypeKeyboard::KeyDesc* key1 = nullptr;
    for (u32 i = 0; i < count; i++)
    {
        if (eur[i].KeyID == 0x04) keyA = &eur[i];
        if (eur[i].KeyID == 0x1E) key1 = &eur[i];
    }

    CHECK(keyA != nullptr);
    CHECK(key1 != nullptr);
    if (!keyA || !key1) return TestFailures();

    CHECK_EQ(keyA->Base, 'A');
    CHECK_EQ(keyA->Shift, 'a');
    CHECK_EQ(keyA->CapsSensitive, 1);

    CHECK_EQ(PokeTypeKeyboard::CharForKey(*keyA, false, false, false), 'A');
    CHECK_EQ(PokeTypeKeyboard::CharForKey(*keyA, true,  false, false), 'a');
    // caps swaps the two levels, but only for caps-sensitive keys
    CHECK_EQ(PokeTypeKeyboard::CharForKey(*keyA, false, false, true),  'a');
    CHECK_EQ(PokeTypeKeyboard::CharForKey(*keyA, true,  false, true),  'A');

    CHECK_EQ(key1->Base, '1');
    CHECK_EQ(key1->CapsSensitive, 0);
    CHECK_EQ(PokeTypeKeyboard::CharForKey(*key1, false, false, true), '1');
    CHECK_EQ(PokeTypeKeyboard::CharForKey(*key1, true,  false, false), key1->Shift);

    // AltGr: E carries the euro sign on the European board
    const PokeTypeKeyboard::KeyDesc* keyE = nullptr;
    for (u32 i = 0; i < count; i++)
        if (eur[i].KeyID == 0x08) keyE = &eur[i];

    CHECK(keyE != nullptr);
    if (keyE)
    {
        CHECK_EQ(keyE->AltGr, 0x20AC);
        CHECK_EQ(PokeTypeKeyboard::CharForKey(*keyE, false, true, false), 0x20AC);
    }

    return TestFailures();
}
