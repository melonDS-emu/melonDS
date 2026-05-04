#ifdef MELONPRIME_DS

#include "MelonPrimePatchFixNoxusBladePersistence.h"
#include "Config.h"
#include "NDS.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace MelonPrime {

namespace {

// Hook point: the instruction immediately after CPlayer+0x860 = 0 in the
// death/respawn cleanup sequence.
// JP/US/EU: CPlayer* is in r10.  KR1_0: CPlayer* is in r6.

struct DeathCleanupHook
{
    uint32_t Address;
    uint8_t  PlayerReg;
};

struct RomDeathCleanupHooks
{
    const DeathCleanupHook* Hooks;
    std::size_t             Count;
};

static constexpr DeathCleanupHook kHooks_JP1_0[] = { {0x02017D30u, 10} };
static constexpr DeathCleanupHook kHooks_JP1_1[] = { {0x02017D30u, 10} };
static constexpr DeathCleanupHook kHooks_US1_0[] = { {0x02017D50u, 10} };
static constexpr DeathCleanupHook kHooks_US1_1[] = { {0x02017D54u, 10} };
static constexpr DeathCleanupHook kHooks_EU1_0[] = { {0x02017D48u, 10} };
static constexpr DeathCleanupHook kHooks_EU1_1[] = { {0x02017D54u, 10} };
static constexpr DeathCleanupHook kHooks_KR1_0[] = { {0x0201A2B4u,  6} };

static constexpr RomDeathCleanupHooks kRomHooks[] = {
    {kHooks_JP1_0, 1}, {kHooks_JP1_1, 1},
    {kHooks_US1_0, 1}, {kHooks_US1_1, 1},
    {kHooks_EU1_0, 1}, {kHooks_EU1_1, 1},
    {kHooks_KR1_0, 1},
};

#ifdef MELONPRIME_ENABLE_DEVELOPER_FEATURES
static constexpr bool kFixNoxusBladePersistenceAvailable = true;
#else
static constexpr bool kFixNoxusBladePersistenceAvailable = false;
#endif
static constexpr uint32_t kOffAltAttackTimer    = 0x704u;

static bool IsMainRamRange(uint32_t address, uint32_t size)
{
    return size != 0
        && address >= 0x02000000u
        && size - 1u <= 0x023FFFFFu - address;
}

static const DeathCleanupHook* FindHook(
    const DeathCleanupHook* hooks,
    std::size_t count,
    uint32_t arm9ExecAddr)
{
    if (!hooks)
        return nullptr;

    switch (count)
    {
    case 1:
        return hooks[0].Address == arm9ExecAddr ? &hooks[0] : nullptr;
    case 0:
        return nullptr;
    default:
        for (std::size_t i = 0; i < count; ++i)
        {
            if (hooks[i].Address == arm9ExecAddr)
                return &hooks[i];
        }
        return nullptr;
    }
}

static bool ApplyHookInternal(
    melonDS::NDS* nds,
    uint32_t arm9ExecAddr,
    const DeathCleanupHook* hook,
    const uint32_t regs[16])
{
    if (!hook)
        return false;

    const uint32_t player = regs[hook->PlayerReg];
    if (!IsMainRamRange(player, kOffAltAttackTimer + sizeof(uint16_t)))
        return false;

    uint16_t zero = 0;
    std::memcpy(nds->MainRAM + ((player + kOffAltAttackTimer) & 0x3FFFFFu), &zero, sizeof(zero));
    return true;
}

// File-static hook state — set/cleared by the shared dispatcher.
static Config::Table* s_cfg            = nullptr;
static const DeathCleanupHook* s_activeHooks = nullptr;
static std::size_t s_activeHookCount = 0;

// Config generation cache: avoids GetBool map lookup on every hook invocation.
static std::atomic<uint32_t> s_configGen{1};
static uint32_t s_configGenSeen  = 0;   // emu thread only
static bool     s_enabledCached  = false; // emu thread only

} // anonymous namespace

uint32_t FixNoxusBladePersistence_GetAddresses(
    uint8_t romGroupIndex, uint32_t* out, uint32_t maxCount)
{
    if (!kFixNoxusBladePersistenceAvailable)
        return 0;
    if (romGroupIndex >= sizeof(kRomHooks) / sizeof(kRomHooks[0]) || maxCount == 0)
        return 0;

    const RomDeathCleanupHooks& rh = kRomHooks[romGroupIndex];
    uint32_t count = 0;
    for (std::size_t i = 0; i < rh.Count && count < maxCount; ++i)
        out[count++] = rh.Hooks[i].Address;
    return count;
}

void FixNoxusBladePersistence_SetState(Config::Table* cfg, uint8_t romGroupIndex)
{
    s_cfg           = cfg;

    if (romGroupIndex < sizeof(kRomHooks) / sizeof(kRomHooks[0]))
    {
        s_activeHooks = kRomHooks[romGroupIndex].Hooks;
        s_activeHookCount = kRomHooks[romGroupIndex].Count;
    }
    else
    {
        s_activeHooks = nullptr;
        s_activeHookCount = 0;
    }

    const uint32_t gen = s_configGen.load(std::memory_order_acquire);
    s_enabledCached = kFixNoxusBladePersistenceAvailable
        && cfg
        && cfg->GetBool("Metroid.BugFix.FixNoxusBladePersistence");
    s_configGenSeen = gen;
}

void FixNoxusBladePersistence_ClearState()
{
    s_cfg           = nullptr;
    s_activeHooks = nullptr;
    s_activeHookCount = 0;
    s_enabledCached = false;
}

void FixNoxusBladePersistence_DispatchCheck(
    melonDS::NDS* nds,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16])
{
    if (!s_cfg)
        return;

    const uint32_t gen = s_configGen.load(std::memory_order_acquire);
    if (s_configGenSeen != gen)
    {
        s_enabledCached = kFixNoxusBladePersistenceAvailable
            && s_cfg->GetBool("Metroid.BugFix.FixNoxusBladePersistence");
        s_configGenSeen = gen;
    }
    if (!s_enabledCached)
        return;

    ApplyHookInternal(
        nds,
        arm9ExecAddr,
        FindHook(s_activeHooks, s_activeHookCount, arm9ExecAddr),
        regs);
}

void FixNoxusBladePersistence_ResetPatchState()
{
    s_cfg           = nullptr;
    s_activeHooks = nullptr;
    s_activeHookCount = 0;
    s_configGenSeen = 0;
    s_enabledCached = false;
}

void FixNoxusBladePersistence_NotifyConfigChanged()
{
    s_configGen.fetch_add(1, std::memory_order_release);
}

} // namespace MelonPrime

#endif // MELONPRIME_DS
