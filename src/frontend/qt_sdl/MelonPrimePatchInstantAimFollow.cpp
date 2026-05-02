#ifdef MELONPRIME_DS

#include "MelonPrimePatchInstantAimFollow.h"
#include "Config.h"
#include "MelonPrimeDef.h"
#include "NDS.h"

namespace MelonPrime {
namespace {

struct PatchWord {
    uint32_t address;
    uint32_t applyVal;
    uint32_t revertVal;
};

// ROM group order:
// JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6
static constexpr PatchWord kPatchWords[][5] = {
    { // JP1.0
        { 0x02028070u, 0xE284009Cu, 0x1284009Cu },
        { 0x02028074u, 0xE284304Cu, 0x1284304Cu },
        { 0x02028078u, 0xE8900007u, 0x18900007u },
        { 0x0202807Cu, 0xE8830007u, 0x18830007u },
        { 0x02028080u, 0xEA00008Du, 0x1A00008Du },
    },
    { // JP1.1
        { 0x02028070u, 0xE284009Cu, 0x1284009Cu },
        { 0x02028074u, 0xE284304Cu, 0x1284304Cu },
        { 0x02028078u, 0xE8900007u, 0x18900007u },
        { 0x0202807Cu, 0xE8830007u, 0x18830007u },
        { 0x02028080u, 0xEA00008Du, 0x1A00008Du },
    },
    { // US1.0
        { 0x02028094u, 0xE284009Cu, 0x1284009Cu },
        { 0x02028098u, 0xE284304Cu, 0x1284304Cu },
        { 0x0202809Cu, 0xE8900007u, 0x18900007u },
        { 0x020280A0u, 0xE8830007u, 0x18830007u },
        { 0x020280A4u, 0xEA00008Du, 0x1A00008Du },
    },
    { // US1.1
        { 0x02028094u, 0xE284009Cu, 0x1284009Cu },
        { 0x02028098u, 0xE284304Cu, 0x1284304Cu },
        { 0x0202809Cu, 0xE8900007u, 0x18900007u },
        { 0x020280A0u, 0xE8830007u, 0x18830007u },
        { 0x020280A4u, 0xEA00008Du, 0x1A00008Du },
    },
    { // EU1.0
        { 0x0202808Cu, 0xE284009Cu, 0x1284009Cu },
        { 0x02028090u, 0xE284304Cu, 0x1284304Cu },
        { 0x02028094u, 0xE8900007u, 0x18900007u },
        { 0x02028098u, 0xE8830007u, 0x18830007u },
        { 0x0202809Cu, 0xEA00008Du, 0x1A00008Du },
    },
    { // EU1.1
        { 0x02028094u, 0xE284009Cu, 0x1284009Cu },
        { 0x02028098u, 0xE284304Cu, 0x1284304Cu },
        { 0x0202809Cu, 0xE8900007u, 0x18900007u },
        { 0x020280A0u, 0xE8830007u, 0x18830007u },
        { 0x020280A4u, 0xEA00008Du, 0x1A00008Du },
    },
    { // KR1.0
        { 0x0200B200u, 0xE1A00000u, 0x0A000003u },
        { 0u, 0u, 0u },
        { 0u, 0u, 0u },
        { 0u, 0u, 0u },
        { 0u, 0u, 0u },
    },
};

static constexpr uint8_t kPatchWordCounts[] = { 5, 5, 5, 5, 5, 5, 1 };
static bool s_applied = false;
static uint8_t s_appliedRomGroupIndex = 0xFFu;

[[nodiscard]] static bool IsValidRomGroup(uint8_t romGroupIndex) noexcept
{
    return romGroupIndex < sizeof(kPatchWordCounts) / sizeof(kPatchWordCounts[0]);
}

[[nodiscard]] static bool CanWritePatch(
    melonDS::NDS* nds,
    uint8_t romGroupIndex,
    bool apply)
{
    if (!nds || !IsValidRomGroup(romGroupIndex))
        return false;

    const auto& words = kPatchWords[romGroupIndex];
    const uint8_t count = kPatchWordCounts[romGroupIndex];
    for (uint8_t i = 0; i < count; ++i)
    {
        const uint32_t current = nds->ARM9Read32(words[i].address);
        const uint32_t expected = apply ? words[i].revertVal : words[i].applyVal;
        const uint32_t already = apply ? words[i].applyVal : words[i].revertVal;
        if (current != expected && current != already)
            return false;
    }
    return true;
}

static void WritePatch(melonDS::NDS* nds, uint8_t romGroupIndex, bool apply)
{
    const auto& words = kPatchWords[romGroupIndex];
    const uint8_t count = kPatchWordCounts[romGroupIndex];
    for (uint8_t i = 0; i < count; ++i)
        nds->ARM9Write32(words[i].address, apply ? words[i].applyVal : words[i].revertVal);
}

} // namespace

void InstantAimFollow_ApplyOnce(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex)
{
    const bool shouldApply = cfg.GetBool(CfgKey::InstantAimFollow);

    if (!shouldApply)
    {
        InstantAimFollow_RestoreOnce(nds, s_appliedRomGroupIndex);
        return;
    }

    if (!IsValidRomGroup(romGroupIndex))
        return;
    if (s_applied && s_appliedRomGroupIndex == romGroupIndex)
        return;
    if (s_applied)
        InstantAimFollow_RestoreOnce(nds, s_appliedRomGroupIndex);
    if (!CanWritePatch(nds, romGroupIndex, true))
        return;

    WritePatch(nds, romGroupIndex, true);
    s_applied = true;
    s_appliedRomGroupIndex = romGroupIndex;
}

void InstantAimFollow_RestoreOnce(
    melonDS::NDS* nds,
    uint8_t romGroupIndex)
{
    if (!s_applied || !nds)
        return;
    if (IsValidRomGroup(s_appliedRomGroupIndex))
        romGroupIndex = s_appliedRomGroupIndex;
    if (!IsValidRomGroup(romGroupIndex))
        return;
    if (!CanWritePatch(nds, romGroupIndex, false))
        return;

    WritePatch(nds, romGroupIndex, false);
    s_applied = false;
    s_appliedRomGroupIndex = 0xFFu;
}

void InstantAimFollow_ResetPatchState()
{
    s_applied = false;
    s_appliedRomGroupIndex = 0xFFu;
}

} // namespace MelonPrime

#endif // MELONPRIME_DS
