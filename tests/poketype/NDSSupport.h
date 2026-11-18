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

#ifndef POKETYPE_NDSSUPPORT_H
#define POKETYPE_NDSSUPPORT_H

#include <cstring>
#include <memory>
#include <optional>
#include <vector>

#include "NDS.h"
#include "Savestate.h"

// An NDS whose scheduler mask the tests can see; NDS keeps it protected.
class TestNDS : public melonDS::NDS
{
public:
    using melonDS::NDS::NDS;

    melonDS::u32& EventMask() { return SchedListMask; }
};

// megabytes of memory timing tables: too big for the stack
inline std::unique_ptr<TestNDS> MakeNDS()
{
    melonDS::NDSArgs args;
    args.JIT = std::nullopt;
    return std::make_unique<TestNDS>(std::move(args));
}

inline std::vector<melonDS::u8> SaveNDS(melonDS::NDS& nds)
{
    melonDS::Savestate save;
    nds.DoSavestate(&save);
    save.Finish();

    const melonDS::u8* p = (const melonDS::u8*)save.Buffer();
    return std::vector<melonDS::u8>(p, p + save.Length());
}

inline bool LoadNDS(melonDS::NDS& nds, std::vector<melonDS::u8>& state)
{
    melonDS::Savestate load(state.data(), (melonDS::u32)state.size(), false);
    if (load.Error) return false;

    bool ok = nds.DoSavestate(&load);
    return ok && !load.Error;
}

// Rewrite a state the way savestate 14.0 laid it out: minor version 0, and no
// scheduler entry for the event 14.1 added. That entry is found by its contents,
// so the event must hold values that no other 16 bytes of the state match.
inline bool MakeVersion14_0(std::vector<melonDS::u8>& state, const melonDS::SchedEvent& added)
{
    using melonDS::u8;
    using melonDS::u16;
    using melonDS::u32;

    u8 entry[16];
    memcpy(&entry[0], &added.Timestamp, 8);
    memcpy(&entry[8], &added.FuncID, 4);
    memcpy(&entry[12], &added.Param, 4);

    size_t found = 0;
    int matches = 0;
    for (size_t i = 0; i + 16 <= state.size(); i++)
    {
        if (memcmp(&state[i], entry, 16) == 0)
        {
            found = i;
            matches++;
        }
    }
    if (matches != 1) return false;

    // section lengths include the 16-byte section header
    for (size_t off = 0x10; off + 16 <= state.size();)
    {
        u32 len;
        memcpy(&len, &state[off + 4], 4);
        if (len < 16) return false;

        if (found >= off + 16 && found + 16 <= off + len)
        {
            len -= 16;
            memcpy(&state[off + 4], &len, 4);
            state.erase(state.begin() + found, state.begin() + found + 16);

            // the scheduler mask follows the last entry, which is the one removed,
            // and a 14.0 mask has no bit for that event
            static_assert(melonDS::Event_CartBTKeyboardTimer == melonDS::Event_MAX - 1);
            u32 mask;
            memcpy(&mask, &state[found], 4);
            mask &= ~(1u << melonDS::Event_CartBTKeyboardTimer);
            memcpy(&state[found], &mask, 4);

            u32 total = (u32)state.size();
            memcpy(&state[0x08], &total, 4);

            u16 minor = 0;
            memcpy(&state[0x06], &minor, 2);
            return true;
        }

        off += len;
    }

    return false;
}

#endif // POKETYPE_NDSSUPPORT_H
