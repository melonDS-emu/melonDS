#ifdef MELONPRIME_DS

#include "MelonPrimePatchShowHeadshotOnline.h"
#include "Config.h"
#include "NDS.h"

namespace MelonPrime {
namespace {

struct PatchWord {
    uint32_t address;
    uint32_t applyVal;
    uint32_t revertVal;
};

static constexpr const char* kCfgShowHeadshotOnline = "Metroid.GameFeature.ShowHeadshotOnline";

// H228 HEADSHOT WiFi Force Standalone Display
// ROM group order: JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6
static constexpr PatchWord kPatchWords[7] = {
    { 0x0201748Cu, 0xE1A00000u, 0x1A000016u }, // JP1.0
    { 0x0201748Cu, 0xE1A00000u, 0x1A000016u }, // JP1.1
    { 0x020174ACu, 0xE1A00000u, 0x1A000016u }, // US1.0
    { 0x020174B0u, 0xE1A00000u, 0x1A000016u }, // US1.1
    { 0x020174A4u, 0xE1A00000u, 0x1A000016u }, // EU1.0
    { 0x020174B0u, 0xE1A00000u, 0x1A000016u }, // EU1.1
    { 0x02019A44u, 0xE1A00000u, 0x1A000016u }, // KR1.0
};

static bool s_applied = false;
static uint8_t s_appliedRomGroupIndex = 0xFFu;

[[nodiscard]] static bool IsValidRomGroup(uint8_t romGroupIndex) noexcept
{
    return romGroupIndex < 7;
}

[[nodiscard]] static bool CanWritePatch(melonDS::NDS* nds, uint8_t romGroupIndex, bool apply)
{
    if (!nds || !IsValidRomGroup(romGroupIndex))
        return false;

    const auto& word = kPatchWords[romGroupIndex];
    const uint32_t current = nds->ARM9Read32(word.address);
    const uint32_t expected = apply ? word.revertVal : word.applyVal;
    const uint32_t already = apply ? word.applyVal : word.revertVal;
    return current == expected || current == already;
}

static void WritePatch(melonDS::NDS* nds, uint8_t romGroupIndex, bool apply)
{
    const auto& word = kPatchWords[romGroupIndex];
    nds->ARM9Write32(word.address, apply ? word.applyVal : word.revertVal);
}

} // namespace

void ShowHeadshotOnline_ApplyOnce(melonDS::NDS* nds, Config::Table& cfg, uint8_t romGroupIndex)
{
    if (!cfg.GetBool(kCfgShowHeadshotOnline))
    {
        ShowHeadshotOnline_RestoreOnce(nds, s_appliedRomGroupIndex);
        return;
    }

    if (!IsValidRomGroup(romGroupIndex))
        return;
    if (s_applied && s_appliedRomGroupIndex == romGroupIndex)
        return;
    if (s_applied)
        ShowHeadshotOnline_RestoreOnce(nds, s_appliedRomGroupIndex);
    if (!CanWritePatch(nds, romGroupIndex, true))
        return;

    WritePatch(nds, romGroupIndex, true);
    s_applied = true;
    s_appliedRomGroupIndex = romGroupIndex;
}

void ShowHeadshotOnline_RestoreOnce(melonDS::NDS* nds, uint8_t romGroupIndex)
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

void ShowHeadshotOnline_ResetPatchState()
{
    s_applied = false;
    s_appliedRomGroupIndex = 0xFFu;
}

} // namespace MelonPrime

#endif // MELONPRIME_DS
