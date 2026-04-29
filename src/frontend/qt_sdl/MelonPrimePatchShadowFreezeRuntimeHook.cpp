#ifdef MELONPRIME_DS

#include "MelonPrimePatchShadowFreezeRuntimeHook.h"
#include "Config.h"
#include "NDS.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace MelonPrime {

namespace {

// ROM group order:
//   JP1_0=0, JP1_1=1, US1_0=2, US1_1=3, EU1_0=4, EU1_1=5, KR1_0=6
//
// Cave-free / ARM9-write-free runtime hook.
// The hook is placed at the conditional branch after the original dot/cos compare.
// At that point, the original lateral-distance range check has already passed.
// We skip the original branch and choose hit/miss in C++ using the full 3D vector.
//
// Important KR1_0 difference:
//   JP/US/EU decision sites keep target player in r6, beam in r7, threshold in r1.
//   KR1_0 decision sites keep target player in r5, beam in r6, threshold in r10.

enum class IceWaveTargetKind : uint8_t
{
    Body,
    Halfturret,
};

struct IceWaveDecisionHook
{
    uint32_t DecisionAddress;
    uint32_t HitAddress;
    uint32_t MissAddress;
    IceWaveTargetKind Kind;
    uint8_t PlayerReg;
    uint8_t BeamReg;
    uint8_t ThresholdReg;
};

struct IceWaveRomHooks
{
    const IceWaveDecisionHook* Hooks;
    std::size_t Count;
};

static constexpr IceWaveDecisionHook kHooks_JP1_0[] = {
    {0x0203E5FCu, 0x0203E600u, 0x0203E62Cu, IceWaveTargetKind::Body,       6, 7, 1},
    {0x0203E734u, 0x0203E738u, 0x0203E764u, IceWaveTargetKind::Halfturret, 6, 7, 1},
};

static constexpr IceWaveDecisionHook kHooks_JP1_1[] = {
    {0x0203E5FCu, 0x0203E600u, 0x0203E62Cu, IceWaveTargetKind::Body,       6, 7, 1},
    {0x0203E734u, 0x0203E738u, 0x0203E764u, IceWaveTargetKind::Halfturret, 6, 7, 1},
};

static constexpr IceWaveDecisionHook kHooks_US1_0[] = {
    {0x0203E4B4u, 0x0203E4B8u, 0x0203E4E4u, IceWaveTargetKind::Body,       6, 7, 1},
    {0x0203E5ECu, 0x0203E5F0u, 0x0203E61Cu, IceWaveTargetKind::Halfturret, 6, 7, 1},
};

static constexpr IceWaveDecisionHook kHooks_US1_1[] = {
    {0x0203E3E4u, 0x0203E3E8u, 0x0203E414u, IceWaveTargetKind::Body,       6, 7, 1},
    {0x0203E51Cu, 0x0203E520u, 0x0203E54Cu, IceWaveTargetKind::Halfturret, 6, 7, 1},
};

static constexpr IceWaveDecisionHook kHooks_EU1_0[] = {
    {0x0203E3DCu, 0x0203E3E0u, 0x0203E40Cu, IceWaveTargetKind::Body,       6, 7, 1},
    {0x0203E514u, 0x0203E518u, 0x0203E544u, IceWaveTargetKind::Halfturret, 6, 7, 1},
};

static constexpr IceWaveDecisionHook kHooks_EU1_1[] = {
    {0x0203E3E4u, 0x0203E3E8u, 0x0203E414u, IceWaveTargetKind::Body,       6, 7, 1},
    {0x0203E51Cu, 0x0203E520u, 0x0203E54Cu, IceWaveTargetKind::Halfturret, 6, 7, 1},
};

static constexpr IceWaveDecisionHook kHooks_KR1_0[] = {
    // 0203D94C cmp r0,r10 / 0203D950 ble 0203D980
    {0x0203D950u, 0x0203D954u, 0x0203D980u, IceWaveTargetKind::Body,       5, 6, 10},
    // 0203DA74 cmp r0,r10 / 0203DA78 ble 0203DAA8
    {0x0203DA78u, 0x0203DA7Cu, 0x0203DAA8u, IceWaveTargetKind::Halfturret, 5, 6, 10},
};

static constexpr IceWaveRomHooks kRomHooks[] = {
    {kHooks_JP1_0, sizeof(kHooks_JP1_0) / sizeof(kHooks_JP1_0[0])},
    {kHooks_JP1_1, sizeof(kHooks_JP1_1) / sizeof(kHooks_JP1_1[0])},
    {kHooks_US1_0, sizeof(kHooks_US1_0) / sizeof(kHooks_US1_0[0])},
    {kHooks_US1_1, sizeof(kHooks_US1_1) / sizeof(kHooks_US1_1[0])},
    {kHooks_EU1_0, sizeof(kHooks_EU1_0) / sizeof(kHooks_EU1_0[0])},
    {kHooks_EU1_1, sizeof(kHooks_EU1_1) / sizeof(kHooks_EU1_1[0])},
    {kHooks_KR1_0, sizeof(kHooks_KR1_0) / sizeof(kHooks_KR1_0[0])},
};

static bool IsMainRamAddress(uint32_t address)
{
    return address >= 0x02000000u && address <= 0x023FFFFFu;
}

static bool IsMainRamRange(uint32_t address, uint32_t size)
{
    return size != 0
        && address >= 0x02000000u
        && address <= 0x023FFFFFu
        && size - 1u <= 0x023FFFFFu - address;
}

// Direct MainRAM read — callers must ensure IsMainRamRange(address, 4) is true.
static uint32_t ReadMainRam32(melonDS::NDS* nds, uint32_t address)
{
    uint32_t v;
    std::memcpy(&v, nds->MainRAM + (address & 0x3FFFFFu), sizeof(v));
    return v;
}

static int32_t ReadMainRamS32(melonDS::NDS* nds, uint32_t address)
{
    return static_cast<int32_t>(ReadMainRam32(nds, address));
}

static uint64_t AbsI64ToU64(int64_t value)
{
    return value < 0 ? static_cast<uint64_t>(-value) : static_cast<uint64_t>(value);
}

static bool AddSquare(uint64_t& sum, int64_t value)
{
    const uint64_t absValue = AbsI64ToU64(value);
    if (absValue != 0 && absValue > std::numeric_limits<uint64_t>::max() / absValue)
        return false;

    const uint64_t square = absValue * absValue;
    if (sum > std::numeric_limits<uint64_t>::max() - square)
        return false;

    sum += square;
    return true;
}

static uint64_t IsqrtU64(uint64_t value)
{
    uint64_t result = 0;
    uint64_t bit = 1ull << 62;

    while (bit > value)
        bit >>= 2;

    while (bit != 0)
    {
        if (value >= result + bit)
        {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }

        bit >>= 2;
    }

    return result;
}

static int32_t ClampToS32(int64_t value)
{
    if (value > static_cast<int64_t>(std::numeric_limits<int32_t>::max()))
        return std::numeric_limits<int32_t>::max();
    if (value < static_cast<int64_t>(std::numeric_limits<int32_t>::min()))
        return std::numeric_limits<int32_t>::min();
    return static_cast<int32_t>(value);
}

static bool GetTargetPositionAddress(melonDS::NDS* nds, uint32_t player, IceWaveTargetKind kind, uint32_t& outPositionAddress)
{
    if (!IsMainRamRange(player, 0xF28u))
        return false;

    if (kind == IceWaveTargetKind::Body)
    {
        outPositionAddress = player + 0x1Cu;
        return IsMainRamRange(outPositionAddress, 12u);
    }

    const uint32_t halfturret = ReadMainRam32(nds, player + 0xF24u);
    if (!IsMainRamRange(halfturret, 0x3Cu))
        return false;

    outPositionAddress = halfturret + 0x30u;
    return IsMainRamRange(outPositionAddress, 12u);
}

static bool ComputeFull3DIceWaveDotQ12(
    melonDS::NDS* nds,
    uint32_t beam,
    uint32_t targetPositionAddress,
    int32_t& outDotQ12)
{
    if (!IsMainRamRange(beam, 0xACu))
        return false;

    const uint32_t beamDirectionAddress = beam + 0x70u;
    const uint32_t beamPositionAddress = beam + 0xA0u;

    const int32_t targetX = ReadMainRamS32(nds, targetPositionAddress + 0u);
    const int32_t targetY = ReadMainRamS32(nds, targetPositionAddress + 4u);
    const int32_t targetZ = ReadMainRamS32(nds, targetPositionAddress + 8u);

    const int32_t beamX = ReadMainRamS32(nds, beamPositionAddress + 0u);
    const int32_t beamY = ReadMainRamS32(nds, beamPositionAddress + 4u);
    const int32_t beamZ = ReadMainRamS32(nds, beamPositionAddress + 8u);

    const int32_t dirX = ReadMainRamS32(nds, beamDirectionAddress + 0u);
    const int32_t dirY = ReadMainRamS32(nds, beamDirectionAddress + 4u);
    const int32_t dirZ = ReadMainRamS32(nds, beamDirectionAddress + 8u);

    const int64_t dx = static_cast<int64_t>(targetX) - static_cast<int64_t>(beamX);
    const int64_t dy = static_cast<int64_t>(targetY) - static_cast<int64_t>(beamY);
    const int64_t dz = static_cast<int64_t>(targetZ) - static_cast<int64_t>(beamZ);

    uint64_t lenSq = 0;
    if (!AddSquare(lenSq, dx) || !AddSquare(lenSq, dy) || !AddSquare(lenSq, dz))
        return false;
    if (lenSq == 0)
        return false;

    const uint64_t len = IsqrtU64(lenSq);
    if (len == 0 || len > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        return false;

    // dx/dy/dz are fx32/Q12 and beam direction is normalized Q12.
    // (delta * dir) is Q24; dividing by len(Q12) yields Q12.
    const int64_t dotNumerator =
        dx * static_cast<int64_t>(dirX) +
        dy * static_cast<int64_t>(dirY) +
        dz * static_cast<int64_t>(dirZ);

    outDotQ12 = ClampToS32(dotNumerator / static_cast<int64_t>(len));
    return true;
}

static const IceWaveDecisionHook* FindHook(uint8_t romGroupIndex, uint32_t arm9ExecAddr)
{
    if (romGroupIndex >= sizeof(kRomHooks) / sizeof(kRomHooks[0]))
        return nullptr;

    const IceWaveRomHooks& hooks = kRomHooks[romGroupIndex];
    for (std::size_t i = 0; i < hooks.Count; ++i)
    {
        if (hooks.Hooks[i].DecisionAddress == arm9ExecAddr)
            return &hooks.Hooks[i];
    }

    return nullptr;
}

// File-static hook registration state (no context struct needed).
static Config::Table* s_cfg = nullptr;
static uint8_t s_romGroupIndex = 0xFFu;

// Config generation cache: avoids a GetBool map lookup on every hook invocation.
// s_configGen is written by the GUI thread (NotifyConfigChanged) and read by the
// emu thread (HookCallback).  Atomic acquire/release is sufficient.
static std::atomic<uint32_t> s_configGen{1};
static uint32_t s_configGenSeen = 0;  // emu thread only
static bool s_enabledCached = false;  // emu thread only

// Core hook logic shared by both the callback and the public direct entry point.
static bool ApplyHook(
    melonDS::NDS* nds,
    uint8_t romGroupIndex,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr)
{
    const IceWaveDecisionHook* hook = FindHook(romGroupIndex, arm9ExecAddr);
    if (!hook)
        return false;

    const uint32_t player      = regs[hook->PlayerReg];
    const uint32_t beam        = regs[hook->BeamReg];
    const int32_t thresholdQ12 = static_cast<int32_t>(regs[hook->ThresholdReg]);

    if (!IsMainRamAddress(player) || !IsMainRamAddress(beam))
        return false;

    uint32_t targetPositionAddress = 0;
    if (!GetTargetPositionAddress(nds, player, hook->Kind, targetPositionAddress))
        return false;

    int32_t fullDotQ12 = 0;
    if (!ComputeFull3DIceWaveDotQ12(nds, beam, targetPositionAddress, fullDotQ12))
        return false;

    // Original: cmp dot, threshold / ble miss / fallthrough hit
    redirectExecAddr = (fullDotQ12 > thresholdQ12) ? hook->HitAddress : hook->MissAddress;
    return true;
}

static bool HookCallback(
    melonDS::NDS* nds,
    void* /*userdata*/,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr)
{
    redirectExecAddr = 0;
    if (!s_cfg)
        return false;

    // Refresh enabled cache when the config generation changes (settings saved).
    const uint32_t gen = s_configGen.load(std::memory_order_acquire);
    if (s_configGenSeen != gen)
    {
        s_enabledCached = s_cfg->GetBool("Metroid.BugFix.FixShadowFreeze");
        s_configGenSeen = gen;
    }
    if (!s_enabledCached)
        return false;

    return ApplyHook(nds, s_romGroupIndex, arm9ExecAddr, regs, redirectExecAddr);
}

} // anonymous namespace

void ShadowFreezeRuntimeHook_Install(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex)
{
    if (!nds || romGroupIndex >= sizeof(kRomHooks) / sizeof(kRomHooks[0]))
    {
        s_cfg = nullptr;
        s_romGroupIndex = 0xFFu;
        if (nds)
            nds->ClearARM9InstructionHook();
        return;
    }

    s_cfg = &cfg;
    s_romGroupIndex = romGroupIndex;
    s_configGenSeen = 0;  // force cache refresh on first invocation

    const IceWaveRomHooks& romHooks = kRomHooks[romGroupIndex];
    const uint32_t count = static_cast<uint32_t>(
        std::min(romHooks.Count,
                 static_cast<std::size_t>(melonDS::NDS::ARM9InstructionHookMaxAddresses)));

    uint32_t addresses[melonDS::NDS::ARM9InstructionHookMaxAddresses] = {};
    for (uint32_t i = 0; i < count; ++i)
        addresses[i] = romHooks.Hooks[i].DecisionAddress;

    nds->SetARM9InstructionHook(HookCallback, nullptr, addresses, count);
}

void ShadowFreezeRuntimeHook_Uninstall(melonDS::NDS* nds)
{
    s_cfg = nullptr;
    s_romGroupIndex = 0xFFu;
    if (nds)
        nds->ClearARM9InstructionHook();
}

bool ShadowFreezeRuntimeHook_CheckAndRedirect(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex,
    uint32_t arm9ExecAddr,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr)
{
    redirectExecAddr = 0;

    if (!nds || !regs)
        return false;

    if (!cfg.GetBool("Metroid.BugFix.FixShadowFreeze"))
        return false;

    return ApplyHook(nds, romGroupIndex, arm9ExecAddr, regs, redirectExecAddr);
}

bool ShadowFreezeRuntimeHook_CheckAndRedirectFromPipelinedR15(
    melonDS::NDS* nds,
    Config::Table& cfg,
    uint8_t romGroupIndex,
    const uint32_t regs[16],
    uint32_t& redirectExecAddr)
{
    if (!regs)
        return false;

    const uint32_t arm9ExecAddr = regs[15] - 8u;
    return ShadowFreezeRuntimeHook_CheckAndRedirect(
        nds,
        cfg,
        romGroupIndex,
        arm9ExecAddr,
        regs,
        redirectExecAddr);
}

void ShadowFreezeRuntimeHook_ResetPatchState()
{
    s_cfg = nullptr;
    s_romGroupIndex = 0xFFu;
    s_configGenSeen = 0;
    s_enabledCached = false;
}

void ShadowFreezeRuntimeHook_NotifyConfigChanged()
{
    s_configGen.fetch_add(1, std::memory_order_release);
}

} // namespace MelonPrime

#endif // MELONPRIME_DS
