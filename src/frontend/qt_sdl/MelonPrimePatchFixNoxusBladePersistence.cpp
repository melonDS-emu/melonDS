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

static constexpr uint32_t kOffHunterId          = 0x400u;
static constexpr uint32_t kOffStateFlags2       = 0x4C8u;
static constexpr uint32_t kOffAltAttackTimer    = 0x704u;
static constexpr uint32_t kAltFormAttackBit     = 0x8u;   // player+0x4C8 bit3
static constexpr uint8_t  kHunterNoxus          = 4u;

static bool IsMainRamRange(uint32_t address, uint32_t size)
{
    return size != 0
        && address >= 0x02000000u
        && size - 1u <= 0x023FFFFFu - address;
}

static bool ApplyHookInternal(
    melonDS::NDS* nds,
    uint32_t arm9ExecAddr,
    uint8_t romGroupIndex,
    const uint32_t regs[16])
{
    if (romGroupIndex >= sizeof(kRomHooks) / sizeof(kRomHooks[0]))
        return false;

    const RomDeathCleanupHooks& rh = kRomHooks[romGroupIndex];
    const DeathCleanupHook* hook = nullptr;
    for (std::size_t i = 0; i < rh.Count; ++i)
    {
        if (rh.Hooks[i].Address == arm9ExecAddr)
        {
            hook = &rh.Hooks[i];
            break;
        }
    }
    if (!hook)
        return false;

    const uint32_t player = regs[hook->PlayerReg];
    if (!IsMainRamRange(player, kOffAltAttackTimer + sizeof(uint16_t)))
        return false;

    // Noxus only (hunterId == 4).
    if (nds->MainRAM[(player + kOffHunterId) & 0x3FFFFFu] != kHunterNoxus)
        return false;

    uint16_t zero = 0;
    std::memcpy(nds->MainRAM + ((player + kOffAltAttackTimer) & 0x3FFFFFu), &zero, sizeof(zero));
    return true;
}

// File-static hook state — set/cleared by the shared dispatcher.
static Config::Table* s_cfg            = nullptr;
static uint8_t        s_romGroupIndex  = 0xFFu;

// Config generation cache: avoids GetBool map lookup on every hook invocation.
static std::atomic<uint32_t> s_configGen{1};
static uint32_t s_configGenSeen  = 0;   // emu thread only
static bool     s_enabledCached  = false; // emu thread only

} // anonymous namespace

uint32_t FixNoxusBladePersistence_GetAddresses(
    uint8_t romGroupIndex, uint32_t* out, uint32_t maxCount)
{
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
    s_romGroupIndex = romGroupIndex;
    s_configGenSeen = 0; // force cache refresh on first invocation
}

void FixNoxusBladePersistence_ClearState()
{
    s_cfg           = nullptr;
    s_romGroupIndex = 0xFFu;
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
        s_enabledCached = s_cfg->GetBool("Metroid.BugFix.FixNoxusBladePersistence");
        s_configGenSeen = gen;
    }
    if (!s_enabledCached)
        return;

    ApplyHookInternal(nds, arm9ExecAddr, s_romGroupIndex, regs);
}

void FixNoxusBladePersistence_ResetPatchState()
{
    s_cfg           = nullptr;
    s_romGroupIndex = 0xFFu;
    s_configGenSeen = 0;
    s_enabledCached = false;
}

void FixNoxusBladePersistence_NotifyConfigChanged()
{
    s_configGen.fetch_add(1, std::memory_order_release);
}

} // namespace MelonPrime

#endif // MELONPRIME_DS
