#ifdef MELONPRIME_DS

#include "MelonPrimePatchShowEnemyHpMeterOnline.h"
#include "Config.h"
#include "NDS.h"

namespace MelonPrime {
namespace {

struct PatchWord {
    uint32_t address;
    uint32_t applyVal;
    uint32_t revertVal;
};

static constexpr const char* kCfgShowEnemyHpMeterOnline = "Metroid.GameFeature.ShowEnemyHpMeterOnline";

// Enemy HP Meter WiFi Force Display
// ROM group order: JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6
static constexpr PatchWord kPatchWords[7][3] = {
    { // JP1.0
        { 0x0202DBE4u, 0xE1A00000u, 0x128DD044u },
        { 0x0202DBE8u, 0xE1A00000u, 0x18BD4030u },
        { 0x0202DBECu, 0xE1A00000u, 0x112FFF1Eu },
    },
    { // JP1.1
        { 0x0202DBE4u, 0xE1A00000u, 0x128DD044u },
        { 0x0202DBE8u, 0xE1A00000u, 0x18BD4030u },
        { 0x0202DBECu, 0xE1A00000u, 0x112FFF1Eu },
    },
    { // US1.0
        { 0x0202DBC0u, 0xE1A00000u, 0x128DD044u },
        { 0x0202DBC4u, 0xE1A00000u, 0x18BD4030u },
        { 0x0202DBC8u, 0xE1A00000u, 0x112FFF1Eu },
    },
    { // US1.1
        { 0x0202DBC0u, 0xE1A00000u, 0x128DD044u },
        { 0x0202DBC4u, 0xE1A00000u, 0x18BD4030u },
        { 0x0202DBC8u, 0xE1A00000u, 0x112FFF1Eu },
    },
    { // EU1.0
        { 0x0202DBB8u, 0xE1A00000u, 0x128DD044u },
        { 0x0202DBBCu, 0xE1A00000u, 0x18BD4030u },
        { 0x0202DBC0u, 0xE1A00000u, 0x112FFF1Eu },
    },
    { // EU1.1
        { 0x0202DBC0u, 0xE1A00000u, 0x128DD044u },
        { 0x0202DBC4u, 0xE1A00000u, 0x18BD4030u },
        { 0x0202DBC8u, 0xE1A00000u, 0x112FFF1Eu },
    },
    { // KR1.0
        { 0x02036904u, 0xE1A00000u, 0x128DD044u },
        { 0x02036908u, 0xE1A00000u, 0x18BD8030u },
        { 0u, 0u, 0u },
    },
};

static constexpr uint8_t kPatchWordCounts[7] = { 3, 3, 3, 3, 3, 3, 2 };

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

void ShowEnemyHpMeterOnline_ApplyOnce(melonDS::NDS* nds, Config::Table& cfg, uint8_t romGroupIndex)
{
    if (!cfg.GetBool(kCfgShowEnemyHpMeterOnline))
    {
        ShowEnemyHpMeterOnline_RestoreOnce(nds, s_appliedRomGroupIndex);
        return;
    }

    if (!IsValidRomGroup(romGroupIndex))
        return;
    if (s_applied && s_appliedRomGroupIndex == romGroupIndex)
        return;
    if (s_applied)
        ShowEnemyHpMeterOnline_RestoreOnce(nds, s_appliedRomGroupIndex);
    if (!CanWritePatch(nds, romGroupIndex, true))
        return;

    WritePatch(nds, romGroupIndex, true);
    s_applied = true;
    s_appliedRomGroupIndex = romGroupIndex;
}

void ShowEnemyHpMeterOnline_RestoreOnce(melonDS::NDS* nds, uint8_t romGroupIndex)
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

void ShowEnemyHpMeterOnline_ResetPatchState()
{
    s_applied = false;
    s_appliedRomGroupIndex = 0xFFu;
}

} // namespace MelonPrime

#endif // MELONPRIME_DS
